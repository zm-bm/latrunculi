import bz2
import collections
import hashlib
import io
import json
import pathlib
import shutil
import subprocess
import tarfile
import tempfile

import chess
import chess.pgn


RESULTS = {"1-0": 1, "1/2-1/2": 0, "0-1": -1}
SPLITS = ("train", "selection", "validation", "heldout")
PHASE_BUCKETS = ((0, 31), (32, 63), (64, 95), (96, 128))
FORMAT_VERSION = 1


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sha256_json(value):
    return hashlib.sha256(canonical_json(value).encode()).hexdigest()


def canonical_fen(board):
    return " ".join(board.fen().split()[:4])


def group_from_source(source):
    return source.split(":", 1)[0]


def split_for(group, config):
    value = int.from_bytes(
        hashlib.sha256(f"{config['seed']}:{group}".encode()).digest()[:8], "big"
    ) % 100
    boundary = 0
    for split in SPLITS:
        boundary += config["splits"][split]
        if value < boundary:
            return split
    raise AssertionError("unreachable split")


def game_group_key(input_hash, member_name, game_index, game):
    if "FEN" in game.headers:
        return f"fen:{canonical_fen(game.board())}"
    return f"game:{input_hash}:{member_name}:{game_index}"


def quantile_indices(size, limit):
    count = min(size, limit)
    return [((2 * index + 1) * size) // (2 * count) for index in range(count)]


def pgn_streams(path):
    if tarfile.is_tarfile(path):
        with tarfile.open(path) as archive:
            members = sorted(
                (
                    member
                    for member in archive.getmembers()
                    if member.isfile()
                    and (member.name.endswith(".pgn") or member.name.endswith(".pgn.bz2"))
                ),
                key=lambda member: member.name,
            )
            for member in members:
                binary = archive.extractfile(member)
                if binary is None:
                    continue
                if member.name.endswith(".bz2"):
                    binary = bz2.BZ2File(binary)
                with io.TextIOWrapper(binary, encoding="utf-8", errors="replace") as stream:
                    yield member.name, stream
        return

    if path.name.endswith(".pgn.bz2"):
        with bz2.open(path, "rt", encoding="utf-8", errors="replace") as stream:
            yield path.name, stream
        return

    with path.open(encoding="utf-8", errors="replace") as stream:
        yield path.name, stream


def collect_positions(inputs, config, input_path):
    counts = collections.Counter()
    results = collections.Counter()
    group_games = collections.Counter()
    with input_path.open("w", encoding="utf-8") as output:
        for path, input_hash in inputs:
            for member_name, stream in pgn_streams(path):
                game_index = 0
                while True:
                    try:
                        game = chess.pgn.read_game(stream)
                    except Exception:
                        counts["games.malformed"] += 1
                        break
                    if game is None:
                        break

                    game_index += 1
                    counts["games.read"] += 1
                    result_text = game.headers.get("Result", "*")
                    if game.errors or result_text not in RESULTS:
                        counts["games.malformed"] += 1
                        continue

                    result = RESULTS[result_text]
                    group_key = game_group_key(input_hash, member_name, game_index, game)
                    group_id = hashlib.sha256(group_key.encode()).hexdigest()[:16]
                    game_id = hashlib.sha256(
                        f"{input_hash}:{member_name}:{game_index}".encode()
                    ).hexdigest()[:16]
                    group_games[group_id] += 1
                    counts["games.valid"] += 1
                    results[f"games.{result_text}"] += 1

                    board = game.board()
                    eligible = []
                    for game_ply, move in enumerate(game.mainline_moves(), 1):
                        board.push(move)
                        if game_ply < config["minimum_game_ply"]:
                            counts["positions.before_minimum_ply"] += 1
                            continue
                        if board.is_insufficient_material():
                            counts["positions.dead"] += 1
                            continue
                        if board.is_game_over(claim_draw=True):
                            counts["positions.terminal"] += 1
                            continue
                        eligible.append((game_ply, board.fen()))

                    for index in quantile_indices(
                        len(eligible), config["maximum_positions_per_game"]
                    ):
                        game_ply, fen = eligible[index]
                        source = f"{group_id}:{game_id}:{game_ply}"
                        output.write(f"{source}\t{result}\t{fen}\n")
                        counts["positions.sampled"] += 1
                        results[f"sampled.{result_text}"] += 1

    counts["groups.read"] = len(group_games)
    counts["groups.singleton"] = sum(games == 1 for games in group_games.values())

    if counts["games.valid"] < config["minimum_games"]:
        raise ValueError(
            f"corpus has {counts['games.valid']} valid games; "
            f"requires {config['minimum_games']}"
        )
    if len(group_games) < config["minimum_groups"]:
        raise ValueError(
            f"corpus has {len(group_games)} groups; "
            f"requires {config['minimum_groups']}"
        )

    return dict(sorted(counts.items())), dict(sorted(results.items()))


def export_settled_features(engine, input_path, output_path):
    command = [str(engine), "features", "--settle"]
    with input_path.open("rb") as input_stream, output_path.open("wb") as output_stream:
        result = subprocess.run(
            command,
            stdin=input_stream,
            stdout=output_stream,
            stderr=subprocess.PIPE,
            check=False,
        )
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace").strip())


def reconstruct(schema, record):
    mg = record["fixed"][0]
    eg = record["fixed"][1]
    weights = schema["features"]
    for feature_id, coefficient in record["coefficients"]:
        mg += weights[feature_id]["mg"] * coefficient
        eg += weights[feature_id]["eg"] * coefficient

    stronger_pawns = record["pawn_counts"][1 if eg < 0 else 0]
    scale = min(
        schema["scale_limit"],
        schema["scale_base"] + schema["scale_per_pawn"] * stronger_pawns,
    )
    eg = abs(eg * scale) // schema["scale_limit"] * (-1 if eg < 0 else 1)

    material_names = ("material.knight", "material.bishop", "material.rook", "material.queen")
    material_weights = {feature["name"]: feature["mg"] for feature in schema["features"]}
    material = sum(
        count * material_weights[name]
        for count, name in zip(record["phase_counts"], material_names)
    )
    material = min(schema["phase_material_max"], max(schema["phase_material_min"], material))
    phase = (
        (material - schema["phase_material_min"]) * schema["phase_limit"]
        // (schema["phase_material_max"] - schema["phase_material_min"])
    )

    white_value = mg * phase + eg * (schema["phase_limit"] - phase)
    white_value = abs(white_value) // schema["phase_limit"] * (-1 if white_value < 0 else 1)
    side_value = white_value if record["turn"] == "w" else -white_value
    return side_value + schema["tempo"], phase


def read_schema(path):
    with path.open(encoding="utf-8") as stream:
        line = stream.readline()
    if not line:
        raise ValueError(f"{path}: missing schema")
    schema = json.loads(line)
    if schema.get("type") != "schema":
        raise ValueError(f"{path}: missing schema")
    return schema


def split_settled(settled_path, output_dir, config):
    if set(config["splits"]) != set(SPLITS) or sum(config["splits"].values()) != 100:
        raise ValueError("split percentages must define every split and total 100")
    schema = read_schema(settled_path)
    selected = {}
    results_by_fen = collections.defaultdict(set)
    occurrences = collections.Counter()
    exported = 0

    with settled_path.open(encoding="utf-8") as stream:
        next(stream)
        for line_number, line in enumerate(stream, 2):
            record = json.loads(line)
            if record.get("type") != "position":
                raise ValueError(f"{settled_path}:{line_number}: invalid record")
            exported += 1
            fen = " ".join(record["fen"].split()[:4])
            source = record["source"]
            occurrences[fen] += 1
            results_by_fen[fen].add(record["result"])
            selected[fen] = min(source, selected.get(fen, source))

    counts = collections.Counter()
    counts["positions.exported"] = exported
    counts["positions.duplicate"] = sum(count - 1 for count in occurrences.values())
    conflicts = {fen for fen, results in results_by_fen.items() if len(results) > 1}
    counts["positions.conflicting"] = len(conflicts)
    counts["positions.conflicting_occurrences"] = sum(occurrences[fen] for fen in conflicts)

    streams = {
        split: (output_dir / f"{split}.jsonl").open("w", encoding="utf-8")
        for split in SPLITS
    }
    try:
        schema_line = canonical_json(schema) + "\n"
        for output in streams.values():
            output.write(schema_line)

        with settled_path.open(encoding="utf-8") as stream:
            next(stream)
            for line in stream:
                record = json.loads(line)
                fen = " ".join(record["fen"].split()[:4])
                if fen in conflicts or record["source"] != selected[fen]:
                    continue
                split = split_for(group_from_source(record["source"]), config)
                streams[split].write(canonical_json(record) + "\n")
                counts[f"positions.{split}"] += 1
    finally:
        for output in streams.values():
            output.close()

    return dict(sorted(counts.items())), schema


def validate_dataset(output_dir):
    schema = None
    positions = set()
    sources = set()
    group_splits = {}
    groups_by_split = collections.defaultdict(set)
    counts = collections.Counter()
    results = collections.Counter()
    phase_buckets = collections.Counter()
    for split in SPLITS:
        path = output_dir / f"{split}.jsonl"
        with path.open(encoding="utf-8") as stream:
            line = stream.readline()
            if not line:
                raise ValueError(f"{path}: missing schema")
            current_schema = json.loads(line)
            if current_schema.get("type") != "schema":
                raise ValueError(f"{path}: missing schema")
            if schema is None:
                schema = current_schema
                ids = [feature["id"] for feature in schema["features"]]
                names = [feature["name"] for feature in schema["features"]]
                if ids != list(range(len(ids))) or len(names) != len(set(names)):
                    raise ValueError("invalid feature IDs or duplicate names")
            elif current_schema != schema:
                raise ValueError("schema differs across splits")

            for line_number, line in enumerate(stream, 2):
                record = json.loads(line)
                if record.get("type") != "position" or record["version"] != schema["version"]:
                    raise ValueError(f"{path}:{line_number}: invalid position record")
                if record["source"] in sources:
                    raise ValueError(f"duplicate source: {record['source']}")
                sources.add(record["source"])

                fen = " ".join(record["fen"].split()[:4])
                if fen in positions:
                    raise ValueError(f"duplicate position: {fen}")
                positions.add(fen)

                group = group_from_source(record["source"])
                previous_split = group_splits.setdefault(group, split)
                if previous_split != split:
                    raise ValueError(f"opening group leaked across splits: {group}")
                groups_by_split[split].add(group)

                rebuilt, phase = reconstruct(schema, record)
                if rebuilt != record["eval"]:
                    raise ValueError(
                        f"{path}:{line_number}: evaluation {record['eval']} != {rebuilt}"
                    )

                counts[split] += 1
                results[f"{split}.{record['result']}"] += 1
                bucket = min(3, phase // 32)
                start, end = PHASE_BUCKETS[bucket]
                phase_buckets[f"{split}.{start}-{end}"] += 1

    return {
        "schema_version": schema["version"],
        "feature_count": len(schema["features"]),
        "positions": dict(sorted(counts.items())),
        "results": dict(sorted(results.items())),
        "phase_buckets": dict(sorted(phase_buckets.items())),
        "groups": {split: len(groups_by_split[split]) for split in SPLITS},
        "duplicates": 0,
        "split_leaks": 0,
    }, schema


def validate_output(output_dir):
    manifest = json.loads((output_dir / "manifest.json").read_text())
    if manifest.get("format_version") != FORMAT_VERSION:
        raise ValueError("unsupported dataset manifest")
    experiment_hash = manifest.get("experiment_sha256")
    if not isinstance(experiment_hash, str) or len(experiment_hash) != 64:
        raise ValueError("invalid dataset experiment hash")
    expected_outputs = {f"{split}.jsonl" for split in SPLITS}
    if set(manifest.get("outputs", {})) != expected_outputs:
        raise ValueError("invalid dataset outputs")
    for name, expected_hash in manifest["outputs"].items():
        if sha256_file(output_dir / name) != expected_hash:
            raise ValueError(f"dataset output hash mismatch: {name}")

    report, schema = validate_dataset(output_dir)
    if report != manifest.get("validation"):
        raise ValueError("dataset validation report changed")
    return report, schema


def build_dataset(engine, paths, output_dir, experiment):
    if output_dir.exists():
        raise ValueError(f"output already exists: {output_dir}")
    if not engine.is_file() or not paths or any(not path.is_file() for path in paths):
        raise ValueError("engine and PGN inputs must exist")

    engine_hash = sha256_file(engine)
    hashes = {path: sha256_file(path) for path in paths}
    if len(set(hashes.values())) != len(paths):
        raise ValueError("duplicate PGN input")
    inputs = sorted(hashes.items(), key=lambda item: (item[1], item[0].name))
    output_dir.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="work-", dir=output_dir) as directory:
        work = pathlib.Path(directory)
        collection, source_results = collect_positions(
            inputs, experiment["dataset"], work / "positions.tsv"
        )
        export_settled_features(engine, work / "positions.tsv", work / "settled.jsonl")
        splitting, schema = split_settled(
            work / "settled.jsonl", output_dir, experiment["dataset"]
        )
    collection["positions.settling_rejected"] = (
        collection.get("positions.sampled", 0)
        - splitting["positions.exported"]
    )

    report, validated_schema = validate_dataset(output_dir)
    if validated_schema != schema:
        raise ValueError("exported schema changed while building the dataset")
    expected = experiment["dataset"]
    if schema["version"] != expected["schema_version"]:
        raise ValueError("engine schema version differs from the experiment")
    if len(schema["features"]) != expected["feature_count"]:
        raise ValueError("engine feature count differs from the experiment")
    if sum(report["groups"].values()) < expected["minimum_groups"]:
        raise ValueError("too few opening groups remain after settling and deduplication")
    if sha256_file(engine) != engine_hash:
        raise ValueError("engine changed while building the dataset")
    if any(sha256_file(path) != input_hash for path, input_hash in inputs):
        raise ValueError("PGN input changed while building the dataset")

    manifest = {
        "format_version": FORMAT_VERSION,
        "python_chess_version": chess.__version__,
        "experiment_sha256": sha256_json(experiment),
        "engine": {"name": engine.name, "sha256": engine_hash},
        "inputs": [
            {"name": path.name, "sha256": input_hash} for path, input_hash in inputs
        ],
        "collection": collection,
        "splitting": splitting,
        "source_results": source_results,
        "validation": report,
    }
    manifest["outputs"] = {
        f"{split}.jsonl": sha256_file(output_dir / f"{split}.jsonl") for split in SPLITS
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def atomic_build(engine, paths, output_dir, experiment):
    if output_dir.exists():
        raise ValueError(f"output already exists: {output_dir}")
    partial = output_dir.with_name(output_dir.name + ".partial")
    if partial.exists():
        shutil.rmtree(partial)
    try:
        manifest = build_dataset(engine, paths, partial, experiment)
        partial.rename(output_dir)
    except Exception:
        if partial.exists():
            shutil.rmtree(partial)
        raise
    return manifest
