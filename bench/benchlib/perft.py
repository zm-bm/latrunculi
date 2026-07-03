from __future__ import annotations

import argparse
import csv
from pathlib import Path

from .common import (
    add_common_run_args,
    base_manifest,
    build_binary,
    default_benchmark_path,
    format_num,
    make_run_dir,
    percent_delta,
    read_tsv,
    run_capture,
    write_manifest,
    write_tsv,
)

PERFT_FORMAT = "perft"

PERFT_COLUMNS = [
    "result_format",
    "case",
    "profile",
    "depth",
    "nodes",
    "expected_nodes",
    "total_ns",
    "nodes_per_sec",
]


def add_perft_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("perft", help="run recursive move generation benchmark")
    add_common_run_args(parser)
    parser.add_argument("--profile", choices=["smoke", "standard"], default="smoke")


def command_run_perft(args: argparse.Namespace) -> int:
    build_binary(args, "benchmark")
    benchmark = default_benchmark_path(args.build_preset)
    if not benchmark.exists():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")

    command = [str(benchmark), "--profile", args.profile, "--format", "tsv"]
    stdout, stderr = run_capture(command, cwd=args.repo)
    if not stdout.strip():
        raise RuntimeError("perft benchmark produced no TSV output")

    run_dir = make_run_dir(args.output_root, args.label)
    (run_dir / "raw" / "benchmark.stdout").write_text(stdout, encoding="utf-8")
    (run_dir / "raw" / "benchmark.stderr").write_text(stderr, encoding="utf-8")
    rows = parse_perft_rows(stdout)

    manifest = base_manifest(args, run_dir, PERFT_FORMAT)
    manifest.update(
        {
            "benchmark_path": str(benchmark.resolve()),
            "command": command,
            "profile": args.profile,
        }
    )
    write_tsv(run_dir / "results.tsv", rows, PERFT_COLUMNS)
    write_manifest(run_dir / "manifest.json", manifest)
    (run_dir / "summary.md").write_text(render_perft_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def parse_perft_rows(output: str) -> list[dict[str, str]]:
    reader = csv.DictReader(output.splitlines(), delimiter="\t")
    if reader.fieldnames is None or reader.fieldnames != PERFT_COLUMNS:
        raise RuntimeError("perft benchmark produced unexpected TSV header")
    rows = [dict(row) for row in reader]
    if not rows:
        raise RuntimeError("perft benchmark produced no TSV rows")
    for row in rows:
        if row.get("result_format") != PERFT_FORMAT:
            raise RuntimeError(f"perft benchmark produced unexpected row format: {row.get('result_format')}")
    return rows


def render_perft_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    lines = [
        f"# Perft benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Profile: `{manifest['profile']}`",
        "",
        "## Results",
        "| Case | Depth | Nodes | Time ms | Nodes/sec |",
        "|---|---:|---:|---:|---:|",
    ]
    for row in rows:
        total_ms = float(row["total_ns"]) / 1_000_000.0
        lines.append(
            "| {} | {} | {} | {} | {} |".format(
                row["case"],
                row["depth"],
                row["nodes"],
                format_num(total_ms, digits=3),
                format_num(row["nodes_per_sec"]),
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def perft_key(row: dict[str, str]) -> str:
    return row["case"]


def render_perft_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv")
    _, new_rows = read_tsv(new_dir / "results.tsv")
    old = {perft_key(row): row for row in old_rows}
    new = {perft_key(row): row for row in new_rows}
    keys = sorted(set(old) | set(new))
    lines = [
        f"# Perft comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        "",
        "## Deltas",
        "| Case | Depth old/new | Nodes old/new | Time old/new | Time delta | Nodes/sec old/new | Nodes/sec delta |",
        "|---|---|---|---|---:|---|---:|",
    ]
    for key in keys:
        old_row = old.get(key, {})
        new_row = new.get(key, {})
        old_ms = total_ms(old_row)
        new_ms = total_ms(new_row)
        lines.append(
            "| {} | {} / {} | {} / {} | {} / {} | {} | {} / {} | {} |".format(
                key,
                old_row.get("depth", ""),
                new_row.get("depth", ""),
                old_row.get("nodes", ""),
                new_row.get("nodes", ""),
                format_num(old_ms, digits=3),
                format_num(new_ms, digits=3),
                percent_delta(old_ms, new_ms),
                format_num(old_row.get("nodes_per_sec")),
                format_num(new_row.get("nodes_per_sec")),
                percent_delta(old_row.get("nodes_per_sec"), new_row.get("nodes_per_sec")),
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def total_ms(row: dict[str, str]) -> float | None:
    if not row:
        return None
    return float(row["total_ns"]) / 1_000_000.0
