from __future__ import annotations

import argparse
import os
import pty
import re
import select
import subprocess
import time
from collections import defaultdict
from pathlib import Path

from .common import (
    BENCH_DIR,
    add_common_run_args,
    average,
    base_manifest,
    build_binary,
    default_engine_path,
    format_num,
    make_run_dir,
    num,
    percent_delta,
    read_tsv,
    write_manifest,
    write_tsv,
)

SEARCH_FORMAT = "search"
DEFAULT_ARASAN20_EPD = BENCH_DIR / "arasan20.epd"
STARTPOS_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
SUITE_POSITION_IDS = [
    "startpos",
    "arasan20-01",
    "arasan20-08",
    "arasan20-16",
    "arasan20-21",
    "arasan20-30",
]

SEARCH_HASH_MB = 64
SEARCH_REPEATS = 1

SEARCH_COLUMNS = [
    "result_format",
    "position_id",
    "repeat",
    "threads",
    "depth_target",
    "hash_mb",
    "depth",
    "seldepth",
    "score_type",
    "score_value",
    "nodes",
    "time_ms",
    "nps",
    "bestmove",
    "pv",
    "raw_output_file",
]


def add_search_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("search", help="run fixed-depth search benchmark")
    add_common_run_args(parser)
    parser.add_argument("--depth", type=int, required=True)
    parser.add_argument("--threads", type=int, default=1)


def command_run_search(args: argparse.Namespace) -> int:
    build_binary(args, "latrunculi")
    engine = default_engine_path(args.build_preset)
    if not engine.exists():
        raise FileNotFoundError(f"engine binary not found: {engine}")
    if args.depth <= 0:
        raise ValueError("--depth must be positive")
    if args.threads <= 0:
        raise ValueError("--threads must be positive")

    epd_file = DEFAULT_ARASAN20_EPD.resolve()
    selected_positions = select_positions(load_benchmark_positions(epd_file), "suite")
    timeout = max(120.0, float(args.depth) * 60.0)

    run_dir = make_run_dir(args.output_root, args.label)
    manifest = base_manifest(args, run_dir, SEARCH_FORMAT)
    manifest.update(
        {
            "engine_path": str(engine.resolve()),
            "epd_file": str(epd_file),
            "selected_positions": [position["id"] for position in selected_positions],
            "threads": args.threads,
            "depth_target": args.depth,
            "hash_mb": SEARCH_HASH_MB,
            "repeats": SEARCH_REPEATS,
            "timeout_seconds": timeout,
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)

    rows: list[dict[str, str]] = []
    for repeat in range(1, SEARCH_REPEATS + 1):
        for position in selected_positions:
            result = run_position(
                engine,
                position["fen"],
                SEARCH_HASH_MB,
                depth=args.depth,
                threads=args.threads,
                search_timeout=timeout,
            )
            raw_name = f"{position['id']}-t{args.threads}-d{args.depth}-r{repeat}.log"
            (run_dir / "raw" / raw_name).write_text(result.pop("raw_output", "") + "\n", encoding="utf-8")
            rows.append(
                {
                    "result_format": SEARCH_FORMAT,
                    "position_id": position["id"],
                    "repeat": str(repeat),
                    "threads": str(args.threads),
                    "depth_target": str(args.depth),
                    "hash_mb": str(SEARCH_HASH_MB),
                    "raw_output_file": f"raw/{raw_name}",
                    **result,
                }
            )

    write_tsv(run_dir / "results.tsv", rows, SEARCH_COLUMNS)
    (run_dir / "summary.md").write_text(render_search_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def load_benchmark_positions(epd_file: Path) -> list[dict[str, str]]:
    positions = [
        {
            "id": "startpos",
            "fen": STARTPOS_FEN,
            "description": "Standard starting position",
        }
    ]
    with epd_file.open(encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) < 4:
                raise ValueError(f"expected EPD fields in {epd_file}:{line_number}")
            epd_id = extract_epd_field(line, "id")
            if not epd_id:
                raise ValueError(f"missing id field in {epd_file}:{line_number}")
            positions.append(
                {
                    "id": normalize_arasan_id(epd_id),
                    "fen": " ".join(fields[:4]) + " 0 1",
                    "description": extract_epd_field(line, "c0") or epd_id,
                }
            )
    return positions


def extract_epd_field(line: str, field: str) -> str:
    match = re.search(rf'\b{re.escape(field)}\s+"([^"]*)"', line)
    return match.group(1) if match else ""


def normalize_arasan_id(epd_id: str) -> str:
    match = re.fullmatch(r"arasan20\.(\d+)", epd_id)
    if match:
        return f"arasan20-{int(match.group(1)):02d}"
    return epd_id.replace(".", "-")


def select_positions(positions: list[dict[str, str]], selection: str) -> list[dict[str, str]]:
    if selection == "suite":
        return positions_by_id(positions, SUITE_POSITION_IDS)
    wanted = [item.strip() for item in selection.split(",") if item.strip()]
    return positions_by_id(positions, wanted)


def positions_by_id(positions: list[dict[str, str]], wanted: list[str]) -> list[dict[str, str]]:
    wanted_set = set(wanted)
    chosen = [position for position in positions if position["id"] in wanted_set]
    known = {entry["id"] for entry in chosen}
    missing = [position_id for position_id in wanted if position_id not in known]
    if missing:
        raise ValueError(f"unknown position ids: {', '.join(missing)}")
    return chosen


def parse_info_line(line: str) -> dict[str, str]:
    info: dict[str, str] = {}
    tokens = line.strip().split()
    index = 1
    while index < len(tokens):
        token = tokens[index]
        if token in {"depth", "seldepth", "nodes", "time", "nps"} and index + 1 < len(tokens):
            info[token] = tokens[index + 1]
            index += 2
            continue
        if token == "score" and index + 2 < len(tokens):
            info["score_type"] = tokens[index + 1]
            info["score_value"] = tokens[index + 2]
            index += 3
            continue
        if token == "pv":
            info["pv"] = " ".join(tokens[index + 1 :])
            break
        index += 1
    return info


def run_position(
    engine: Path,
    fen: str,
    hash_mb: int,
    *,
    depth: int,
    threads: int,
    search_timeout: float,
) -> dict[str, str]:
    result = {
        "depth": "",
        "seldepth": "",
        "score_type": "",
        "score_value": "",
        "nodes": "",
        "time_ms": "",
        "nps": "",
        "bestmove": "",
        "pv": "",
    }
    master_fd, slave_fd = pty.openpty()
    pending_output = ""
    raw_lines: list[str] = []

    def read_line(timeout_seconds: float) -> str:
        nonlocal pending_output
        deadline = time.monotonic() + timeout_seconds
        while True:
            if "\n" in pending_output:
                line, pending_output = pending_output.split("\n", 1)
                return line.rstrip("\r")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out waiting for engine output")
            ready, _, _ = select.select([master_fd], [], [], remaining)
            if not ready:
                continue
            try:
                chunk = os.read(master_fd, 4096)
            except OSError as exc:
                if pending_output:
                    line, pending_output = pending_output, ""
                    return line.rstrip("\r")
                raise RuntimeError("engine output closed unexpectedly") from exc
            if not chunk:
                if pending_output:
                    line, pending_output = pending_output, ""
                    return line.rstrip("\r")
                raise RuntimeError("engine produced no more output")
            pending_output += chunk.decode("utf-8", errors="replace")

    try:
        with subprocess.Popen(
            [str(engine)],
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            text=False,
            close_fds=True,
        ) as process:
            os.close(slave_fd)
            slave_fd = -1

            def send(command_text: str) -> None:
                os.write(master_fd, (command_text + "\n").encode("utf-8"))

            send("uci")
            send(f"setoption name Threads value {threads}")
            send(f"setoption name Hash value {hash_mb}")
            send("isready")

            saw_uciok = False
            saw_readyok = False
            while True:
                stripped = read_line(timeout_seconds=5)
                raw_lines.append(stripped)
                if stripped == "uciok":
                    saw_uciok = True
                elif stripped == "readyok":
                    saw_readyok = True
                if saw_uciok and saw_readyok:
                    break

            send("ucinewgame")
            send(f"position fen {fen}")
            send(f"go depth {depth}")
            while True:
                stripped = read_line(timeout_seconds=search_timeout)
                raw_lines.append(stripped)
                if stripped.startswith("info "):
                    info = parse_info_line(stripped)
                    if info.get("depth"):
                        result["depth"] = info.get("depth", result["depth"])
                        result["seldepth"] = info.get("seldepth", result["seldepth"])
                        result["score_type"] = info.get("score_type", result["score_type"])
                        result["score_value"] = info.get("score_value", result["score_value"])
                        result["nodes"] = info.get("nodes", result["nodes"])
                        result["time_ms"] = info.get("time", result["time_ms"])
                        result["nps"] = info.get("nps", result["nps"])
                        result["pv"] = info.get("pv", result["pv"])
                elif stripped.startswith("bestmove "):
                    result["bestmove"] = stripped.split()[1]
                    break

            send("quit")
            drain_deadline = time.monotonic() + 2.0
            while process.poll() is None or ("\n" in pending_output and time.monotonic() < drain_deadline):
                try:
                    stripped = read_line(timeout_seconds=0.2)
                except TimeoutError:
                    continue
                except RuntimeError:
                    break
                raw_lines.append(stripped)
            process.wait(timeout=search_timeout)
    finally:
        if slave_fd != -1:
            os.close(slave_fd)
        os.close(master_fd)

    if not result["bestmove"]:
        raise RuntimeError(f"engine produced no bestmove for fen: {fen}")
    result["raw_output"] = "\n".join(raw_lines).strip()
    return result


def aggregate_search_rows(rows: list[dict[str, str]]) -> dict[str, dict[str, float | str | None]]:
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row["position_id"]].append(row)

    result: dict[str, dict[str, float | str | None]] = {}
    for position_id, group in grouped.items():
        result[position_id] = {
            "depth": average(num(row.get("depth")) for row in group),
            "nodes": average(num(row.get("nodes")) for row in group),
            "time_ms": average(num(row.get("time_ms")) for row in group),
            "nps": average(num(row.get("nps")) for row in group),
            "score": unique_join(format_score(row) for row in group),
            "bestmove": unique_join(row.get("bestmove", "") for row in group),
        }
    return result


def format_score(row: dict[str, str]) -> str:
    score_type = row.get("score_type", "")
    score_value = row.get("score_value", "")
    return f"{score_type} {score_value}".strip()


def unique_join(values: object) -> str:
    seen: list[str] = []
    for value in values:
        if value and value not in seen:
            seen.append(value)
    return ", ".join(seen)


def render_search_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    grouped = aggregate_search_rows(rows)
    lines = [
        f"# Search benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Engine: `{manifest['engine_path']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Positions: `{manifest['selected_positions']}`",
        f"- Depth: `{manifest['depth_target']}`; threads: `{manifest['threads']}`; hash: `{manifest['hash_mb']} MiB`",
        "",
        "## Averaged results",
        "| Position | Depth | Nodes | Time ms | NPS | Score(s) | Bestmove(s) |",
        "|---|---:|---:|---:|---:|---|---|",
    ]
    for position_id, agg in grouped.items():
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} |".format(
                position_id,
                format_num(agg["depth"]),
                format_num(agg["nodes"]),
                format_num(agg["time_ms"]),
                format_num(agg["nps"]),
                agg["score"],
                agg["bestmove"],
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def render_search_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv")
    _, new_rows = read_tsv(new_dir / "results.tsv")
    old = aggregate_search_rows(old_rows)
    new = aggregate_search_rows(new_rows)
    keys = sorted(set(old) | set(new))
    lines = [
        f"# Search comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        "",
        "## Averaged deltas",
        "| Position | Depth old/new | Nodes delta | Time old/new | Time delta | NPS old/new | NPS delta | Score old/new | Bestmove old/new |",
        "|---|---|---:|---|---:|---|---:|---|---|",
    ]
    for key in keys:
        old_agg = old.get(key, {})
        new_agg = new.get(key, {})
        lines.append(
            "| {} | {} / {} | {} | {} / {} | {} | {} / {} | {} | {} / {} | {} / {} |".format(
                key,
                format_num(old_agg.get("depth")),
                format_num(new_agg.get("depth")),
                percent_delta(old_agg.get("nodes"), new_agg.get("nodes")),
                format_num(old_agg.get("time_ms")),
                format_num(new_agg.get("time_ms")),
                percent_delta(old_agg.get("time_ms"), new_agg.get("time_ms")),
                format_num(old_agg.get("nps")),
                format_num(new_agg.get("nps")),
                percent_delta(old_agg.get("nps"), new_agg.get("nps")),
                old_agg.get("score", ""),
                new_agg.get("score", ""),
                old_agg.get("bestmove", ""),
                new_agg.get("bestmove", ""),
            )
        )
    return "\n".join(lines).rstrip() + "\n"
