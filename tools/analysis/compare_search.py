#!/usr/bin/env python3
"""Validate comparable native-search runs and summarize nodes or paired time."""

import argparse
import csv
import math
import statistics
from dataclasses import dataclass
from pathlib import Path


FORMAT = "search_measurement_v5"
CONFIG = ("limit_type", "limit_value", "threads", "hash_mb")
SIGNATURE = ("completed_depth", "static_score", "score", "nodes", "best_move", "pv")
FIELDS = {"result_format", "case", "fen", *CONFIG,
          *SIGNATURE, "total_ns"}


@dataclass(frozen=True)
class Run:
    path: Path
    rows: dict[str, dict[str, str]]
    config: tuple[str, ...]

    @property
    def total_nodes(self) -> int:
        return sum(int(row["nodes"]) for row in self.rows.values())

    @property
    def total_ns(self) -> int:
        return sum(int(row["total_ns"]) for row in self.rows.values())


def fail(message: str) -> None:
    raise SystemExit(message)


def load_run(path: Path) -> Run:
    try:
        with path.open(encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            missing = FIELDS - set(reader.fieldnames or ())
            if missing:
                fail(f"{path}: missing fields: {','.join(sorted(missing))}")
            parsed = list(reader)
    except OSError as error:
        fail(f"{path}: {error}")
    if not parsed:
        fail(f"{path}: empty run")
    rows = {}
    config = None
    for line, row in enumerate(parsed, start=2):
        if None in row or any(row[field] is None for field in FIELDS):
            fail(f"{path}:{line}: malformed TSV row")
        case = row["case"]
        if not case or case in rows:
            fail(f"{path}:{line}: empty or duplicate case {case!r}")
        if row["result_format"] != FORMAT:
            fail(f"{path}:{line}: expected {FORMAT}")
        fields = row["fen"].split()
        if len(fields) != 6 or fields[1] not in ("w", "b"):
            fail(f"{path}:{line}: invalid six-field FEN")
        if row["limit_type"] not in ("depth", "nodes", "movetime"):
            fail(f"{path}:{line}: invalid limit type")
        try:
            for field in ("limit_value", "threads", "hash_mb", "completed_depth",
                          "nodes", "total_ns"):
                if int(row[field]) <= 0:
                    fail(f"{path}:{line}: {field} must be positive")
            int(row["static_score"])
            int(row["score"])
        except ValueError:
            fail(f"{path}:{line}: malformed numeric field")
        if not row["best_move"] or row["pv"].split()[:1] != [row["best_move"]]:
            fail(f"{path}:{line}: best move and PV disagree")
        row_config = tuple(row[field] for field in CONFIG)
        if config is None:
            config = row_config
        elif row_config != config:
            fail(f"{path}:{line}: mixed search settings")
        rows[case] = row
    assert config is not None
    return Run(path, rows, config)


def require_comparable(reference: Run, other: Run) -> None:
    if reference.config != other.config:
        fail(f"{other.path}: search settings differ from {reference.path}")
    if reference.rows.keys() != other.rows.keys():
        fail(f"{other.path}: case set differs from {reference.path}")
    changed = [case for case in reference.rows
               if reference.rows[case]["fen"] != other.rows[case]["fen"]]
    if changed:
        fail(f"{other.path}: position differs for {','.join(sorted(changed))}")


def require_signatures(label: str, reference: Run, other: Run) -> None:
    require_comparable(reference, other)
    changed = [case for case in reference.rows if any(
        reference.rows[case][field] != other.rows[case][field] for field in SIGNATURE)]
    if changed:
        fail(f"{label}: signature mismatches for {','.join(sorted(changed))}")


def require_legal_pvs(run: Run) -> int:
    """Validate every continuation independently from its full starting FEN."""
    try:
        import chess
    except ModuleNotFoundError:
        fail("PV legality requires tools/analysis/requirements.txt")

    moves = 0
    for case, row in run.rows.items():
        try:
            board = chess.Board(row["fen"])
            if not board.is_valid():
                raise ValueError("invalid position")
            for ply, token in enumerate(row["pv"].split(), start=1):
                move = chess.Move.from_uci(token)
                if move not in board.legal_moves:
                    raise ValueError(f"illegal PV move {token} at ply {ply}")
                board.push(move)
                moves += 1
        except ValueError as error:
            fail(f"{run.path}: {case}: {error}")
    return moves


def legality(args: argparse.Namespace) -> None:
    for path in args.runs:
        run = load_run(path)
        moves = require_legal_pvs(run)
        print(f"{path}: cases={len(run.rows)} legal_pv_moves={moves}")


def nodes(args: argparse.Namespace) -> None:
    paths = [path.resolve() for path in (args.baseline, args.candidate)
             if path is not None]
    if args.repeat:
        paths.append(args.repeat.resolve())
    if len(set(paths)) != len(paths):
        fail("node comparison requires distinct files for each run")
    baseline, candidate = load_run(args.baseline), load_run(args.candidate)
    require_comparable(baseline, candidate)
    if args.repeat:
        require_signatures("candidate repeat", candidate, load_run(args.repeat))
    if args.exact_tree:
        require_signatures("exact-tree comparison", baseline, candidate)
    ratios = [int(candidate.rows[case]["nodes"]) / int(row["nodes"])
              for case, row in baseline.rows.items()]
    print(f"cases={len(ratios)}")
    print(f"baseline_nodes={baseline.total_nodes}")
    print(f"candidate_nodes={candidate.total_nodes}")
    print(f"geometric_mean_node_ratio={math.exp(statistics.fmean(map(math.log, ratios))):.9f}")
    print(f"total_node_ratio={candidate.total_nodes / baseline.total_nodes:.9f}")
    if args.details:
        print(f"median_node_ratio={statistics.median(ratios):.9f}")
        for label, predicate in (("improved", lambda r: r < 1),
                                 ("equal", lambda r: r == 1),
                                 ("regressed", lambda r: r > 1)):
            print(f"{label}={sum(map(predicate, ratios))}")
        for field, label in (("best_move", "root_move_changes"),
                             ("score", "score_changes")):
            print(f"{label}={sum(baseline.rows[c][field] != candidate.rows[c][field] for c in baseline.rows)}")


def timing(args: argparse.Namespace) -> None:
    pairs = [(order, Path(b), Path(c)) for order, b, c in args.pair]
    if len(pairs) < 2 or len(pairs) % 2 or any(
        pairs[i][0] != ("BC" if i % 2 == 0 else "CB") for i in range(len(pairs))
    ):
        fail("timing pairs must alternate BC,CB in complete blocks")
    paths = [path.resolve() for _, b, c in pairs for path in (b, c)]
    if len(set(paths)) != len(paths):
        fail("timing pairs must use distinct files")
    baseline_ref = candidate_ref = None
    ratios = []
    for index, (order, bpath, cpath) in enumerate(pairs, start=1):
        baseline, candidate = load_run(bpath), load_run(cpath)
        require_comparable(baseline, candidate)
        if baseline_ref is None:
            baseline_ref, candidate_ref = baseline, candidate
        else:
            require_signatures(f"baseline repeat {index}", baseline_ref, baseline)
            require_signatures(f"candidate repeat {index}", candidate_ref, candidate)
        ratios.append(candidate.total_ns / baseline.total_ns)
        print(f"pair_{index}_order={order}")
        print(f"pair_{index}_search_time_ratio={ratios[-1]:.9f}")
    if args.exact_tree:
        require_signatures("exact-tree timing", baseline_ref, candidate_ref)
    blocks = [math.sqrt(ratios[i] * ratios[i + 1]) for i in range(0, len(ratios), 2)]
    print(f"pairs={len(ratios)}")
    print(f"median_search_time_ratio={statistics.median(ratios):.9f}")
    print(f"minimum_search_time_ratio={min(ratios):.9f}")
    print(f"maximum_search_time_ratio={max(ratios):.9f}")
    print(f"candidate_wins={sum(r < 1 for r in ratios)}")
    for order in ("BC", "CB"):
        selected = [ratio for pair, ratio in zip(pairs, ratios) if pair[0] == order]
        print(f"{order.lower()}_median_search_time_ratio={statistics.median(selected):.9f}")
    for index, ratio in enumerate(blocks, start=1):
        print(f"balanced_block_{index}_search_time_ratio={ratio:.9f}")
    print(f"median_balanced_search_time_ratio={statistics.median(blocks):.9f}")


def main(argv=None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    node_parser = commands.add_parser("nodes")
    node_parser.add_argument("baseline", type=Path)
    node_parser.add_argument("candidate", type=Path)
    node_parser.add_argument("--repeat", type=Path)
    node_parser.add_argument("--exact-tree", action="store_true")
    node_parser.add_argument("--details", action="store_true")
    time_parser = commands.add_parser("timing")
    time_parser.add_argument("--pair", nargs=3, action="append", required=True,
                             metavar=("ORDER", "BASELINE", "CANDIDATE"))
    time_parser.add_argument("--exact-tree", action="store_true")
    legal_parser = commands.add_parser("legality")
    legal_parser.add_argument("runs", nargs="+", type=Path)
    args = parser.parse_args(argv)
    {"nodes": nodes, "timing": timing, "legality": legality}[args.command](args)


if __name__ == "__main__":
    main()
