#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import subprocess
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Iterable

from direct_uci_bench import DEFAULT_POSITIONS, load_positions, parse_threads, run_position, select_positions

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_OUTPUT_ROOT = Path("/home/rick/code/AGENTS/latrunculi/scratch/bench-runs")
DEFAULT_BUILD_PRESET = "release-stats"
DEFAULT_ENGINE = REPO_ROOT / "build/release-stats/bin/latrunculi"
SCHEMA_VERSION = "search-experiment-v1"

PROFILES = {
    "smoke": {"positions": "startpos,arasan20-01", "threads": "1,4", "movetime": 500, "repeats": 1, "hash": 64},
    "mid": {"positions": "all", "threads": "1,4", "movetime": 1500, "repeats": 3, "hash": 64},
    "wide": {"positions": "all", "threads": "1,2,4,8", "movetime": 2000, "repeats": 3, "hash": 64},
}

RESULT_COLUMNS = [
    "schema_version",
    "position_id",
    "repeat",
    "threads",
    "movetime_ms",
    "depth",
    "seldepth",
    "score_type",
    "score_value",
    "nodes",
    "time_ms",
    "nps",
    "bestmove",
    "pv",
    "stats_beta_cutoffs",
    "stats_cutoff_early_pct",
    "stats_cutoff_late_pct",
    "stats_tt_hit_pct",
    "stats_tt_cutoff_pct",
    "stats_qnode_pct",
    "stats_ebf",
    "stats_cumulative_ebf",
    "raw_output_file",
]

COMPARE_FIELDS = [
    "depth",
    "nodes",
    "nps",
    "stats_beta_cutoffs",
    "stats_cutoff_early_pct",
    "stats_cutoff_late_pct",
    "stats_tt_hit_pct",
    "stats_tt_cutoff_pct",
    "stats_qnode_pct",
    "stats_ebf",
    "stats_cumulative_ebf",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run and compare stats-enabled direct-UCI search experiments.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="create a timestamped stats-enabled benchmark run directory")
    run.add_argument("--label", required=True, help="short label included in the run directory name")
    run.add_argument("--profile", choices=sorted(PROFILES), default="mid", help="predefined run profile (default: mid)")
    run.add_argument("--positions", help="comma-separated position ids, or all")
    run.add_argument("--threads", help="comma-separated thread counts")
    run.add_argument("--movetime", type=int, help="movetime in milliseconds")
    run.add_argument("--repeats", type=int, help="number of repeats per position/thread")
    run.add_argument("--hash", type=int, help="hash size in MiB")
    run.add_argument("--positions-file", type=Path, default=DEFAULT_POSITIONS)
    run.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    run.add_argument("--repo", type=Path, default=REPO_ROOT)
    run.add_argument("--build-preset", default=DEFAULT_BUILD_PRESET)
    run.add_argument("--engine", type=Path, default=DEFAULT_ENGINE)
    run.add_argument("--skip-build", action="store_true", help="do not configure/build the stats preset before running")

    compare = subparsers.add_parser("compare", help="compare two saved search experiment run directories")
    compare.add_argument("baseline_run_dir", type=Path)
    compare.add_argument("candidate_run_dir", type=Path)
    compare.add_argument("--output", type=Path, help="comparison note path (default: candidate/comparison-vs-baseline.md)")
    return parser.parse_args()


def run_command(command: list[str], *, cwd: Path) -> str:
    completed = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed ({' '.join(command)}):\n{completed.stdout}")
    return completed.stdout.strip()


def git_text(repo: Path, args: list[str]) -> str:
    try:
        return run_command(["git", *args], cwd=repo)
    except RuntimeError:
        return ""


def git_dirty(repo: Path) -> bool:
    return bool(git_text(repo, ["status", "--short"]))


def slugify(label: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", label.strip()).strip("-")
    return slug or "run"


def effective_config(args: argparse.Namespace) -> dict[str, object]:
    profile = dict(PROFILES[args.profile])
    for name in ("positions", "threads", "movetime", "repeats", "hash"):
        value = getattr(args, name)
        if value is not None:
            profile[name] = value
    if int(profile["repeats"]) <= 0:
        raise ValueError("--repeats must be positive")
    if int(profile["movetime"]) <= 0:
        raise ValueError("--movetime must be positive")
    if int(profile["hash"]) <= 0:
        raise ValueError("--hash must be positive")
    return profile


def make_run_dir(output_root: Path, label: str) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base = output_root / f"{timestamp}-{slugify(label)}"
    run_dir = base
    index = 2
    while run_dir.exists():
        run_dir = Path(f"{base}-{index}")
        index += 1
    run_dir.mkdir(parents=True)
    (run_dir / "raw").mkdir()
    return run_dir


def maybe_build(args: argparse.Namespace) -> None:
    if args.skip_build:
        return
    run_command(["cmake", "--preset", args.build_preset], cwd=args.repo)
    run_command(["cmake", "--build", "--preset", args.build_preset, "--target", "latrunculi"], cwd=args.repo)


def write_manifest(path: Path, manifest: dict[str, object]) -> None:
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def emit_results(path: Path, rows: Iterable[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=RESULT_COLUMNS, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({column: row.get(column, "") for column in RESULT_COLUMNS})


def num(value: str) -> float | None:
    if value == "" or value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def average(values: Iterable[float | None]) -> float | None:
    clean = [value for value in values if value is not None]
    return sum(clean) / len(clean) if clean else None


def format_num(value: float | None, *, digits: int = 1) -> str:
    if value is None or math.isnan(value):
        return ""
    if abs(value - round(value)) < 0.05:
        return str(int(round(value)))
    return f"{value:.{digits}f}"


def group_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str], list[dict[str, str]]]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[(row["position_id"], row["threads"])].append(row)
    return dict(sorted(grouped.items()))


def aggregate_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, float | str | None]]:
    grouped = group_rows(rows)
    result: dict[tuple[str, str], dict[str, float | str | None]] = {}
    for key, group in grouped.items():
        result[key] = {field: average(num(row.get(field, "")) for row in group) for field in COMPARE_FIELDS}
        result[key]["bestmove"] = ", ".join(sorted({row["bestmove"] for row in group if row.get("bestmove")}))
    return result


def render_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    grouped = aggregate_rows(rows)
    lines = [
        f"# Search experiment: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Engine: `{manifest['engine_path']}`",
        f"- Build preset: `{manifest['build_preset']}` (search stats enabled: `{manifest['search_stats_enabled']}`)",
        f"- Profile: `{manifest['profile']}`",
        f"- Positions: `{manifest['selected_positions']}`",
        f"- Threads: `{','.join(str(t) for t in manifest['threads'])}`",
        f"- Movetime: `{manifest['movetime_ms']} ms`; repeats: `{manifest['repeats']}`; hash: `{manifest['hash_mb']} MiB`",
        "",
        "## Averaged results by position/thread",
        "| Position | Threads | Depth | Nodes | NPS | Beta cutoffs | Early% | Late% | TTHit% | TTCut% | QNode% | EBF | Cumul | Bestmove(s) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|",
    ]
    for (position_id, threads), agg in grouped.items():
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
                position_id,
                threads,
                format_num(agg["depth"]),
                format_num(agg["nodes"]),
                format_num(agg["nps"]),
                format_num(agg["stats_beta_cutoffs"]),
                format_num(agg["stats_cutoff_early_pct"]),
                format_num(agg["stats_cutoff_late_pct"]),
                format_num(agg["stats_tt_hit_pct"]),
                format_num(agg["stats_tt_cutoff_pct"]),
                format_num(agg["stats_qnode_pct"]),
                format_num(agg["stats_ebf"]),
                format_num(agg["stats_cumulative_ebf"]),
                agg["bestmove"] or "",
            )
        )
    lines.extend([
        "",
        "## Files",
        "- `manifest.json`: metadata needed for safe comparisons.",
        "- `results.tsv`: stable schema `search-experiment-v1` with parsed aggregate stats columns.",
        "- `raw/*.log`: combined stdout/stderr engine output, including raw SearchStats diagnostics.",
    ])
    return "\n".join(lines).rstrip() + "\n"


def command_run(args: argparse.Namespace) -> int:
    config = effective_config(args)
    positions_file = args.positions_file.resolve()
    selected_positions = select_positions(load_positions(positions_file), str(config["positions"]))
    threads = parse_threads(str(config["threads"]))

    maybe_build(args)
    if not args.engine.exists():
        raise FileNotFoundError(f"engine not found: {args.engine}")

    run_dir = make_run_dir(args.output_root, args.label)
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "label": args.label,
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "run_dir": str(run_dir),
        "repo_path": str(args.repo.resolve()),
        "git_revision": git_text(args.repo, ["rev-parse", "HEAD"]),
        "git_dirty": git_dirty(args.repo),
        "engine_path": str(args.engine.resolve()),
        "build_preset": args.build_preset,
        "search_stats_enabled": args.build_preset == DEFAULT_BUILD_PRESET,
        "profile": args.profile,
        "positions_file": str(positions_file),
        "selected_positions": [position["id"] for position in selected_positions],
        "threads": threads,
        "movetime_ms": int(config["movetime"]),
        "hash_mb": int(config["hash"]),
        "repeats": int(config["repeats"]),
        "result_schema_version": SCHEMA_VERSION,
    }
    write_manifest(run_dir / "manifest.json", manifest)

    rows: list[dict[str, str]] = []
    for repeat in range(1, int(config["repeats"]) + 1):
        for position in selected_positions:
            for thread_count in threads:
                result = run_position(
                    args.engine,
                    position["fen"],
                    thread_count,
                    int(config["movetime"]),
                    int(config["hash"]),
                    capture_raw=True,
                )
                raw_name = f"{position['id']}-t{thread_count}-r{repeat}.log"
                (run_dir / "raw" / raw_name).write_text(result.pop("raw_output", "") + "\n", encoding="utf-8")
                result.pop("search_stats_raw", None)
                rows.append(
                    {
                        "schema_version": SCHEMA_VERSION,
                        "position_id": position["id"],
                        "repeat": str(repeat),
                        "threads": str(thread_count),
                        "movetime_ms": str(config["movetime"]),
                        "raw_output_file": f"raw/{raw_name}",
                        **result,
                    }
                )

    emit_results(run_dir / "results.tsv", rows)
    (run_dir / "summary.md").write_text(render_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def read_manifest(run_dir: Path) -> dict[str, object]:
    return json.loads((run_dir / "manifest.json").read_text(encoding="utf-8"))


def read_results(run_dir: Path) -> list[dict[str, str]]:
    with (run_dir / "results.tsv").open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames != RESULT_COLUMNS:
            raise ValueError(f"unexpected result columns in {run_dir / 'results.tsv'}: {reader.fieldnames}")
        return [dict(row) for row in reader]


def percent_delta(old: float | str | None, new: float | str | None) -> str:
    if not isinstance(old, (float, int)) or not isinstance(new, (float, int)) or old == 0:
        return ""
    return f"{((new - old) / old) * 100.0:+.1f}%"


def manifest_warnings(old: dict[str, object], new: dict[str, object]) -> list[str]:
    keys = [
        "selected_positions",
        "threads",
        "movetime_ms",
        "hash_mb",
        "repeats",
        "search_stats_enabled",
        "result_schema_version",
    ]
    warnings = []
    for key in keys:
        if old.get(key) != new.get(key):
            warnings.append(f"- `{key}` differs: baseline=`{old.get(key)}` candidate=`{new.get(key)}`")
    return warnings


def render_compare(old_dir: Path, new_dir: Path, output: Path) -> str:
    old_manifest = read_manifest(old_dir)
    new_manifest = read_manifest(new_dir)
    old_rows = read_results(old_dir)
    new_rows = read_results(new_dir)
    old = aggregate_rows(old_rows)
    new = aggregate_rows(new_rows)
    all_keys = sorted(set(old) | set(new))

    lines = [
        f"# Search experiment comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}` (`{old_manifest.get('git_revision')}`, dirty `{old_manifest.get('git_dirty')}`)",
        f"- Candidate: `{new_dir}` (`{new_manifest.get('git_revision')}`, dirty `{new_manifest.get('git_dirty')}`)",
        "",
    ]
    warnings = manifest_warnings(old_manifest, new_manifest)
    if warnings:
        lines.extend(["## Compatibility warnings", *warnings, ""])
    else:
        lines.extend(["## Compatibility warnings", "- None; compared manifest parameters match.", ""])

    lines.extend([
        "## Averaged deltas by position/thread",
        "| Position | Threads | Depth old→new | Nodes delta | NPS old→new | NPS delta | Beta cutoff delta | Early% delta | Late% delta | TTHit% delta | TTCut% delta | QNode% delta | EBF delta | Cumul delta | Bestmove old→new |",
        "|---|---:|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|",
    ])
    for key in all_keys:
        position_id, threads = key
        old_agg = old.get(key, {})
        new_agg = new.get(key, {})
        old_nps = old_agg.get("nps")
        new_nps = new_agg.get("nps")
        lines.append(
            "| {} | {} | {}→{} | {} | {}→{} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {}→{} |".format(
                position_id,
                threads,
                format_num(old_agg.get("depth")),
                format_num(new_agg.get("depth")),
                percent_delta(old_agg.get("nodes"), new_agg.get("nodes")),
                format_num(old_nps),
                format_num(new_nps),
                percent_delta(old_nps, new_nps),
                percent_delta(old_agg.get("stats_beta_cutoffs"), new_agg.get("stats_beta_cutoffs")),
                delta_points(old_agg.get("stats_cutoff_early_pct"), new_agg.get("stats_cutoff_early_pct")),
                delta_points(old_agg.get("stats_cutoff_late_pct"), new_agg.get("stats_cutoff_late_pct")),
                delta_points(old_agg.get("stats_tt_hit_pct"), new_agg.get("stats_tt_hit_pct")),
                delta_points(old_agg.get("stats_tt_cutoff_pct"), new_agg.get("stats_tt_cutoff_pct")),
                delta_points(old_agg.get("stats_qnode_pct"), new_agg.get("stats_qnode_pct")),
                delta_points(old_agg.get("stats_ebf"), new_agg.get("stats_ebf")),
                delta_points(old_agg.get("stats_cumulative_ebf"), new_agg.get("stats_cumulative_ebf")),
                old_agg.get("bestmove", ""),
                new_agg.get("bestmove", ""),
            )
        )
    lines.extend([
        "",
        "## Notes",
        "- Rows are matched and averaged by `position_id` and `threads`; repeats are averaged within each group.",
        "- Search-stat columns are parsed from the final SearchStats depth row for each direct-UCI run; raw diagnostics remain in each run's `raw/` directory.",
    ])
    content = "\n".join(lines).rstrip() + "\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(content, encoding="utf-8")
    return content


def delta_points(old: float | str | None, new: float | str | None) -> str:
    if not isinstance(old, (float, int)) or not isinstance(new, (float, int)):
        return ""
    return f"{new - old:+.1f}"


def command_compare(args: argparse.Namespace) -> int:
    old_dir = args.baseline_run_dir.resolve()
    new_dir = args.candidate_run_dir.resolve()
    output = args.output or (new_dir / f"comparison-vs-{old_dir.name}.md")
    render_compare(old_dir, new_dir, output)
    print(output)
    return 0


def main() -> int:
    args = parse_args()
    if args.command == "run":
        return command_run(args)
    return command_compare(args)


if __name__ == "__main__":
    raise SystemExit(main())
