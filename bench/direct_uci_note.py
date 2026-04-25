#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path
from typing import Iterable

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_POSITIONS = REPO_ROOT / "bench/direct_uci_suite.tsv"

CURRENT_COLUMNS = [
    "position_id",
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
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate lightweight benchmark notes from direct-UCI TSV output."
    )
    subparsers = parser.add_subparsers(dest="mode", required=True)

    current = subparsers.add_parser(
        "current",
        help="render a current-version benchmark note from one TSV file",
    )
    add_shared_arguments(current)
    current.add_argument("--tsv", type=Path, required=True, help="TSV output from direct_uci_bench.py")
    current.add_argument(
        "--label",
        default="current-version run",
        help="human-readable label for the run",
    )

    compare = subparsers.add_parser(
        "compare",
        help="render an old-vs-new comparison note from two TSV files",
    )
    add_shared_arguments(compare)
    compare.add_argument("--old-tsv", type=Path, required=True, help="baseline TSV output")
    compare.add_argument("--new-tsv", type=Path, required=True, help="candidate TSV output")
    compare.add_argument("--old-label", default="old", help="label for the baseline TSV")
    compare.add_argument("--new-label", default="new", help="label for the candidate TSV")
    compare.add_argument(
        "--old-repo",
        type=Path,
        help="baseline repository root; defaults to --repo when omitted",
    )
    compare.add_argument(
        "--new-repo",
        type=Path,
        help="candidate repository root; defaults to --repo when omitted",
    )
    compare.add_argument(
        "--old-engine",
        help="baseline engine path/identity; defaults to --engine when omitted",
    )
    compare.add_argument(
        "--new-engine",
        help="candidate engine path/identity; defaults to --engine when omitted",
    )
    compare.add_argument(
        "--old-revision",
        default="",
        help="baseline revision label/ID to record in the note",
    )
    compare.add_argument(
        "--new-revision",
        default="",
        help="candidate revision label/ID to record in the note",
    )
    compare.add_argument(
        "--label",
        default="before-vs-after comparison",
        help="human-readable label for the comparison note",
    )

    return parser.parse_args()


def add_shared_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--repo",
        type=Path,
        default=REPO_ROOT,
        help="repository root used for the run",
    )
    parser.add_argument(
        "--positions-file",
        type=Path,
        default=DEFAULT_POSITIONS,
        help="fixed direct-UCI suite TSV file",
    )
    parser.add_argument(
        "--engine",
        default="./build/release/bin/latrunculi",
        help="engine path used for current-version runs",
    )
    parser.add_argument(
        "--threads",
        default="",
        help="comma-separated thread counts used for the run; inferred from TSV when omitted",
    )
    parser.add_argument(
        "--movetime",
        type=int,
        default=None,
        help="movetime in milliseconds; inferred from TSV when omitted",
    )
    parser.add_argument(
        "--hash",
        type=int,
        default=None,
        help="hash size in MiB if you want it recorded explicitly",
    )
    parser.add_argument(
        "--positions",
        default="",
        help="comma-separated position ids used for the run; inferred from TSV when omitted",
    )
    parser.add_argument(
        "--purpose",
        default="practical direct-UCI benchmark note",
        help="short purpose line near the top of the note",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="write the note to this path instead of stdout",
    )


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames != CURRENT_COLUMNS:
            raise ValueError(f"unexpected TSV columns in {path}: {reader.fieldnames}")
        rows = [dict(row) for row in reader]
    if not rows:
        raise ValueError(f"no benchmark rows found in {path}")
    return rows


def sorted_unique(rows: Iterable[dict[str, str]], key: str) -> list[str]:
    values = {row[key] for row in rows if row.get(key, "")}
    if key == "threads":
        return [str(value) for value in sorted(int(item) for item in values)]
    return sorted(values)


def infer_movetime(rows: list[dict[str, str]]) -> str:
    values = sorted({row["movetime_ms"] for row in rows if row.get("movetime_ms", "")})
    return ", ".join(values)


def effective_positions(args: argparse.Namespace, rows: list[dict[str, str]]) -> str:
    return args.positions or ", ".join(sorted_unique(rows, "position_id"))


def effective_threads(args: argparse.Namespace, rows: list[dict[str, str]]) -> str:
    return args.threads or ",".join(sorted_unique(rows, "threads"))


def effective_movetime(args: argparse.Namespace, rows: list[dict[str, str]]) -> str:
    return str(args.movetime) if args.movetime is not None else infer_movetime(rows)


def group_by_position(rows: list[dict[str, str]]) -> dict[str, list[dict[str, str]]]:
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row["position_id"]].append(row)
    for position_rows in grouped.values():
        position_rows.sort(key=lambda row: int(row["threads"]))
    return dict(sorted(grouped.items()))


def format_capture_command(
    *,
    engine: str,
    positions_file: Path,
    positions: str,
    threads: str,
    movetime: str,
    hash_value: int | None,
    output_name: str,
) -> str:
    pieces = [
        "python3 bench/direct_uci_bench.py",
        f"--engine {shell_quote(engine)}",
        f"--positions-file {shell_quote(str(positions_file))}",
        f"--positions {shell_quote(positions)}",
        f"--threads {shell_quote(threads)}",
        f"--movetime {movetime}",
    ]
    if hash_value is not None:
        pieces.append(f"--hash {hash_value}")
    pieces.append(f"> {output_name}")
    return " \\\n  ".join(pieces)


def format_replay_block(args: argparse.Namespace, *, rows: list[dict[str, str]], note_mode: str) -> str:
    positions = effective_positions(args, rows)
    threads = effective_threads(args, rows)
    movetime = effective_movetime(args, rows)
    note_cmd = [
        f"python3 bench/direct_uci_note.py {note_mode}",
        "--repo " + shell_quote(str(args.repo)),
        "--positions-file " + shell_quote(str(args.positions_file)),
    ]
    if note_mode == "current":
        capture_name = "<capture.tsv>"
        capture_cmd = format_capture_command(
            engine=str(args.engine),
            positions_file=args.positions_file,
            positions=positions,
            threads=threads,
            movetime=movetime,
            hash_value=args.hash,
            output_name=capture_name,
        )
        note_cmd.extend(
            [
                "--engine " + shell_quote(str(args.engine)),
                f"--tsv {capture_name}",
                "--output <note.md>",
            ]
        )
        return "```bash\n" + capture_cmd + "\n\n" + " \\\n  ".join(note_cmd) + "\n```"

    old_repo = args.old_repo or args.repo
    new_repo = args.new_repo or args.repo
    old_engine = args.old_engine or str(args.engine)
    new_engine = args.new_engine or str(args.engine)
    old_capture_name = "<old.tsv>"
    new_capture_name = "<new.tsv>"
    old_capture_cmd = format_capture_command(
        engine=old_engine,
        positions_file=args.positions_file,
        positions=positions,
        threads=threads,
        movetime=movetime,
        hash_value=args.hash,
        output_name=old_capture_name,
    )
    new_capture_cmd = format_capture_command(
        engine=new_engine,
        positions_file=args.positions_file,
        positions=positions,
        threads=threads,
        movetime=movetime,
        hash_value=args.hash,
        output_name=new_capture_name,
    )
    note_cmd.extend(
        [
            "--old-tsv " + old_capture_name,
            "--new-tsv " + new_capture_name,
            f"--old-label {shell_quote(args.old_label)}",
            f"--new-label {shell_quote(args.new_label)}",
            "--old-repo " + shell_quote(str(old_repo)),
            "--new-repo " + shell_quote(str(new_repo)),
            "--old-engine " + shell_quote(old_engine),
            "--new-engine " + shell_quote(new_engine),
            "--output <note.md>",
        ]
    )
    if args.old_revision:
        note_cmd.append(f"--old-revision {shell_quote(args.old_revision)}")
    if args.new_revision:
        note_cmd.append(f"--new-revision {shell_quote(args.new_revision)}")
    return "```bash\n" + old_capture_cmd + "\n\n" + new_capture_cmd + "\n\n" + " \\\n  ".join(note_cmd) + "\n```"


def shell_quote(text: str) -> str:
    if not text:
        return "''"
    if all(ch.isalnum() or ch in "._/-" for ch in text):
        return text
    return "'" + text.replace("'", "'\"'\"'") + "'"


def format_revision(label: str, revision: str) -> str:
    if revision:
        return f"`{revision}` ({label})"
    return f"{label} (revision not recorded)"


def ensure_matching_movetime(old_rows: list[dict[str, str]], new_rows: list[dict[str, str]]) -> str:
    old_movetime = infer_movetime(old_rows)
    new_movetime = infer_movetime(new_rows)
    if old_movetime != new_movetime:
        raise ValueError(
            "comparison TSVs must use the same movetime; "
            f"old={old_movetime}, new={new_movetime}"
        )
    return new_movetime


def int_or_blank(value: str) -> int | None:
    return int(value) if value else None


def nps_scaling_summary(rows: list[dict[str, str]]) -> str | None:
    by_threads = {int(row["threads"]): int_or_blank(row["nps"]) for row in rows}
    base = by_threads.get(1)
    if not base:
        return None
    pieces = []
    for threads in sorted(by_threads):
        if threads == 1 or by_threads[threads] is None:
            continue
        scale = by_threads[threads] / base
        pieces.append(f"{threads}t={scale:.2f}x")
    if not pieces:
        return None
    return ", ".join(pieces) + " relative to 1t NPS"


def render_current(args: argparse.Namespace) -> str:
    rows = read_tsv(args.tsv)
    positions = effective_positions(args, rows)
    threads = effective_threads(args, rows)
    movetime = effective_movetime(args, rows)
    lines = [
        f"# Benchmark note: {args.label}",
        "",
        "## Scope",
        f"- Note type: current-version run",
        f"- Purpose: {args.purpose}",
        f"- Repo: `{args.repo}`",
        f"- Engine: `{args.engine}`",
        f"- Direct-UCI TSV capture: `{args.tsv}`",
        "",
        "## Replay procedure",
        f"- Positions file: `{args.positions_file}`",
        f"- Selected positions: `{positions}`",
        f"- Threads: `{threads}`",
        f"- Movetime: `{movetime} ms`",
    ]
    if args.hash is not None:
        lines.append(f"- Hash: `{args.hash} MiB`")
    lines.extend(
        [
            "- Source of truth: direct scripted UCI runs from `bench/direct_uci_bench.py`",
            "",
            format_replay_block(args, rows=rows, note_mode="current"),
            "",
            "## Results",
        ]
    )

    for position_id, position_rows in group_by_position(rows).items():
        lines.extend(
            [
                f"### {position_id}",
                "| Threads | Depth | Seldepth | Score | Nodes | Time (ms) | NPS | Bestmove | PV |",
                "|---:|---:|---:|---|---:|---:|---:|---|---|",
            ]
        )
        for row in position_rows:
            score = ""
            if row["score_type"] and row["score_value"]:
                score = f"{row['score_type']} {row['score_value']}"
            lines.append(
                f"| {row['threads']} | {row['depth']} | {row['seldepth']} | {score} | {row['nodes']} | {row['time_ms']} | {row['nps']} | {row['bestmove']} | {row['pv']} |"
            )
        scaling = nps_scaling_summary(position_rows)
        if scaling:
            lines.append("")
            lines.append(f"- Scaling summary: {scaling}.")
        lines.append("")

    lines.extend(
        [
            "## Rerun checklist",
            "- Reuse the same positions file, selected positions, thread counts, movetime, and hash setting.",
            "- Capture the TSV columns exactly as emitted by `bench/direct_uci_bench.py`.",
            "- Keep old/new comparison notes on the same procedure before drawing conclusions from NPS or PV differences.",
        ]
    )
    return "\n".join(lines).rstrip() + "\n"


def rows_by_key(rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, str]]:
    keyed: dict[tuple[str, str], dict[str, str]] = {}
    for row in rows:
        key = (row["position_id"], row["threads"])
        if key in keyed:
            raise ValueError(f"duplicate row for position/thread pair: {key}")
        keyed[key] = row
    return keyed


def percent_delta(old_value: int | None, new_value: int | None) -> str:
    if old_value in (None, 0) or new_value is None:
        return ""
    delta = ((new_value - old_value) / old_value) * 100.0
    return f"{delta:+.1f}%"


def render_compare(args: argparse.Namespace) -> str:
    old_rows = read_tsv(args.old_tsv)
    new_rows = read_tsv(args.new_tsv)
    old_keyed = rows_by_key(old_rows)
    new_keyed = rows_by_key(new_rows)

    if set(old_keyed) != set(new_keyed):
        missing_old = sorted(set(new_keyed) - set(old_keyed))
        missing_new = sorted(set(old_keyed) - set(new_keyed))
        raise ValueError(
            "comparison TSVs must cover the same position/thread pairs; "
            f"missing from old={missing_old}, missing from new={missing_new}"
        )

    positions = effective_positions(args, new_rows)
    threads = effective_threads(args, new_rows)
    movetime = str(args.movetime) if args.movetime is not None else ensure_matching_movetime(old_rows, new_rows)
    old_repo = args.old_repo or args.repo
    new_repo = args.new_repo or args.repo
    old_engine = args.old_engine or str(args.engine)
    new_engine = args.new_engine or str(args.engine)

    lines = [
        f"# Benchmark note: {args.label}",
        "",
        "## Scope",
        "- Note type: old-vs-new comparison run",
        f"- Purpose: {args.purpose}",
        f"- Repo: `{args.repo}`",
        f"- Baseline repo: `{old_repo}`",
        f"- Baseline engine: `{old_engine}`",
        f"- Baseline revision: {format_revision(args.old_label, args.old_revision)}",
        f"- Baseline TSV: `{args.old_tsv}` ({args.old_label})",
        f"- Candidate repo: `{new_repo}`",
        f"- Candidate engine: `{new_engine}`",
        f"- Candidate revision: {format_revision(args.new_label, args.new_revision)}",
        f"- Candidate TSV: `{args.new_tsv}` ({args.new_label})",
        "",
        "## Replay procedure",
        f"- Positions file: `{args.positions_file}`",
        f"- Selected positions: `{positions}`",
        f"- Threads: `{threads}`",
        f"- Movetime: `{movetime} ms`",
    ]
    if args.hash is not None:
        lines.append(f"- Hash: `{args.hash} MiB`")
    lines.extend(
        [
            "- Compare only runs collected with the same direct-UCI procedure and TSV columns.",
            "",
            format_replay_block(args, rows=new_rows, note_mode="compare"),
            "",
            "## Comparison",
        ]
    )

    grouped_old = group_by_position(old_rows)
    grouped_new = group_by_position(new_rows)
    for position_id in sorted(grouped_new):
        lines.extend(
            [
                f"### {position_id}",
                f"| Threads | {args.old_label} depth | {args.new_label} depth | {args.old_label} NPS | {args.new_label} NPS | NPS delta | {args.old_label} bestmove | {args.new_label} bestmove | {args.old_label} PV | {args.new_label} PV |",
                "|---:|---:|---:|---:|---:|---:|---|---|---|---|",
            ]
        )
        for old_row, new_row in zip(grouped_old[position_id], grouped_new[position_id], strict=True):
            old_nps = int_or_blank(old_row["nps"])
            new_nps = int_or_blank(new_row["nps"])
            lines.append(
                f"| {old_row['threads']} | {old_row['depth']} | {new_row['depth']} | {old_row['nps']} | {new_row['nps']} | {percent_delta(old_nps, new_nps)} | {old_row['bestmove']} | {new_row['bestmove']} | {old_row['pv']} | {new_row['pv']} |"
            )
        old_scaling = nps_scaling_summary(grouped_old[position_id])
        new_scaling = nps_scaling_summary(grouped_new[position_id])
        lines.append("")
        if old_scaling:
            lines.append(f"- {args.old_label} scaling: {old_scaling}.")
        if new_scaling:
            lines.append(f"- {args.new_label} scaling: {new_scaling}.")
        if any(
            old_row["bestmove"] != new_row["bestmove"]
            for old_row, new_row in zip(grouped_old[position_id], grouped_new[position_id], strict=True)
        ):
            lines.append("- Root move changed for at least one sampled thread count; inspect PVs before treating the NPS delta as the whole story.")
        lines.append("")

    lines.extend(
        [
            "## Rerun checklist",
            "- Re-run both revisions with the same positions file, selected positions, thread counts, movetime, and hash setting.",
            "- Keep the raw TSV captures alongside the comparison note so future agents can diff the exact rows.",
            "- Treat these notes as reproducible short-run evidence, not as Elo/SPRT proof.",
        ]
    )
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    args = parse_args()
    if args.mode == "current":
        content = render_current(args)
    else:
        content = render_compare(args)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
    else:
        print(content, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
