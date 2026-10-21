#!/usr/bin/env python3
"""Run and compare native search benchmarks."""

import argparse
import csv
from dataclasses import dataclass
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import time

from . import engine, paths


DEFAULT_SUITE = Path(__file__).resolve().parent / "resources/search.epd"
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


def load_suite(path):
    """Read benchmark IDs and four-field FENs without interpreting annotations."""
    import chess

    positions = {}
    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        record = line.strip()
        if not record or record.startswith("#"):
            continue
        fields = record.split(maxsplit=4)
        if len(fields) != 5:
            raise ValueError(f"{path}:{line_number}: expected FEN and operations")
        # Match the native runner's semicolon-delimited operations.
        ids = []
        for operation in fields[4].split(";"):
            parts = operation.split(maxsplit=1)
            if parts and parts[0] == "id":
                ids.append(parts[1].strip() if len(parts) == 2 else "")
        if len(ids) != 1 or not re.fullmatch(r'"[^"\r\n]*"', ids[0]):
            raise ValueError(f"{path}:{line_number}: expected one quoted id")
        case = ids[0][1:-1]
        if not case or case in positions or any(ord(c) < 32 for c in case):
            raise ValueError(f"{path}:{line_number}: empty, duplicate, or invalid id")
        fen = " ".join(fields[:4]) + " 0 1"
        try:
            if not chess.Board(fen).is_valid():
                raise ValueError("invalid position")
        except ValueError as error:
            raise ValueError(f"{path}:{line_number}: {error}") from error
        positions[case] = fen
    if not positions:
        raise ValueError(f"{path}: empty suite")
    return positions


def load_run(path: Path) -> Run:
    try:
        with path.open(encoding="utf-8", newline="") as source:
            reader = csv.DictReader(source, delimiter="\t")
            missing = FIELDS - set(reader.fieldnames or ())
            if missing:
                raise ValueError(f"{path}: missing fields: {','.join(sorted(missing))}")
            parsed = list(reader)
    except OSError as error:
        raise ValueError(f"{path}: {error}")
    if not parsed:
        raise ValueError(f"{path}: empty run")
    rows = {}
    config = None
    for line, row in enumerate(parsed, start=2):
        if None in row or any(row[field] is None for field in FIELDS):
            raise ValueError(f"{path}:{line}: malformed TSV row")
        case = row["case"]
        if not case or case in rows:
            raise ValueError(f"{path}:{line}: empty or duplicate case {case!r}")
        if row["result_format"] != FORMAT:
            raise ValueError(f"{path}:{line}: expected {FORMAT}")
        fields = row["fen"].split()
        if len(fields) != 6 or fields[1] not in ("w", "b"):
            raise ValueError(f"{path}:{line}: invalid six-field FEN")
        if row["limit_type"] not in ("depth", "nodes", "movetime"):
            raise ValueError(f"{path}:{line}: invalid limit type")
        for field in ("limit_value", "threads", "hash_mb", "completed_depth",
                      "nodes", "total_ns", "static_score", "score"):
            try:
                value = int(row[field])
            except ValueError as error:
                raise ValueError(f"{path}:{line}: {field} must be an integer") from error
            if field not in ("static_score", "score") and value <= 0:
                raise ValueError(f"{path}:{line}: {field} must be positive")
        if not row["best_move"] or row["pv"].split()[:1] != [row["best_move"]]:
            raise ValueError(f"{path}:{line}: best move and PV disagree")
        row_config = tuple(row[field] for field in CONFIG)
        if config is None:
            config = row_config
        elif row_config != config:
            raise ValueError(f"{path}:{line}: mixed search settings")
        rows[case] = row
    assert config is not None
    run = Run(path, rows, config)
    require_legal_pvs(run)
    return run


def require_comparable(reference: Run, other: Run) -> None:
    if reference.config != other.config:
        raise ValueError(f"{other.path}: search settings differ from {reference.path}")
    if reference.rows.keys() != other.rows.keys():
        raise ValueError(f"{other.path}: case set differs from {reference.path}")
    changed = [case for case in reference.rows
               if reference.rows[case]["fen"] != other.rows[case]["fen"]]
    if changed:
        raise ValueError(f"{other.path}: position differs for {','.join(sorted(changed))}")


def require_signatures(label: str, reference: Run, other: Run) -> None:
    require_comparable(reference, other)
    changed = [case for case in reference.rows if any(
        reference.rows[case][field] != other.rows[case][field] for field in SIGNATURE)]
    if changed:
        raise ValueError(f"{label}: signature mismatches for {','.join(sorted(changed))}")


def require_legal_pvs(run: Run) -> None:
    """Validate every continuation independently from its full starting FEN."""
    try:
        import chess
    except ModuleNotFoundError:
        raise ValueError("PV legality requires the shared environment; see docs/scripts.md")

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
        except ValueError as error:
            raise ValueError(f"{run.path}: {case}: {error}")


def comparison_summary(pairs: list[tuple[str, Run, Run]]) -> dict:
    """Calculate statistics for validated BC/CB blocks, excluding warmups."""
    baseline_ref, candidate_ref = pairs[0][1:]
    ratios = [candidate.total_ns / baseline.total_ns for _, baseline, candidate in pairs]
    node_ratios = [int(candidate_ref.rows[case]["nodes"]) / int(row["nodes"])
                   for case, row in baseline_ref.rows.items()]
    summary = dict(zip(CONFIG, baseline_ref.config))
    summary.update(
        cases=len(node_ratios),
        baseline_nodes=baseline_ref.total_nodes,
        candidate_nodes=candidate_ref.total_nodes,
        geometric_mean_node_ratio=math.exp(statistics.fmean(map(math.log, node_ratios))),
        total_node_ratio=candidate_ref.total_nodes / baseline_ref.total_nodes,
        root_move_changes=sum(baseline_ref.rows[c]["best_move"] !=
                              candidate_ref.rows[c]["best_move"] for c in baseline_ref.rows),
        score_changes=sum(baseline_ref.rows[c]["score"] !=
                          candidate_ref.rows[c]["score"] for c in baseline_ref.rows),
        pairs=len(pairs),
        timing="paired",
    )
    for index, ((order, _, _), ratio) in enumerate(zip(pairs, ratios), start=1):
        summary[f"pair_{index}_order"] = order
        summary[f"pair_{index}_search_time_ratio"] = ratio
    blocks = [math.sqrt(ratios[i] * ratios[i + 1]) for i in range(0, len(ratios), 2)]
    summary.update(
        median_search_time_ratio=statistics.median(ratios),
        minimum_search_time_ratio=min(ratios),
        maximum_search_time_ratio=max(ratios),
        candidate_wins=sum(r < 1 for r in ratios),
        bc_median_search_time_ratio=statistics.median(ratios[::2]),
        cb_median_search_time_ratio=statistics.median(ratios[1::2]),
    )
    for index, ratio in enumerate(blocks, start=1):
        summary[f"balanced_block_{index}_search_time_ratio"] = ratio
    summary["median_balanced_search_time_ratio"] = statistics.median(blocks)
    return summary


def format_overview(summary: dict) -> str:
    """Describe a successfully validated comparison without judging its benefit."""
    ratio = summary["median_balanced_search_time_ratio"]
    return (
        f"Candidate search time: {(ratio - 1) * 100:+.2f}% (median balanced ratio)\n"
        f"Candidate total nodes: {(summary['total_node_ratio'] - 1) * 100:+.2f}%\n"
        f"Changed best moves: {summary['root_move_changes']} / {summary['cases']}\n"
        "Repeatability and PV legality checks: passed\n"
    )


def format_summary(summary: dict) -> str:
    return "".join(f"{key}={value:.9f}\n" if isinstance(value, float) else f"{key}={value}\n"
                   for key, value in summary.items())


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def finite_seconds(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return number


def benchmark_path(value):
    return paths.build_binary(value, "latrunculi-benchmark")


def print_run_settings(args):
    print(f"Suite: {args.suite.resolve()}\n"
          f"Search: depth {args.depth}; threads {args.threads}; hash {args.hash_mb} MiB\n"
          f"CPU: {args.cpu if args.cpu is not None else 'inherited affinity'}; "
          f"timeout {args.timeout:g}s per run\n"
          f"Output: {args.output.absolute()}", flush=True)


def load_corpus_run(path, suite, args):
    run = load_run(path)
    expected = ("depth", str(args.depth), str(args.threads), str(args.hash_mb))
    if run.config != expected:
        raise ValueError(f"{path}: requested search settings differ")
    if run.rows.keys() != suite.keys():
        raise ValueError(f"{path}: case set differs from suite")
    for case, fen in suite.items():
        if run.rows[case]["fen"] != fen:
            raise ValueError(f"{path}: {case}: position differs from suite")
    return run


def benchmark_command(binary, args):
    return [str(binary), "--suite", str(args.suite.resolve()),
            "--depth", str(args.depth), "--threads", str(args.threads),
            "--hash", str(args.hash_mb)]


def run_corpus(args):
    binary, suite = engine.executable(args.benchmark), load_suite(args.suite)
    prefix, _ = affinity(args.cpu)
    reference = load_corpus_run(args.reference, suite, args) if args.reference else None
    args.output.mkdir(parents=True, exist_ok=False)
    print(f"Benchmark: {binary}\nRepeats: {args.repeats}\n"
          f"Reference: {args.reference.resolve() if args.reference else 'none'}", flush=True)
    print_run_settings(args)
    for repeat in range(1, args.repeats + 1):
        path = args.output / f"corpus-{repeat}.tsv"
        engine.collect(prefix + benchmark_command(binary, args), path, args.timeout)
        run = load_corpus_run(path, suite, args)
        if reference is not None:
            require_signatures("corpus reference/repeat", reference, run)
        else:
            reference = run
        print(f"{path.name}: {len(run.rows)} cases, {run.total_nodes} nodes; checks pass",
              flush=True)


def require_untraced():
    status = Path("/proc/self/status")
    if status.exists():
        for line in status.read_text().splitlines():
            if line.startswith("TracerPid:") and int(line.split()[1]):
                raise ValueError("timing must run without tracing/ptrace")


def cpu_counters():
    path = Path("/proc/stat")
    if not path.exists():
        return {}
    rows = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields and re.fullmatch(r"cpu\d*", fields[0]):
            counters = list(map(int, fields[1:9]))
            rows[fields[0]] = (sum(counters), counters[3] + counters[4])
    return rows


def cpu_load(before, after):
    result = {}
    for cpu in before.keys() & after.keys():
        total = after[cpu][0] - before[cpu][0]
        idle = after[cpu][1] - before[cpu][1]
        if total > 0:
            result[cpu] = (total - idle) / total
    return result


def affinity(cpu):
    if cpu is None:
        return [], []
    if not hasattr(os, "sched_getaffinity") or cpu not in os.sched_getaffinity(0):
        raise ValueError(f"CPU {cpu} is not available in this process's affinity")
    taskset = shutil.which("taskset")
    if taskset is None:
        raise ValueError("CPU affinity requires taskset")
    path = Path(f"/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list")
    siblings = []
    if path.exists():
        for group in path.read_text().strip().split(","):
            bounds = list(map(int, group.split("-")))
            siblings.extend(range(bounds[0], bounds[-1] + 1))
    return [taskset, "--cpu-list", str(cpu)], [s for s in siblings if s != cpu]


def compare_benchmarks(args):
    if args.pairs < 2 or args.pairs % 2:
        raise ValueError("comparison needs an even pair count of at least two")
    require_untraced()
    prefix, siblings = affinity(args.cpu)
    suite = load_suite(args.suite)
    binaries = {name: engine.executable(getattr(args, name)) for name in ("baseline", "candidate")}
    args.output.mkdir(parents=True, exist_ok=False)
    print(f"Baseline: {binaries['baseline']}\nCandidate: {binaries['candidate']}\n"
          f"Comparison: {args.pairs} pairs (BC/CB); "
          f"exact tree {'yes' if args.exact_tree else 'no'}; "
          f"idle {args.idle_seconds:g}s", flush=True)
    print_run_settings(args)
    schedule = [(f"warmup-{name}", name) for name in binaries]
    for pair in range(1, args.pairs + 1):
        order = ("baseline", "candidate") if pair % 2 else ("candidate", "baseline")
        schedule.extend((f"pair-{pair}-{name}", name) for name in order)

    references, runs = {}, {}
    with (args.output / "timing-load.jsonl").open("x") as metrics:
        before, start = cpu_counters(), time.monotonic()
        time.sleep(args.idle_seconds)
        metrics.write(json.dumps({"run": "idle", "cpu": args.cpu, "smt_siblings": siblings,
                                  "wall_seconds": time.monotonic() - start,
                                  "cpu_busy_fraction": cpu_load(before, cpu_counters())}) + "\n")
        metrics.flush()
        for label, name in schedule:
            before = cpu_counters()
            path = args.output / f"{label}.tsv"
            wall = engine.collect(prefix + benchmark_command(binaries[name], args), path, args.timeout)
            metrics.write(json.dumps({"run": label, "wall_seconds": wall,
                                      "cpu_busy_fraction": cpu_load(before, cpu_counters())}) + "\n")
            metrics.flush()
            measured = load_corpus_run(path, suite, args)
            if name in references:
                require_signatures(label, references[name], measured)
            else:
                references[name] = measured
                if len(references) == 2:
                    require_comparable(references["baseline"], references["candidate"])
                    if args.exact_tree:
                        require_signatures("exact-tree warmups", references["baseline"],
                                           references["candidate"])
            runs[label] = measured

    pairs = [("BC" if pair % 2 else "CB", runs[f"pair-{pair}-baseline"],
              runs[f"pair-{pair}-candidate"]) for pair in range(1, args.pairs + 1)]
    summary = comparison_summary(pairs)
    details = format_summary(summary)
    with (args.output / "summary.txt").open("x") as output:
        output.write(details)
    print(format_overview(summary) + "\n" + details, end="", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(prog="benchmark", description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run_parser = commands.add_parser("run", help="collect corpus passes")
    run_parser.add_argument("benchmark", type=benchmark_path,
                            help="build name or executable path containing /")
    compare_parser = commands.add_parser("compare", help="collect fresh paired comparisons")
    compare_parser.add_argument("baseline", type=benchmark_path,
                                help="build name or executable path containing /")
    compare_parser.add_argument("candidate", type=benchmark_path, nargs="?", default="release-dev",
                                help="build name or executable path (default: release-dev)")
    compare_parser.add_argument("--pairs", type=positive_int, default=6)
    compare_parser.add_argument("--idle-seconds", type=finite_seconds, default=5)
    compare_parser.add_argument("--exact-tree", action="store_true")
    for subparser in (run_parser, compare_parser):
        subparser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
        subparser.add_argument("--depth", type=positive_int, default=10)
        subparser.add_argument("--threads", type=positive_int, default=1)
        subparser.add_argument("--hash", dest="hash_mb", type=positive_int, default=32)
        subparser.add_argument("--cpu", type=int)
    run_parser.add_argument("--repeats", type=positive_int, default=1)
    run_parser.add_argument("--reference", type=Path)
    for subparser in (run_parser, compare_parser):
        output = subparser.add_mutually_exclusive_group(required=True)
        output.add_argument("--name", type=paths.name,
                            help="new output name under this checkout's tools/output/")
        output.add_argument("--output", type=Path,
                            help="new directory for raw outputs; existing paths are refused")
        subparser.add_argument("--timeout", type=finite_seconds, default=600,
                               help="seconds per child process (default: 600)")
    args = parser.parse_args(argv)
    args.output = paths.output_directory(args.name, args.output)
    try:
        {"run": run_corpus, "compare": compare_benchmarks}[args.command](args)
    except ModuleNotFoundError as error:
        raise SystemExit(f"{error}; set up the shared environment in docs/scripts.md") from error
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
    except KeyboardInterrupt:
        print("benchmark: interrupted", file=sys.stderr)
        raise SystemExit(130) from None


if __name__ == "__main__":
    main()
