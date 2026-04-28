#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import os
import pty
import re
import select
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterable

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
DEFAULT_ENGINE = REPO_ROOT / "build/release/bin/latrunculi"
DEFAULT_POSITIONS = REPO_ROOT / "bench/direct_uci_suite.tsv"
DEFAULT_THREADS = [1, 4]
DEFAULT_MOVETIME_MS = 1000
DEFAULT_HASH_MB = 64


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the practical direct-UCI benchmark suite and emit stable TSV rows."
    )
    parser.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, help="engine binary to run")
    parser.add_argument(
        "--positions-file",
        type=Path,
        default=DEFAULT_POSITIONS,
        help="TSV file with fixed benchmark positions",
    )
    parser.add_argument(
        "--positions",
        default="all",
        help="comma-separated position ids from the suite (default: all)",
    )
    parser.add_argument(
        "--threads",
        default=",".join(str(value) for value in DEFAULT_THREADS),
        help="comma-separated thread counts to test",
    )
    parser.add_argument(
        "--movetime",
        type=int,
        default=DEFAULT_MOVETIME_MS,
        help="movetime in milliseconds for every run",
    )
    parser.add_argument("--hash", type=int, default=DEFAULT_HASH_MB, help="hash size in MiB")
    return parser.parse_args()


def load_positions(path: Path) -> list[dict[str, str]]:
    positions: list[dict[str, str]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle, delimiter="\t")
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if len(row) != 3:
                raise ValueError(f"expected 3 tab-separated fields in {path}, got: {row}")
            positions.append({"id": row[0], "fen": row[1], "description": row[2]})
    if not positions:
        raise ValueError(f"no benchmark positions found in {path}")
    return positions


def select_positions(positions: list[dict[str, str]], selection: str) -> list[dict[str, str]]:
    if selection == "all":
        return positions

    wanted = [item.strip() for item in selection.split(",") if item.strip()]
    wanted_set = set(wanted)
    chosen = [position for position in positions if position["id"] in wanted_set]
    missing = [position_id for position_id in wanted if position_id not in {entry['id'] for entry in chosen}]
    if missing:
        raise ValueError(f"unknown position ids: {', '.join(missing)}")
    return chosen


def parse_threads(raw: str) -> list[int]:
    values = [int(item.strip()) for item in raw.split(",") if item.strip()]
    if not values:
        raise ValueError("at least one thread count is required")
    if any(value <= 0 for value in values):
        raise ValueError("thread counts must be positive integers")
    return values


def parse_info_line(line: str) -> dict[str, str]:
    info: dict[str, str] = {}
    tokens = line.strip().split()
    index = 1  # skip leading 'info'
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


def parse_search_stats_lines(lines: list[str]) -> tuple[dict[str, str], str]:
    stats: dict[str, str] = {
        "stats_beta_cutoffs": "",
        "stats_cutoff_early_pct": "",
        "stats_cutoff_late_pct": "",
        "stats_tt_hit_pct": "",
        "stats_tt_cutoff_pct": "",
        "stats_qnode_pct": "",
        "stats_ebf": "",
        "stats_cumulative_ebf": "",
    }
    table_lines: list[str] = []
    table_rows: list[dict[str, str]] = []
    capture = False
    for line in lines:
        stripped = line.strip()
        if "Depth" in stripped and "Nodes (QNode%)" in stripped and "Cutoffs" in stripped:
            capture = True
            table_lines.append(line)
            continue
        if not capture:
            continue
        if not stripped:
            if table_lines:
                break
            continue
        table_lines.append(line)
        parts = [part.strip() for part in line.split("|")]
        if len(parts) != 6 or not parts[0].isdigit():
            continue
        row = parse_search_stats_row(parts)
        if row:
            table_rows.append(row)
    if table_rows:
        stats.update(aggregate_search_stats_rows(table_rows))
    return stats, "\n".join(table_lines).strip()


def aggregate_search_stats_rows(rows: list[dict[str, str]]) -> dict[str, str]:
    total_nodes = sum(int(row.get("stats_nodes", "0") or 0) for row in rows)
    total_cutoffs = sum(int(row.get("stats_beta_cutoffs", "0") or 0) for row in rows)
    total_qnodes = sum(float(row.get("stats_qnodes", "0") or 0.0) for row in rows)
    total_early = sum(float(row.get("stats_cutoff_early_count", "0") or 0.0) for row in rows)
    total_late = sum(float(row.get("stats_cutoff_late_count", "0") or 0.0) for row in rows)
    weighted_tt_hit = weighted_average(rows, "stats_tt_hit_pct", "stats_nodes")
    weighted_tt_cut = weighted_average(rows, "stats_tt_cutoff_pct", "stats_nodes")
    last = rows[-1]
    return {
        "stats_beta_cutoffs": str(total_cutoffs),
        "stats_cutoff_early_pct": f"{(100.0 * total_early / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_late_pct": f"{(100.0 * total_late / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_tt_hit_pct": f"{weighted_tt_hit:.1f}" if weighted_tt_hit is not None else "",
        "stats_tt_cutoff_pct": f"{weighted_tt_cut:.1f}" if weighted_tt_cut is not None else "",
        "stats_qnode_pct": f"{(100.0 * total_qnodes / total_nodes):.1f}" if total_nodes else "",
        "stats_ebf": last.get("stats_ebf", ""),
        "stats_cumulative_ebf": last.get("stats_cumulative_ebf", ""),
    }


def weighted_average(rows: list[dict[str, str]], value_key: str, weight_key: str) -> float | None:
    total_weight = 0.0
    total = 0.0
    for row in rows:
        try:
            value = float(row.get(value_key, ""))
            weight = float(row.get(weight_key, ""))
        except ValueError:
            continue
        total += value * weight
        total_weight += weight
    return total / total_weight if total_weight else None


def parse_search_stats_row(parts: list[str]) -> dict[str, str]:
    parsed: dict[str, str] = {}
    nodes_match = re.search(r"^(\d+)\s+\(\s*([-+0-9.]+)%\)", parts[1])
    cutoff_match = re.search(
        r"^(\d+)\s+\(\s*([-+0-9.]+)\s*/\s*([-+0-9.]+)%\)", parts[2]
    )
    pct_match = re.search(r"([-+0-9.]+)%", parts[3])
    ttcut_match = re.search(r"([-+0-9.]+)%", parts[4])
    ebf_match = re.search(r"([-+0-9.]+)\s*/\s*([-+0-9.]+)", parts[5])
    if nodes_match:
        nodes = int(nodes_match.group(1))
        qnode_pct = float(nodes_match.group(2))
        parsed["stats_nodes"] = str(nodes)
        parsed["stats_qnodes"] = f"{nodes * qnode_pct / 100.0:.3f}"
        parsed["stats_qnode_pct"] = nodes_match.group(2)
    if cutoff_match:
        cutoffs = int(cutoff_match.group(1))
        early_pct = float(cutoff_match.group(2))
        late_pct = float(cutoff_match.group(3))
        parsed["stats_beta_cutoffs"] = str(cutoffs)
        parsed["stats_cutoff_early_count"] = f"{cutoffs * early_pct / 100.0:.3f}"
        parsed["stats_cutoff_late_count"] = f"{cutoffs * late_pct / 100.0:.3f}"
        parsed["stats_cutoff_early_pct"] = cutoff_match.group(2)
        parsed["stats_cutoff_late_pct"] = cutoff_match.group(3)
    if pct_match:
        parsed["stats_tt_hit_pct"] = pct_match.group(1)
    if ttcut_match:
        parsed["stats_tt_cutoff_pct"] = ttcut_match.group(1)
    if ebf_match:
        parsed["stats_ebf"] = ebf_match.group(1)
        parsed["stats_cumulative_ebf"] = ebf_match.group(2)
    return parsed


def run_position(
    engine: Path,
    fen: str,
    threads: int,
    movetime: int,
    hash_mb: int,
    *,
    capture_raw: bool = False,
) -> dict[str, str]:
    command = [str(engine)]
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
            command,
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
            send(f"go movetime {movetime}")

            search_timeout = max(5.0, movetime / 1000.0 + 5.0)
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

    if capture_raw:
        stats, stats_raw = parse_search_stats_lines(raw_lines)
        result.update(stats)
        result["search_stats_raw"] = stats_raw
        result["raw_output"] = "\n".join(raw_lines).strip()

    return result



def emit_rows(rows: Iterable[dict[str, str]]) -> None:
    header = [
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
    writer = csv.DictWriter(sys.stdout, fieldnames=header, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    for row in rows:
        writer.writerow(row)


def main() -> int:
    args = parse_args()
    positions = select_positions(load_positions(args.positions_file), args.positions)
    threads = parse_threads(args.threads)

    if not args.engine.exists():
        raise FileNotFoundError(f"engine not found: {args.engine}")

    rows: list[dict[str, str]] = []
    for position in positions:
        for thread_count in threads:
            result = run_position(args.engine, position["fen"], thread_count, args.movetime, args.hash)
            rows.append(
                {
                    "position_id": position["id"],
                    "threads": str(thread_count),
                    "movetime_ms": str(args.movetime),
                    **result,
                }
            )

    emit_rows(rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
