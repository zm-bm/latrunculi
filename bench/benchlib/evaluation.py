from __future__ import annotations

import argparse
import difflib
import sys
from pathlib import Path

from .common import (
    BENCH_DIR,
    DEFAULT_BUILD_PRESET,
    REPO_ROOT,
    build_binary,
    default_benchmark_path,
    run_capture,
)

EVALUATION_CORPUS = BENCH_DIR / "eval/corpus.tsv"
EVALUATION_BASELINE = BENCH_DIR / "eval/baseline.tsv"


def add_evaluation_parser(
    subparsers: argparse._SubParsersAction[argparse.ArgumentParser],
) -> None:
    parser = subparsers.add_parser(
        "eval", help="validate and manage deterministic evaluation snapshots"
    )
    actions = parser.add_subparsers(dest="eval_action", required=True)
    for name, help_text in (
        ("emit", "write the current evaluation snapshot to stdout"),
        ("verify", "compare the current snapshot with the checked-in baseline"),
        ("regenerate", "replace the checked-in baseline explicitly"),
    ):
        action = actions.add_parser(name, help=help_text)
        action.add_argument(
            "--benchmark", type=Path, help="benchmark binary; bypasses the configured build"
        )
    parser.set_defaults(
        repo=REPO_ROOT,
        build_preset=DEFAULT_BUILD_PRESET,
        skip_build=False,
    )


def command_evaluation(args: argparse.Namespace) -> int:
    benchmark = resolve_benchmark(args)
    command = [
        str(benchmark),
        "eval",
        "--corpus",
        str(EVALUATION_CORPUS.resolve()),
    ]

    snapshot, stderr = run_capture(command, cwd=args.repo)
    if stderr:
        print(stderr, file=sys.stderr, end="")
    if not snapshot:
        raise RuntimeError("evaluation snapshot runner produced no output")

    if args.eval_action == "emit":
        sys.stdout.write(snapshot)
        return 0
    if args.eval_action == "verify":
        verify_snapshot(snapshot)
        print(EVALUATION_BASELINE)
        return 0
    if args.eval_action == "regenerate":
        replace_baseline(snapshot)
        print(EVALUATION_BASELINE)
        return 0
    raise ValueError(f"unknown evaluation action: {args.eval_action}")


def resolve_benchmark(args: argparse.Namespace) -> Path:
    if args.benchmark is not None:
        benchmark = args.benchmark.expanduser().resolve()
    else:
        build_binary(args, "benchmark")
        benchmark = default_benchmark_path(args.build_preset)
    if not benchmark.exists():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")
    return benchmark.resolve()


def verify_snapshot(snapshot: str) -> None:
    if not EVALUATION_BASELINE.exists():
        raise FileNotFoundError(f"evaluation baseline not found: {EVALUATION_BASELINE}")
    baseline = EVALUATION_BASELINE.read_text(encoding="utf-8")
    if snapshot == baseline:
        return

    diff = list(
        difflib.unified_diff(
            baseline.splitlines(keepends=True),
            snapshot.splitlines(keepends=True),
            fromfile=str(EVALUATION_BASELINE),
            tofile="current evaluation snapshot",
            n=3,
        )
    )
    limit = 200
    preview = "".join(diff[:limit])
    if len(diff) > limit:
        preview += f"... diff truncated after {limit} lines\n"
    raise RuntimeError(f"evaluation snapshot differs from baseline:\n{preview}")


def replace_baseline(snapshot: str) -> None:
    EVALUATION_BASELINE.parent.mkdir(parents=True, exist_ok=True)
    temporary = EVALUATION_BASELINE.with_suffix(".tmp")
    temporary.write_text(snapshot, encoding="utf-8")
    temporary.replace(EVALUATION_BASELINE)
