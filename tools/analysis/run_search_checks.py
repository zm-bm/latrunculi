#!/usr/bin/env python3
"""Collect corpus, fingerprint, or paired timing evidence from built engines."""

import argparse
import contextlib
import io
import json
import math
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time

if __package__:
    from . import compare_search
else:
    import compare_search


DEFAULT_SUITE = Path(__file__).with_name("search.epd")


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
        lexer = shlex.shlex(fields[4], posix=False, punctuation_chars=";")
        lexer.whitespace_split = True
        lexer.commenters = ""
        operations, operation = [], []
        for token in lexer:
            if token and set(token) == {";"}:
                operations.append(operation)
                operation = []
            else:
                operation.append(token)
        operations.append(operation)
        ids = [op for op in operations if op and op[0] == "id"]
        if (len(ids) != 1 or len(ids[0]) != 2
                or not ids[0][1].startswith('"') or not ids[0][1].endswith('"')):
            raise ValueError(f"{path}:{line_number}: expected one quoted id")
        case = ids[0][1][1:-1]
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


def check_run(path, suite, args):
    run = compare_search.load_run(path)
    expected = ("depth", str(args.depth), str(args.threads), str(args.hash_mb))
    if run.config != expected:
        raise ValueError(f"{path}: requested search settings differ")
    if run.rows.keys() != suite.keys():
        raise ValueError(f"{path}: case set differs from suite")
    for case, fen in suite.items():
        if run.rows[case]["fen"] != fen:
            raise ValueError(f"{path}: {case}: position differs from suite")
    compare_search.require_legal_pvs(run)
    return run


def executable(path):
    path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise ValueError(f"not an executable: {path}")
    return path


def benchmark_command(binary, args):
    return [str(binary), "--suite", str(args.suite.resolve()),
            "--depth", str(args.depth), "--threads", str(args.threads),
            "--hash", str(args.hash_mb)]


def collect(command, output, timeout):
    """Keep raw stdout/stderr even when the child fails or times out."""
    print(f"Running {output.name}", flush=True)
    with output.open("x") as stdout, output.with_suffix(".stderr.txt").open("x") as stderr:
        start = time.monotonic()
        subprocess.run(command, stdout=stdout, stderr=stderr, check=True, timeout=timeout)
    return time.monotonic() - start


def corpus(args):
    binary, suite = executable(args.bench), load_suite(args.suite)
    reference = check_run(args.reference, suite, args) if args.reference else None
    args.output.mkdir(parents=True, exist_ok=False)
    for repeat in range(1, args.repeats + 1):
        path = args.output / f"corpus-{repeat}.tsv"
        collect(benchmark_command(binary, args), path, args.timeout)
        run = check_run(path, suite, args)
        if reference is not None:
            compare_search.require_signatures("corpus reference/repeat", reference, run)
        else:
            reference = run
        print(f"{path.name}: {len(run.rows)} cases, {run.total_nodes} nodes; checks pass",
              flush=True)


def fingerprint_nodes(path):
    matches = re.findall(r"^\s*(\d+) nodes(?:\s|$)", path.read_text(), re.MULTILINE)
    if len(matches) != 1 or int(matches[0]) <= 0:
        raise ValueError(f"{path}: expected one positive bench node count")
    return int(matches[0])


def fingerprint(args):
    binary = executable(args.engine)
    reference = fingerprint_nodes(args.reference) if args.reference else None
    args.output.mkdir(parents=True, exist_ok=False)
    for repeat in range(1, args.repeats + 1):
        path = args.output / f"fingerprint-{repeat}.txt"
        collect([str(binary), "bench"], path, args.timeout)
        nodes = fingerprint_nodes(path)
        if reference is not None and nodes != reference:
            raise ValueError(f"{path}: fingerprint differs: {nodes} != {reference}")
        reference = nodes
        print(f"{path.name}: {nodes} nodes; checks pass", flush=True)


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


def timing(args):
    if args.pairs < 2 or args.pairs % 2:
        raise ValueError("timing needs an even pair count of at least two")
    require_untraced()
    prefix, siblings = affinity(args.cpu)
    suite = load_suite(args.suite)
    binaries = {name: executable(getattr(args, f"{name}_bench"))
                for name in ("baseline", "candidate")}
    references = {name: check_run(getattr(args, f"{name}_reference"), suite, args)
                  for name in binaries}
    if args.exact_tree:
        compare_search.require_signatures("exact-tree references", references["baseline"],
                                          references["candidate"])
    args.output.mkdir(parents=True, exist_ok=False)
    pairs = []
    schedule = [(f"warmup-{name}", name) for name in binaries]
    for pair in range(1, args.pairs + 1):
        order = ("baseline", "candidate") if pair % 2 else ("candidate", "baseline")
        schedule.extend((f"pair-{pair}-{name}", name) for name in order)
        pairs.append(("BC" if pair % 2 else "CB",
                      args.output / f"pair-{pair}-baseline.tsv",
                      args.output / f"pair-{pair}-candidate.tsv"))
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
            wall = collect(prefix + benchmark_command(binaries[name], args), path, args.timeout)
            after = cpu_counters()
            metrics.write(json.dumps({"run": label, "wall_seconds": wall,
                                      "cpu_busy_fraction": cpu_load(before, after)}) + "\n")
            metrics.flush()
            run = check_run(path, suite, args)
            compare_search.require_signatures(label, references[name], run)
    summary = io.StringIO()
    with contextlib.redirect_stdout(summary):
        compare_search.timing(argparse.Namespace(pair=pairs, exact_tree=args.exact_tree))
    with (args.output / "timing.txt").open("x") as output:
        output.write(summary.getvalue())
    print(summary.getvalue(), end="", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    corpus_parser = commands.add_parser("corpus")
    corpus_parser.add_argument("--bench", type=Path, required=True)
    fingerprint_parser = commands.add_parser("fingerprint")
    fingerprint_parser.add_argument("--engine", type=Path, required=True)
    timing_parser = commands.add_parser("timing")
    for name in ("baseline", "candidate"):
        timing_parser.add_argument(f"--{name}-bench", type=Path, required=True)
        timing_parser.add_argument(f"--{name}-reference", type=Path, required=True)
    timing_parser.add_argument("--pairs", type=positive_int, default=6)
    timing_parser.add_argument("--cpu", type=int)
    timing_parser.add_argument("--idle-seconds", type=finite_seconds, default=5)
    timing_parser.add_argument("--exact-tree", action="store_true")
    for subparser in (corpus_parser, timing_parser):
        subparser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
        subparser.add_argument("--depth", type=positive_int, default=10)
        subparser.add_argument("--threads", type=positive_int, default=1)
        subparser.add_argument("--hash", dest="hash_mb", type=positive_int, default=32)
    for subparser in (corpus_parser, fingerprint_parser):
        subparser.add_argument("--repeats", type=positive_int, default=2)
        subparser.add_argument("--reference", type=Path)
    for subparser in (corpus_parser, fingerprint_parser, timing_parser):
        subparser.add_argument("--output", type=Path, required=True,
                               help="new directory for raw outputs; existing paths are refused")
        subparser.add_argument("--timeout", type=finite_seconds, default=600,
                               help="seconds per child process (default: 600)")
    args = parser.parse_args(argv)
    try:
        {"corpus": corpus, "fingerprint": fingerprint, "timing": timing}[args.command](args)
    except ModuleNotFoundError as error:
        raise SystemExit(f"{error}; install tools/analysis/requirements.txt") from error
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error


if __name__ == "__main__":
    main()
