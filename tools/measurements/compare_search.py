#!/usr/bin/env python3

import argparse
import csv
import hashlib
import math
import re
import statistics
from dataclasses import dataclass
from pathlib import Path


FORMAT = "search_measurement_v3"
SUITE_PATH = Path(__file__).with_name("search.epd")
PROFILE = {
    "repetition": "1",
    "repetitions": "1",
    "limit_type": "depth",
    "limit_value": "10",
    "completed_depth": "10",
    "threads": "1",
    "hash_mb": "32",
}
SIGNATURE_FIELDS = ("completed_depth", "score", "nodes", "best_move", "pv")
REQUIRED_FIELDS = {
    "result_format",
    "case",
    "score",
    "nodes",
    "total_ns",
    "best_move",
    "pv",
    *PROFILE,
}
EPD_ID = re.compile(r';\s*id\s+"([^"]+)"\s*;')


@dataclass(frozen=True)
class Run:
    path: Path
    rows: dict[str, dict[str, str]]

    @property
    def total_nodes(self) -> int:
        return sum(int(row["nodes"]) for row in self.rows.values())

    @property
    def total_ns(self) -> int:
        return sum(int(row["total_ns"]) for row in self.rows.values())


def fail(message: str) -> None:
    raise SystemExit(message)


def current_suite() -> tuple[set[str], str]:
    try:
        contents = SUITE_PATH.read_bytes()
    except OSError as error:
        fail(f"{SUITE_PATH}: {error}")
    cases = EPD_ID.findall(contents.decode("utf-8"))
    if len(cases) != 200 or len(set(cases)) != 200:
        fail(f"{SUITE_PATH}: expected 200 unique case IDs")
    return set(cases), hashlib.sha256(contents).hexdigest()


def load_run(path: Path, expected_cases: set[str]) -> Run:
    try:
        with path.open(encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            missing = REQUIRED_FIELDS - set(reader.fieldnames or ())
            if missing:
                fail(f"{path}: missing fields: {','.join(sorted(missing))}")
            parsed = list(reader)
    except OSError as error:
        fail(f"{path}: {error}")

    rows: dict[str, dict[str, str]] = {}
    for line, row in enumerate(parsed, start=2):
        case = row["case"]
        if not case:
            fail(f"{path}:{line}: empty case")
        if case in rows:
            fail(f"{path}:{line}: duplicate case {case}")
        if row["result_format"] != FORMAT:
            fail(f"{path}:{line}: expected {FORMAT}")
        mismatched = [field for field, value in PROFILE.items() if row[field] != value]
        if mismatched:
            fail(f"{path}:{line}: invalid strength profile: {','.join(mismatched)}")
        try:
            int(row["score"])
            nodes = int(row["nodes"])
            total_ns = int(row["total_ns"])
        except (TypeError, ValueError):
            fail(f"{path}:{line}: malformed score, nodes, or total_ns")
        if nodes <= 0 or total_ns <= 0:
            fail(f"{path}:{line}: nodes and total_ns must be positive")
        if not row["best_move"] or not row["pv"]:
            fail(f"{path}:{line}: best_move and pv must be nonempty")
        rows[case] = row

    cases = set(rows)
    if cases != expected_cases:
        missing = sorted(expected_cases - cases)
        extra = sorted(cases - expected_cases)
        fail(
            f"{path}: not the current 200-case strength profile; "
            f"missing={','.join(missing)} extra={','.join(extra)}"
        )
    return Run(path, rows)


def signature_mismatches(reference: Run, other: Run) -> list[str]:
    return [
        case
        for case in reference.rows
        if any(
            reference.rows[case][field] != other.rows[case][field]
            for field in SIGNATURE_FIELDS
        )
    ]


def require_matching_signatures(label: str, reference: Run, other: Run) -> None:
    mismatches = signature_mismatches(reference, other)
    if mismatches:
        fail(f"{label}: signature mismatches for {','.join(sorted(mismatches))}")


def geometric_mean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def print_node_summary(args: argparse.Namespace) -> None:
    expected_cases, suite_sha256 = current_suite()
    baseline = load_run(args.baseline, expected_cases)
    candidate = load_run(args.candidate, expected_cases)

    if args.repeat is not None:
        repeat = load_run(args.repeat, expected_cases)
        require_matching_signatures("invalid candidate repeat", candidate, repeat)
    if args.exact_tree:
        require_matching_signatures("invalid exact-tree comparison", baseline, candidate)

    ratios = {
        case: int(candidate.rows[case]["nodes"]) / int(baseline.rows[case]["nodes"])
        for case in baseline.rows
    }
    print(f"local_suite_sha256={suite_sha256}")
    print(f"cases={len(ratios)}")
    print(f"baseline_nodes={baseline.total_nodes}")
    print(f"candidate_nodes={candidate.total_nodes}")
    print(f"geometric_mean_node_ratio={geometric_mean(list(ratios.values())):.9f}")
    print(f"total_node_ratio={candidate.total_nodes / baseline.total_nodes:.9f}")

    if args.details:
        improved = sum(ratio < 1.0 for ratio in ratios.values())
        equal = sum(ratio == 1.0 for ratio in ratios.values())
        regressions = ((case, ratio) for case, ratio in ratios.items() if ratio > 1.0)
        worst = sorted(regressions, key=lambda item: (-item[1], item[0]))[:5]
        root_changes = sum(
            baseline.rows[case]["best_move"] != candidate.rows[case]["best_move"]
            for case in baseline.rows
        )
        score_changes = sum(
            baseline.rows[case]["score"] != candidate.rows[case]["score"]
            for case in baseline.rows
        )
        print(f"median_node_ratio={statistics.median(ratios.values()):.9f}")
        print(f"improved={improved}")
        print(f"equal={equal}")
        print(f"regressed={len(ratios) - improved - equal}")
        print(f"root_move_changes={root_changes}")
        print(f"score_changes={score_changes}")
        print(
            "largest_node_regressions="
            + ",".join(f"{case}:{ratio:.9f}" for case, ratio in worst)
        )


def print_timing_summary(args: argparse.Namespace) -> None:
    if len(args.pair) != 6:
        fail("timing panel requires exactly six pairs")
    orders = [order for order, _, _ in args.pair]
    if orders != ["BC", "CB", "BC", "CB", "BC", "CB"]:
        fail("declared timing order must be BC,CB,BC,CB,BC,CB")
    paths = [
        path.resolve()
        for _, baseline, candidate in args.pair
        for path in (baseline, candidate)
    ]
    if len(set(paths)) != 12:
        fail("timing panel requires twelve distinct paths to guard against accidental reuse")

    expected_cases, suite_sha256 = current_suite()
    pairs: list[tuple[str, float]] = []
    baseline_reference = None
    candidate_reference = None
    for index, (order, baseline_path, candidate_path) in enumerate(args.pair, start=1):
        baseline = load_run(baseline_path, expected_cases)
        candidate = load_run(candidate_path, expected_cases)
        if baseline_reference is None:
            baseline_reference = baseline
            candidate_reference = candidate
        else:
            require_matching_signatures(
                f"invalid baseline timing repeat {index}", baseline_reference, baseline
            )
            require_matching_signatures(
                f"invalid candidate timing repeat {index}", candidate_reference, candidate
            )
        pairs.append((order, candidate.total_ns / baseline.total_ns))

    assert baseline_reference is not None and candidate_reference is not None
    if args.exact_tree:
        require_matching_signatures(
            "invalid exact-tree timing panel", baseline_reference, candidate_reference
        )

    ratios = [ratio for _, ratio in pairs]
    print(f"local_suite_sha256={suite_sha256}")
    print(f"pairs={len(pairs)}")
    for index, (order, ratio) in enumerate(pairs, start=1):
        print(f"pair_{index}_order={order}")
        print(f"pair_{index}_search_time_ratio={ratio:.9f}")
    print(f"median_search_time_ratio={statistics.median(ratios):.9f}")
    print(f"minimum_search_time_ratio={min(ratios):.9f}")
    print(f"maximum_search_time_ratio={max(ratios):.9f}")
    print(f"candidate_wins={sum(ratio < 1.0 for ratio in ratios)}")
    for order in ("BC", "CB"):
        ordered = [ratio for pair_order, ratio in pairs if pair_order == order]
        print(f"{order.lower()}_median_search_time_ratio={statistics.median(ordered):.9f}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate and summarize Latrunculi search measurement TSV files."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    nodes = subparsers.add_parser("nodes", help="compare deterministic node evidence")
    nodes.add_argument("baseline", type=Path)
    nodes.add_argument("candidate", type=Path)
    nodes.add_argument("--repeat", type=Path, help="fresh candidate repeat")
    nodes.add_argument("--exact-tree", action="store_true")
    nodes.add_argument("--details", action="store_true")
    nodes.set_defaults(run=print_node_summary)

    timing = subparsers.add_parser("timing", help="summarize paired search-time evidence")
    timing.add_argument(
        "--pair",
        action="append",
        nargs=3,
        required=True,
        metavar=("ORDER", "BASELINE", "CANDIDATE"),
    )
    timing.add_argument("--exact-tree", action="store_true")
    timing.set_defaults(run=print_timing_summary)

    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "timing":
        args.pair = [
            (order, Path(baseline), Path(candidate))
            for order, baseline, candidate in args.pair
        ]
    args.run(args)


if __name__ == "__main__":
    main()
