from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from pathlib import Path

from .common import (
    DEFAULT_CPP_BUILD_PRESET,
    add_common_run_args,
    average,
    base_manifest,
    build_binary,
    default_benchmark_path,
    format_num,
    make_run_dir,
    median,
    num,
    percent_delta,
    read_tsv,
    run_capture,
    write_manifest,
    write_tsv,
)

CPP_FORMAT = "cpp-benchmark"
SEARCH_SMOKE_FORMAT = "search-smoke"

CPP_RESULT_COLUMNS = [
    "result_format",
    "repeat",
    "suite",
    "case",
    "mode",
    "profile",
    "iterations",
    "ops",
    "total_ns",
    "ns_per_op",
    "nodes_per_sec",
    "moves_per_op",
]

SEARCH_SMOKE_COLUMNS = [
    "result_format",
    "case",
    "profile",
    "threads",
    "movetime_ms",
    "depth",
    "nodes",
    "time_ms",
    "nps",
    "bestmove",
    "plausible",
    "fen",
]


def add_cpp_suite_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser], suite: str) -> None:
    parser = subparsers.add_parser(suite, help=f"run C++ {suite} benchmark suite")
    add_common_run_args(parser, build_preset=DEFAULT_CPP_BUILD_PRESET)
    parser.add_argument("--profile", choices=["smoke", "standard"], default="smoke")
    parser.add_argument("--benchmark", type=Path, help="benchmark binary path")
    parser.add_argument("--repeats", type=int, default=1, help="number of benchmark binary repeats")


def add_search_smoke_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("search-smoke", help="run C++ search-smoke benchmark suite")
    add_common_run_args(parser, build_preset=DEFAULT_CPP_BUILD_PRESET)
    parser.add_argument("--profile", choices=["smoke", "standard"], default="smoke")
    parser.add_argument("--benchmark", type=Path, help="benchmark binary path")
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--movetime", type=int, default=1000)
    parser.add_argument("--limit", type=int, default=5)
    parser.add_argument("--testfile", type=Path, help="optional search-smoke EPD file")


def command_run_cpp(args: argparse.Namespace) -> int:
    if args.repeats <= 0:
        raise ValueError("--repeats must be positive")
    build_binary(args, "benchmark")
    benchmark = args.benchmark or default_benchmark_path(args.build_preset)
    if not benchmark.exists():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")

    command = [
        str(benchmark),
        "--suite",
        args.suite,
        "--profile",
        args.profile,
        "--format",
        "tsv",
    ]
    run_dir = make_run_dir(args.output_root, args.label)
    rows: list[dict[str, str]] = []
    for repeat in range(1, args.repeats + 1):
        stdout, stderr = run_capture(command, cwd=args.repo)
        if not stdout.strip():
            raise RuntimeError(f"{args.suite} repeat {repeat} produced no TSV output")
        (run_dir / "raw" / f"benchmark-r{repeat}.stdout").write_text(stdout, encoding="utf-8")
        (run_dir / "raw" / f"benchmark-r{repeat}.stderr").write_text(stderr, encoding="utf-8")
        rows.extend(parse_cpp_rows(stdout, args.suite, repeat))

    manifest = base_manifest(args, run_dir, CPP_FORMAT)
    manifest.update(
        {
            "benchmark_path": str(benchmark.resolve()),
            "command": command,
            "profile": args.profile,
            "repeats": args.repeats,
        }
    )
    write_tsv(run_dir / "results.tsv", rows, CPP_RESULT_COLUMNS)
    write_manifest(run_dir / "manifest.json", manifest)
    (run_dir / "summary.md").write_text(render_cpp_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def parse_cpp_rows(output: str, suite: str, repeat: int) -> list[dict[str, str]]:
    reader = csv.DictReader(output.splitlines(), delimiter="\t")
    if reader.fieldnames is None or "suite" not in reader.fieldnames:
        raise RuntimeError(f"{suite} produced unexpected TSV header")
    rows = []
    for row in reader:
        if row.get("suite") != suite:
            raise RuntimeError(f"{suite} produced row for unexpected suite: {row.get('suite')}")
        rows.append({"result_format": CPP_FORMAT, "repeat": str(repeat), **row})
    if not rows:
        raise RuntimeError(f"{suite} produced no TSV rows")
    return rows


def command_run_search_smoke(args: argparse.Namespace) -> int:
    build_binary(args, "benchmark")
    benchmark = args.benchmark or default_benchmark_path(args.build_preset)
    if not benchmark.exists():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")

    command = [
        str(benchmark),
        "--suite",
        "search-smoke",
        "--profile",
        args.profile,
        "--threads",
        str(args.threads),
        "--movetime",
        str(args.movetime),
        "--limit",
        str(args.limit),
    ]
    if args.testfile:
        command.append(str(args.testfile))

    stdout, stderr = run_capture(command, cwd=args.repo)
    run_dir = make_run_dir(args.output_root, args.label)
    (run_dir / "raw" / "benchmark.stdout").write_text(stdout, encoding="utf-8")
    (run_dir / "raw" / "benchmark.stderr").write_text(stderr, encoding="utf-8")
    rows = parse_search_smoke_rows(stdout, args)
    write_tsv(run_dir / "results.tsv", rows, SEARCH_SMOKE_COLUMNS)

    manifest = base_manifest(args, run_dir, SEARCH_SMOKE_FORMAT)
    manifest.update(
        {
            "benchmark_path": str(benchmark.resolve()),
            "command": command,
            "profile": args.profile,
            "threads": args.threads,
            "movetime_ms": args.movetime,
            "limit": args.limit,
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)
    (run_dir / "summary.md").write_text(render_search_smoke_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def parse_search_smoke_rows(output: str, args: argparse.Namespace) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    pattern = re.compile(
        r"^Observed: bestmove (\S+) \(([^)]*)\) \| depth (\d+) \| nodes (\d+) \| time (\d+) ms \| nps (\d+) \| fen (.+)$"
    )
    for line in output.splitlines():
        match = pattern.match(line.strip())
        if not match:
            continue
        bestmove, plausible, depth, nodes, time_ms, nps, fen = match.groups()
        rows.append(
            {
                "result_format": SEARCH_SMOKE_FORMAT,
                "case": f"case-{len(rows) + 1}",
                "profile": args.profile,
                "threads": str(args.threads),
                "movetime_ms": str(args.movetime),
                "depth": depth,
                "nodes": nodes,
                "time_ms": time_ms,
                "nps": nps,
                "bestmove": bestmove,
                "plausible": plausible,
                "fen": fen,
            }
        )
    if not rows:
        raise RuntimeError("search-smoke produced no observed benchmark rows")
    return rows


def cpp_key(row: dict[str, str]) -> tuple[str, str, str, str]:
    return (row.get("suite", ""), row.get("case", ""), row.get("mode", ""), row.get("profile", ""))


def group_cpp_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str, str, str], list[dict[str, str]]]:
    grouped: dict[tuple[str, str, str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[cpp_key(row)].append(row)
    return dict(sorted(grouped.items()))


def aggregate_cpp_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str, str, str], dict[str, float | str | None]]:
    result: dict[tuple[str, str, str, str], dict[str, float | str | None]] = {}
    for key, group in group_cpp_rows(rows).items():
        ns_values = [num(row.get("ns_per_op")) for row in group]
        nps_values = [num(row.get("nodes_per_sec")) for row in group]
        result[key] = {
            "repeat_count": float(len({row.get("repeat", "") for row in group})),
            "median_ns_per_op": median(ns_values),
            "mean_ns_per_op": average(ns_values),
            "min_ns_per_op": min(value for value in ns_values if value is not None),
            "max_ns_per_op": max(value for value in ns_values if value is not None),
            "median_nodes_per_sec": median(nps_values),
            "mean_nodes_per_sec": average(nps_values),
            "moves_per_op": average(num(row.get("moves_per_op")) for row in group),
        }
    return result


def render_cpp_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    grouped = aggregate_cpp_rows(rows)
    lines = [
        f"# Benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Suite: `{manifest['suite']}`",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Profile: `{manifest['profile']}`",
        f"- Repeats: `{manifest['repeats']}`",
        "",
        "## Aggregated Results",
        "| Case | Mode | Repeats | Median ns/op | Mean ns/op | Min ns/op | Max ns/op | Median nodes/sec | Mean nodes/sec | Moves/op |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for (_suite, case_id, mode, _profile), agg in grouped.items():
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
                case_id,
                mode,
                format_num(agg["repeat_count"]),
                format_num(agg["median_ns_per_op"], digits=3),
                format_num(agg["mean_ns_per_op"], digits=3),
                format_num(agg["min_ns_per_op"], digits=3),
                format_num(agg["max_ns_per_op"], digits=3),
                format_num(agg["median_nodes_per_sec"]),
                format_num(agg["mean_nodes_per_sec"]),
                format_num(agg["moves_per_op"], digits=3),
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def render_search_smoke_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    avg_depth = average(num(row.get("depth")) for row in rows)
    avg_nps = average(num(row.get("nps")) for row in rows)
    lines = [
        f"# Search smoke benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Threads: `{manifest['threads']}`",
        f"- Movetime: `{manifest['movetime_ms']} ms`",
        "",
        "## Summary",
        f"- Cases: `{len(rows)}`",
        f"- Average depth: `{format_num(avg_depth)}`",
        f"- Average NPS: `{format_num(avg_nps)}`",
        "",
        "## Results",
        "| Case | Depth | Nodes | Time ms | NPS | Bestmove | Plausible |",
        "|---|---:|---:|---:|---:|---|---|",
    ]
    for row in rows:
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} |".format(
                row["case"],
                row["depth"],
                row["nodes"],
                row["time_ms"],
                row["nps"],
                row["bestmove"],
                row["plausible"],
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def render_cpp_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv")
    _, new_rows = read_tsv(new_dir / "results.tsv")
    old = aggregate_cpp_rows(old_rows)
    new = aggregate_cpp_rows(new_rows)
    keys = sorted(set(old) | set(new))
    lines = [
        f"# Benchmark comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        "",
        "## Deltas",
        "| Case | Mode | Median ns/op old | Median ns/op new | ns/op delta | Median nodes/sec old | Median nodes/sec new | nodes/sec delta |",
        "|---|---|---:|---:|---:|---:|---:|---:|",
    ]
    for key in keys:
        old_agg = old.get(key, {})
        new_agg = new.get(key, {})
        case_id = key[1]
        mode = key[2]
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} |".format(
                case_id,
                mode,
                format_num(old_agg.get("median_ns_per_op"), digits=3),
                format_num(new_agg.get("median_ns_per_op"), digits=3),
                percent_delta(old_agg.get("median_ns_per_op"), new_agg.get("median_ns_per_op")),
                format_num(old_agg.get("median_nodes_per_sec")),
                format_num(new_agg.get("median_nodes_per_sec")),
                percent_delta(old_agg.get("median_nodes_per_sec"), new_agg.get("median_nodes_per_sec")),
            )
        )
    return "\n".join(lines).rstrip() + "\n"
