#!/usr/bin/env python3

import argparse
import bz2
import collections
import hashlib
import io
import json
import pathlib
import subprocess
import tarfile

import chess
import chess.pgn


RESULTS = {"1-0": 1, "1/2-1/2": 0, "0-1": -1}
SPLITS = ("train", "validation", "heldout")


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_fen(board):
    return " ".join(board.fen().split()[:4])


def split_for(group, config):
    value = int.from_bytes(
        hashlib.sha256(f"{config['seed']}:{group}".encode()).digest()[:8], "big"
    ) % 100
    train = config["splits"]["train"]
    validation = train + config["splits"]["validation"]
    if value < train:
        return "train"
    if value < validation:
        return "validation"
    return "heldout"


def game_group_key(path_name, member_name, game_index, round_name, start_fen):
    if not round_name or round_name == "?":
        round_name = f"game-{game_index}"
    return f"{path_name}:{member_name}:{round_name}:{start_fen}"


def load_config(path):
    config = json.loads(path.read_text())
    required = {
        "schema_version",
        "seed",
        "sample_every_plies",
        "minimum_game_ply",
        "endgame_phase_max",
        "balance",
        "quiet_position_policy",
        "splits",
    }
    if set(config) != required:
        raise ValueError(f"config keys must be {sorted(required)}")
    if config["schema_version"] != 1:
        raise ValueError("unsupported schema version")
    if config["sample_every_plies"] < 1 or config["minimum_game_ply"] < 0:
        raise ValueError("invalid sampling interval")
    if config["balance"] != "none":
        raise ValueError("only unbalanced WDL sampling is supported")
    if config["quiet_position_policy"] != "not_in_check_no_legal_capture_or_promotion":
        raise ValueError("unsupported quiet-position policy")
    if set(config["splits"]) != set(SPLITS) or sum(config["splits"].values()) != 100:
        raise ValueError("split percentages must define train, validation, and heldout totaling 100")
    return config


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


def has_legal_capture_or_promotion(board):
    return any(board.is_capture(move) or move.promotion for move in board.legal_moves)


def collect_positions(paths, config, output_dir):
    counts = collections.Counter()
    results = collections.Counter()
    seen = {}
    inputs_dir = output_dir / "inputs"
    inputs_dir.mkdir()
    streams = {
        split: (inputs_dir / f"{split}.tsv").open("w", encoding="utf-8")
        for split in SPLITS
    }

    try:
        for path in paths:
            for member_name, stream in pgn_streams(path):
                game_index = 0
                while True:
                    try:
                        game = chess.pgn.read_game(stream)
                    except Exception:
                        counts["malformed_games"] += 1
                        break
                    if game is None:
                        break

                    game_index += 1
                    counts["games"] += 1
                    result_text = game.headers.get("Result", "*")
                    if game.errors or result_text not in RESULTS:
                        counts["malformed_games"] += 1
                        continue

                    result = RESULTS[result_text]
                    results[f"games.{result_text}"] += 1
                    start_fen = game.headers.get("FEN", chess.STARTING_FEN)
                    group_key = game_group_key(
                        path.name,
                        member_name,
                        game_index,
                        game.headers.get("Round"),
                        start_fen,
                    )
                    group_id = hashlib.sha256(group_key.encode()).hexdigest()[:16]
                    game_id = hashlib.sha256(
                        f"{member_name}:{game_index}".encode()
                    ).hexdigest()[:8]
                    split = split_for(group_key, config)
                    board = game.board()

                    for game_ply, move in enumerate(game.mainline_moves(), 1):
                        board.push(move)
                        if game_ply < config["minimum_game_ply"]:
                            counts["positions.before_minimum_ply"] += 1
                            continue
                        if game_ply % config["sample_every_plies"]:
                            counts["positions.unsampled"] += 1
                            continue

                        counts["positions.sampled"] += 1
                        if board.is_insufficient_material():
                            counts["positions.dead"] += 1
                            continue
                        if board.is_game_over(claim_draw=True):
                            counts["positions.terminal"] += 1
                            continue
                        if board.is_check():
                            counts["positions.in_check"] += 1
                            continue
                        if has_legal_capture_or_promotion(board):
                            counts["positions.tactical"] += 1
                            continue

                        fen_key = canonical_fen(board)
                        if fen_key in seen:
                            counts["positions.duplicate"] += 1
                            if seen[fen_key] != result:
                                counts["positions.conflicting_duplicate_result"] += 1
                            continue
                        seen[fen_key] = result

                        source = f"{group_id}:{game_id}:{game_ply}"
                        streams[split].write(f"{source}\t{result}\t{board.fen()}\n")
                        counts[f"positions.{split}"] += 1
                        results[f"{split}.{result_text}"] += 1
    finally:
        for stream in streams.values():
            stream.close()

    return dict(sorted(counts.items())), dict(sorted(results.items()))


def run_exporter(engine, input_path, output_path):
    with input_path.open("rb") as input_stream, output_path.open("wb") as output_stream:
        result = subprocess.run(
            [str(engine), "features"],
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
    eg = eg * scale
    eg = abs(eg) // schema["scale_limit"] * (-1 if eg < 0 else 1)

    material_names = ("material.knight", "material.bishop", "material.rook", "material.queen")
    material_weights = {
        feature["name"]: feature["mg"] for feature in schema["features"]
    }
    material = sum(
        count * material_weights[name]
        for count, name in zip(record["phase_counts"], material_names)
    )
    material = min(
        schema["phase_material_max"], max(schema["phase_material_min"], material)
    )
    phase = (
        (material - schema["phase_material_min"]) * schema["phase_limit"]
        // (schema["phase_material_max"] - schema["phase_material_min"])
    )

    white_value = mg * phase + eg * (schema["phase_limit"] - phase)
    white_value = abs(white_value) // schema["phase_limit"] * (
        -1 if white_value < 0 else 1
    )
    side_value = white_value if record["turn"] == "w" else -white_value
    return side_value + schema["tempo"], phase


def validate_dataset(output_dir, endgame_phase_max=None):
    schema = None
    positions = set()
    sources = set()
    group_splits = {}
    counts = collections.Counter()
    results = collections.Counter()
    phase_buckets = collections.Counter()
    validation_endgames = []

    for split in SPLITS:
        path = output_dir / f"{split}.jsonl"
        with path.open(encoding="utf-8") as stream:
            current_schema = json.loads(stream.readline())
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
                if record.get("type") != "position":
                    raise ValueError(f"{path}:{line_number}: invalid record type")
                if record["version"] != schema["version"]:
                    raise ValueError(f"{path}:{line_number}: schema version mismatch")
                if record["source"] in sources:
                    raise ValueError(f"duplicate source: {record['source']}")
                sources.add(record["source"])

                fen_key = " ".join(record["fen"].split()[:4])
                if fen_key in positions:
                    raise ValueError(f"duplicate position: {fen_key}")
                positions.add(fen_key)

                group = record["source"].split(":", 1)[0]
                previous_split = group_splits.setdefault(group, split)
                if previous_split != split:
                    raise ValueError(f"game group leaked across splits: {group}")

                rebuilt, phase = reconstruct(schema, record)
                if rebuilt != record["eval"]:
                    raise ValueError(
                        f"{path}:{line_number}: evaluation {record['eval']} != {rebuilt}"
                    )

                counts[split] += 1
                results[f"{split}.{record['result']}"] += 1
                bucket = min(3, phase // 32)
                bucket_end = schema["phase_limit"] if bucket == 3 else bucket * 32 + 31
                phase_buckets[f"{split}.{bucket * 32}-{bucket_end}"] += 1
                if (
                    split == "validation"
                    and endgame_phase_max is not None
                    and phase <= endgame_phase_max
                ):
                    validation_endgames.append(record)

    return {
        "schema_version": schema["version"],
        "feature_count": len(schema["features"]),
        "positions": dict(sorted(counts.items())),
        "results": dict(sorted(results.items())),
        "phase_buckets": dict(sorted(phase_buckets.items())),
        "groups": len(group_splits),
        "duplicates": 0,
        "split_leaks": 0,
    }, schema, validation_endgames


def write_endgame_slice(path, schema, records):
    with path.open("w", encoding="utf-8") as stream:
        stream.write(json.dumps(schema, separators=(",", ":")) + "\n")
        for record in records:
            stream.write(json.dumps(record, separators=(",", ":")) + "\n")


def build(args):
    config_path = args.config.resolve()
    engine = args.engine.resolve()
    paths = sorted((path.resolve() for path in args.pgn), key=lambda path: path.name)
    output_dir = args.output.resolve()

    if output_dir.exists():
        raise ValueError(f"output already exists: {output_dir}")
    if not engine.is_file() or not paths or any(not path.is_file() for path in paths):
        raise ValueError("engine and PGN inputs must exist")

    config = load_config(config_path)
    output_dir.mkdir(parents=True)
    filters, source_results = collect_positions(paths, config, output_dir)

    for split in SPLITS:
        run_exporter(
            engine,
            output_dir / "inputs" / f"{split}.tsv",
            output_dir / f"{split}.jsonl",
        )

    report, schema, endgame_records = validate_dataset(
        output_dir, config["endgame_phase_max"]
    )
    if schema["version"] != config["schema_version"]:
        raise ValueError(
            f"engine schema version {schema['version']} != configured schema version "
            f"{config['schema_version']}"
        )
    write_endgame_slice(
        output_dir / "validation-endgame.jsonl", schema, endgame_records
    )
    report["endgame_validation_positions"] = len(endgame_records)

    manifest = {
        "tool_version": 1,
        "python_chess_version": chess.__version__,
        "config": config,
        "engine": {"name": engine.name, "sha256": sha256_file(engine)},
        "inputs": [
            {"name": path.name, "sha256": sha256_file(path)} for path in paths
        ],
        "filters": filters,
        "source_results": source_results,
        "validation": report,
    }
    manifest["outputs"] = {
        path.name: sha256_file(path)
        for path in sorted(output_dir.glob("*.jsonl"))
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    print(json.dumps(manifest, indent=2, sort_keys=True))


def validate(args):
    report, _, _ = validate_dataset(args.output.resolve())
    print(json.dumps(report, indent=2, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description="Build Latrunculi HCE tuning datasets")
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("--config", type=pathlib.Path, required=True)
    build_parser.add_argument("--engine", type=pathlib.Path, required=True)
    build_parser.add_argument("--output", type=pathlib.Path, required=True)
    build_parser.add_argument("pgn", type=pathlib.Path, nargs="+")
    build_parser.set_defaults(run=build)

    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("output", type=pathlib.Path)
    validate_parser.set_defaults(run=validate)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
