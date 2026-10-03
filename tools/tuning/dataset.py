import bz2
import collections
import hashlib
import io
import json
import pathlib
import platform
import shutil
import subprocess
import tarfile
import tempfile

import chess
import chess.pgn


RESULTS = {"1-0": 1, "1/2-1/2": 0, "0-1": -1}
DATA_FILE = "development.jsonl"
PHASE_BUCKETS = ((0, 31), (32, 63), (64, 95), (96, 128))
FORMAT_VERSION = 4
PREPARATION_POLICY = {
    "schema_version": 2,
    "minimum_game_ply": 8,
    "maximum_positions_per_game": 6,
    "minimum_games": 40000,
    "minimum_groups": 20000,
}
MATERIAL_NAMES = ("material.pawn", "material.knight", "material.bishop", "material.rook", "material.queen")


def validate_policy(policy):
    if not isinstance(policy, dict) or set(policy) != set(PREPARATION_POLICY):
        raise ValueError("invalid preparation policy")
    if any(type(value) is not int for value in policy.values()):
        raise ValueError("preparation policy values must be integers")
    if policy["schema_version"] != 2 or policy["maximum_positions_per_game"] <= 0:
        raise ValueError("unsupported preparation policy")
    if any(policy[name] < 0 for name in ("minimum_game_ply", "minimum_games", "minimum_groups")):
        raise ValueError("preparation limits must be nonnegative")


def validate_schema(schema):
    if (not isinstance(schema, dict) or schema.get("type") != "schema"
            or type(schema.get("version")) is not int or schema["version"] != 2):
        raise ValueError("unsupported dataset schema")
    features = schema.get("features")
    if not isinstance(features, list) or any(not isinstance(feature, dict) for feature in features):
        raise ValueError("invalid feature definitions")
    ids = [feature.get("id") for feature in features]
    names = [feature.get("name") for feature in features]
    if any(type(feature_id) is not int for feature_id in ids) or ids != list(range(len(ids))):
        raise ValueError("invalid feature IDs")
    if (any(not isinstance(name, str) or not name.strip() for name in names)
            or len(names) != len(set(names))):
        raise ValueError("invalid feature names")
    if any(type(feature.get(side)) is not int for feature in features for side in ("mg", "eg")):
        raise ValueError("feature weights must be integers")
    weights = {feature["name"]: feature for feature in features}
    if any(name not in weights for name in MATERIAL_NAMES):
        raise ValueError("missing material feature")
    if any(weights[name][side] <= 0 for name in MATERIAL_NAMES for side in ("mg", "eg")):
        raise ValueError("material weights must be positive")
    conventions = {
        "perspective": {"coefficients": "white", "fixed": "white", "eval": "side_to_move"},
        "result": "1=white_win,0=draw,-1=black_win",
        "phase_counts": ["knight", "bishop", "rook", "queen"],
        "pawn_counts": ["white", "black"],
    }
    if any(schema.get(name) != value for name, value in conventions.items()):
        raise ValueError("unsupported evaluation perspective or count conventions")
    constants = ("phase_limit", "phase_material_min", "phase_material_max",
                 "scale_limit", "scale_base", "scale_per_pawn", "tempo")
    if any(type(schema.get(name)) is not int for name in constants):
        raise ValueError("evaluation invariants must be integers")
    phase_max = sum(count * weights[name]["mg"]
                    for count, name in zip((4, 4, 4, 2), MATERIAL_NAMES[1:]))
    if (schema["phase_limit"] != 128
            or not 0 <= schema["phase_material_min"] < schema["phase_material_max"]
            or schema["phase_material_max"] != phase_max):
        raise ValueError("invalid evaluation phase invariants")
    if (schema["scale_limit"] <= 0 or not 0 <= schema["scale_base"] <= schema["scale_limit"]
            or schema["scale_per_pawn"] < 0):
        raise ValueError("invalid evaluation scaling invariants")
    return schema


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


def _preparation(engine, paths, policy):
    validate_policy(policy)
    engine = pathlib.Path(engine)
    paths = [pathlib.Path(path) for path in paths]
    if not engine.is_file() or not paths or any(not path.is_file() for path in paths):
        raise ValueError("engine and PGN inputs must exist")
    inputs = [(path, sha256_file(path)) for path in paths]
    if len({digest for _, digest in inputs}) != len(inputs):
        raise ValueError("duplicate PGN input")
    inputs.sort(key=lambda item: (item[1], item[0].name))
    return {
        "policy": dict(policy),
        "engine": {"name": engine.name, "sha256": sha256_file(engine)},
        "inputs": [{"name": path.name, "sha256": digest} for path, digest in inputs],
        "tool": {"name": pathlib.Path(__file__).name, "sha256": sha256_file(pathlib.Path(__file__))},
        "python_version": platform.python_version(),
        "python_chess_version": chess.__version__,
    }, inputs


def preparation_identity(engine, paths, policy=PREPARATION_POLICY):
    return _preparation(engine, paths, policy)[0]


def _valid_hash(value):
    return (isinstance(value, str) and len(value) == 64
            and all(character in "0123456789abcdef" for character in value))


def _validate_identity(identity):
    expected = {"policy", "engine", "inputs", "tool", "python_version", "python_chess_version"}
    if not isinstance(identity, dict) or set(identity) != expected:
        raise ValueError("invalid dataset preparation identity")
    validate_policy(identity["policy"])
    inputs = identity["inputs"]
    if not isinstance(inputs, list) or not inputs:
        raise ValueError("invalid dataset inputs")
    for entry in [identity["engine"], identity["tool"], *inputs]:
        if not isinstance(entry, dict) or set(entry) != {"name", "sha256"}:
            raise ValueError("invalid dataset provenance")
        name = entry["name"]
        if (not isinstance(name, str) or not name or pathlib.Path(name).name != name
                or name in (".", "..") or not _valid_hash(entry["sha256"])):
            raise ValueError("invalid dataset provenance")
    if identity["tool"]["name"] != "dataset.py":
        raise ValueError("invalid dataset preparation tool")
    if (inputs != sorted(inputs, key=lambda entry: (entry["sha256"], entry["name"]))
            or len({entry["sha256"] for entry in inputs}) != len(inputs)):
        raise ValueError("invalid or duplicate dataset inputs")
    if any(not isinstance(identity[name], str) or not identity[name]
           for name in ("python_version", "python_chess_version")):
        raise ValueError("invalid dataset preparation versions")


def canonical_fen(board):
    return " ".join(board.fen().split()[:4])


def group_from_source(source):
    return source.split(":", 1)[0]


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
    return validate_schema(schema)


def write_development(settled_path, output_dir):
    schema = read_schema(settled_path)
    selected = {}
    results_by_fen = collections.defaultdict(set)
    occurrences = collections.Counter()
    exported = 0

    with settled_path.open(encoding="utf-8") as stream:
        next(stream)
        for line_number, line in enumerate(stream, 2):
            record = json.loads(line)
            if not isinstance(record, dict) or record.get("type") != "position":
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

    with (output_dir / DATA_FILE).open("w", encoding="utf-8") as output:
        schema_line = canonical_json(schema) + "\n"
        output.write(schema_line)

        with settled_path.open(encoding="utf-8") as stream:
            next(stream)
            for line in stream:
                record = json.loads(line)
                fen = " ".join(record["fen"].split()[:4])
                if fen in conflicts or record["source"] != selected[fen]:
                    continue
                output.write(canonical_json(record) + "\n")
                counts["positions.retained"] += 1

    return dict(sorted(counts.items())), schema


def _validate_record(schema, record, policy):
    if (not isinstance(record, dict) or record.get("type") != "position"
            or type(record.get("version")) is not int or record["version"] != schema["version"]):
        raise ValueError("invalid position record")
    source = record.get("source")
    parts = source.split(":") if isinstance(source, str) else []
    if len(parts) != 3 or not all(parts) or not parts[2].isdecimal() or int(parts[2]) <= 0:
        raise ValueError("invalid position source")
    if policy is not None and int(parts[2]) < policy["minimum_game_ply"]:
        raise ValueError("position precedes the preparation minimum ply")
    if type(record.get("result")) is not int or record["result"] not in (-1, 0, 1):
        raise ValueError("invalid game result")
    fen = record.get("fen")
    if not isinstance(fen, str) or len(fen.split()) != 6:
        raise ValueError("position needs a six-field FEN")
    board = chess.Board(fen)
    if not board.is_valid() or record.get("turn") != ("w" if board.turn else "b"):
        raise ValueError("invalid position or side to move")
    for name, size in (("fixed", 2), ("phase_counts", 4), ("pawn_counts", 2)):
        values = record.get(name)
        if (not isinstance(values, list) or len(values) != size
                or any(type(value) is not int for value in values)):
            raise ValueError(f"invalid {name}")
    phase_counts = [sum(len(board.pieces(piece, color)) for color in chess.COLORS)
                    for piece in (chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN)]
    pawn_counts = [len(board.pieces(chess.PAWN, color)) for color in (chess.WHITE, chess.BLACK)]
    if record["phase_counts"] != phase_counts or record["pawn_counts"] != pawn_counts:
        raise ValueError("position material counts differ from its FEN")
    coefficients = record.get("coefficients")
    if (not isinstance(coefficients, list) or any(
            not isinstance(pair, list) or len(pair) != 2
            or any(type(value) is not int for value in pair)
            or not 0 <= pair[0] < len(schema["features"]) or pair[1] == 0
            for pair in coefficients)):
        raise ValueError("invalid feature coefficients")
    ids = [pair[0] for pair in coefficients]
    if ids != sorted(set(ids)):
        raise ValueError("duplicate or unordered coefficient IDs")
    if type(record.get("eval")) is not int:
        raise ValueError("invalid exported evaluation")
    return parts


def validate_dataset(output_dir, policy=None):
    path = output_dir / DATA_FILE
    positions = set()
    sources = set()
    groups = set()
    game_positions = collections.Counter()
    position_count = 0
    results = collections.Counter()
    phase_buckets = collections.Counter()
    with path.open(encoding="utf-8") as stream:
        line = stream.readline()
        if not line:
            raise ValueError(f"{path}: missing schema")
        schema = json.loads(line)
        validate_schema(schema)

        for line_number, line in enumerate(stream, 2):
            record = json.loads(line)
            try:
                parts = _validate_record(schema, record, policy)
            except ValueError as error:
                raise ValueError(f"{path}:{line_number}: {error}") from error
            if record["source"] in sources:
                raise ValueError(f"duplicate source: {record['source']}")
            sources.add(record["source"])

            fen = " ".join(record["fen"].split()[:4])
            if fen in positions:
                raise ValueError(f"duplicate position: {fen}")
            positions.add(fen)
            groups.add(group_from_source(record["source"]))
            game_positions[tuple(parts[:2])] += 1
            if policy is not None and game_positions[tuple(parts[:2])] > policy["maximum_positions_per_game"]:
                raise ValueError("too many retained positions from one game")

            rebuilt, phase = reconstruct(schema, record)
            if rebuilt != record["eval"]:
                raise ValueError(
                    f"{path}:{line_number}: evaluation {record['eval']} != {rebuilt}"
                )

            position_count += 1
            results[str(record["result"])] += 1
            bucket = min(3, phase // 32)
            start, end = PHASE_BUCKETS[bucket]
            phase_buckets[f"{start}-{end}"] += 1

    return {
        "schema_version": schema["version"],
        "feature_count": len(schema["features"]),
        "positions": position_count,
        "results": dict(sorted(results.items())),
        "phase_buckets": dict(sorted(phase_buckets.items())),
        "groups": len(groups),
        "duplicates": 0,
    }, schema


def _validate_statistics(manifest, report):
    for name in ("collection", "deduplication", "source_results"):
        counts = manifest.get(name)
        if (not isinstance(counts, dict) or any(not isinstance(key, str) for key in counts)
                or any(type(value) is not int or value < 0 for value in counts.values())):
            raise ValueError(f"invalid dataset {name}")
    collection = manifest["collection"]
    deduplication = manifest["deduplication"]
    source_results = manifest["source_results"]
    policy = manifest["identity"]["policy"]
    games = collection.get("games.valid", 0)
    groups = collection.get("groups.read", 0)
    sampled = collection.get("positions.sampled", 0)
    exported = deduplication.get("positions.exported", 0)
    retained = deduplication.get("positions.retained", 0)
    conflicts = deduplication.get("positions.conflicting", 0)
    conflicting_occurrences = deduplication.get("positions.conflicting_occurrences", 0)
    if (games < policy["minimum_games"] or groups < policy["minimum_groups"]
            or report["groups"] < policy["minimum_groups"]):
        raise ValueError("dataset does not meet preparation minimums")
    if (games > collection.get("games.read", 0) or not report["groups"] <= groups <= games
            or collection.get("groups.singleton", 0) > groups
            or sampled > games * policy["maximum_positions_per_game"]):
        raise ValueError("inconsistent dataset collection counts")
    if (sampled != exported + collection.get("positions.settling_rejected", 0)
            or retained != report["positions"]
            or exported != retained + conflicts + deduplication.get("positions.duplicate", 0)
            or not 2 * conflicts <= conflicting_occurrences <= exported - retained
            or (conflicts == 0 and conflicting_occurrences != 0)):
        raise ValueError("inconsistent dataset deduplication counts")
    allowed_results = {f"{prefix}.{result}" for prefix in ("games", "sampled") for result in RESULTS}
    if (not set(source_results) <= allowed_results
            or sum(source_results.get(f"games.{result}", 0) for result in RESULTS) != games
            or sum(source_results.get(f"sampled.{result}", 0) for result in RESULTS) != sampled):
        raise ValueError("inconsistent dataset source results")


def validate_output(output_dir):
    manifest = json.loads((output_dir / "manifest.json").read_text())
    if (not isinstance(manifest, dict) or type(manifest.get("format_version")) is not int
            or manifest["format_version"] != FORMAT_VERSION):
        raise ValueError("unsupported dataset manifest; use the original tool revision for legacy outputs or prepare a new dataset")
    expected_manifest = {"format_version", "identity", "collection", "deduplication",
                         "source_results", "validation", "outputs"}
    if set(manifest) != expected_manifest:
        raise ValueError("invalid dataset manifest fields")
    _validate_identity(manifest["identity"])
    expected_outputs = {DATA_FILE}
    if not isinstance(manifest["outputs"], dict) or set(manifest["outputs"]) != expected_outputs:
        raise ValueError("invalid dataset outputs")
    for name, expected_hash in manifest["outputs"].items():
        if not _valid_hash(expected_hash) or sha256_file(output_dir / name) != expected_hash:
            raise ValueError(f"dataset output hash mismatch: {name}")

    report, schema = validate_dataset(output_dir, manifest["identity"]["policy"])
    if report != manifest.get("validation"):
        raise ValueError("dataset validation report changed")
    _validate_statistics(manifest, report)
    return report, schema


def build_dataset(engine, paths, output_dir, policy=PREPARATION_POLICY):
    if output_dir.exists():
        raise ValueError(f"output already exists: {output_dir}")
    paths = list(paths)
    identity, inputs = _preparation(engine, paths, policy)
    output_dir.mkdir(parents=True)
    with tempfile.TemporaryDirectory(prefix="work-", dir=output_dir) as directory:
        work = pathlib.Path(directory)
        collection, source_results = collect_positions(
            inputs, policy, work / "positions.tsv"
        )
        export_settled_features(engine, work / "positions.tsv", work / "settled.jsonl")
        deduplication, schema = write_development(work / "settled.jsonl", output_dir)
    collection["positions.settling_rejected"] = (
        collection.get("positions.sampled", 0)
        - deduplication["positions.exported"]
    )

    report, validated_schema = validate_dataset(output_dir, policy)
    if validated_schema != schema:
        raise ValueError("exported schema changed while building the dataset")
    if report["groups"] < policy["minimum_groups"]:
        raise ValueError("too few opening groups remain after settling and deduplication")
    if preparation_identity(engine, paths, policy) != identity:
        raise ValueError("preparation inputs changed while building the dataset")

    manifest = {
        "format_version": FORMAT_VERSION,
        "identity": identity,
        "collection": collection,
        "deduplication": deduplication,
        "source_results": source_results,
        "validation": report,
    }
    _validate_statistics(manifest, report)
    manifest["outputs"] = {DATA_FILE: sha256_file(output_dir / DATA_FILE)}
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def atomic_build(engine, paths, output_dir, policy=PREPARATION_POLICY):
    if output_dir.exists():
        raise ValueError(f"output already exists: {output_dir}")
    partial = output_dir.with_name(output_dir.name + ".partial")
    if partial.exists():
        shutil.rmtree(partial)
    try:
        manifest = build_dataset(engine, paths, partial, policy)
        partial.rename(output_dir)
    except BaseException:
        if partial.exists():
            shutil.rmtree(partial)
        raise
    return manifest
