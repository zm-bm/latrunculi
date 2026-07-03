#!/usr/bin/env python3

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from benchlib.common import read_manifest
from benchlib.perft import PERFT_FORMAT, add_perft_parser, command_run_perft, render_perft_compare
from benchlib.uci import (
    SEARCH_FORMAT,
    add_search_parser,
    command_run_search,
    render_search_compare,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Unified benchmark organizer for latrunculi.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run a benchmark suite into scratch/bench-runs")
    run_subparsers = run.add_subparsers(dest="suite", required=True)
    add_search_parser(run_subparsers)
    add_perft_parser(run_subparsers)

    compare = subparsers.add_parser("compare", help="compare two benchmark run directories")
    compare.add_argument("baseline_run_dir", type=Path)
    compare.add_argument("candidate_run_dir", type=Path)
    return parser.parse_args()


def command_compare(args: argparse.Namespace) -> int:
    baseline = args.baseline_run_dir.resolve()
    candidate = args.candidate_run_dir.resolve()
    old_manifest = read_manifest(baseline)
    new_manifest = read_manifest(candidate)
    old_format = old_manifest.get("result_format")
    new_format = new_manifest.get("result_format")
    if old_format != new_format:
        raise ValueError("cannot compare runs with different result formats")
    old_suite = old_manifest.get("suite")
    new_suite = new_manifest.get("suite")
    if old_suite is not None and new_suite is not None and old_suite != new_suite:
        raise ValueError("cannot compare runs from different suites")

    output = candidate / f"comparison-vs-{baseline.name}.md"
    if old_format == SEARCH_FORMAT:
        content = render_search_compare(baseline, candidate, old_manifest, new_manifest)
    elif old_format == PERFT_FORMAT:
        content = render_perft_compare(baseline, candidate, old_manifest, new_manifest)
    else:
        raise ValueError(f"compare is not supported for result format: {old_format}")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content, encoding="utf-8")
    print(output)
    return 0


def main() -> int:
    args = parse_args()
    if args.command == "compare":
        return command_compare(args)
    if args.suite == "search":
        return command_run_search(args)
    if args.suite == "perft":
        return command_run_perft(args)
    raise ValueError(f"unknown suite: {args.suite}")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        raise SystemExit(1)
