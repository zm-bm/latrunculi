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
    DEFAULT_CPP_BUILD_PRESET,
    DEFAULT_DIRECT_BUILD_PRESET,
    add_common_run_args,
    average,
    base_manifest,
    build_binary,
    default_engine_path,
    delta_points,
    format_num,
    make_run_dir,
    num,
    percent_delta,
    read_tsv,
    write_manifest,
    write_tsv,
)

DIRECT_UCI_FORMAT = "direct-uci"
FIXED_DEPTH_UCI_FORMAT = "fixed-depth-uci"
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

DIRECT_PROFILES = {
    "smoke": {"positions": "startpos,arasan20-01", "threads": "1,4", "movetime": 500, "repeats": 1, "hash": 64},
    "mid": {"positions": "suite", "threads": "1,4", "movetime": 1500, "repeats": 3, "hash": 64},
    "wide": {"positions": "suite", "threads": "1,2,4,8", "movetime": 2000, "repeats": 3, "hash": 64},
}

STATS_COLUMNS = [
    "stats_beta_cutoffs",
    "stats_cutoff_early_pct",
    "stats_cutoff_late_pct",
    "stats_cutoff_avg_index",
    "stats_cutoff_1_pct",
    "stats_cutoff_2_pct",
    "stats_cutoff_3_4_pct",
    "stats_cutoff_5_plus_pct",
    "stats_pvs_researches",
    "stats_aspiration_fail_low",
    "stats_aspiration_fail_high",
    "stats_aspiration_researches",
    "stats_main_tt_hit_pct",
    "stats_main_tt_cutoff_pct",
    "stats_q_tt_hit_pct",
    "stats_q_tt_cutoff_pct",
    "stats_qnode_pct",
    "stats_ebf",
    "stats_cumulative_ebf",
]

DIRECT_RESULT_COLUMNS = [
    "result_format",
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
    *STATS_COLUMNS,
    "raw_output_file",
]

FIXED_DEPTH_RESULT_COLUMNS = [
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
    *STATS_COLUMNS,
    "raw_output_file",
]


def add_direct_uci_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("direct-uci", help="run stats-enabled direct-UCI benchmark suite")
    add_common_run_args(parser, build_preset=DEFAULT_DIRECT_BUILD_PRESET)
    parser.add_argument("--profile", choices=sorted(DIRECT_PROFILES), default="mid")
    parser.add_argument("--positions", help="suite, arasan20-full, or comma-separated position ids")
    parser.add_argument("--threads", help="comma-separated thread counts")
    parser.add_argument("--movetime", type=int)
    parser.add_argument("--repeats", type=int)
    parser.add_argument("--hash", type=int)
    parser.add_argument("--epd-file", type=Path, default=DEFAULT_ARASAN20_EPD)
    parser.add_argument("--engine", type=Path, help="engine binary path")


def add_fixed_depth_uci_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("fixed-depth-uci", help="run fixed-depth UCI search positions")
    add_common_run_args(parser, build_preset=DEFAULT_CPP_BUILD_PRESET)
    parser.add_argument("--positions", default="suite", help="suite, arasan20-full, or comma-separated position ids")
    parser.add_argument("--threads", default="1", help="comma-separated thread counts")
    parser.add_argument("--depth", type=int, required=True)
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--hash", type=int, default=64)
    parser.add_argument("--timeout", type=float, default=300.0, help="per-position timeout in seconds")
    parser.add_argument("--epd-file", type=Path, default=DEFAULT_ARASAN20_EPD)
    parser.add_argument("--engine", type=Path, help="engine binary path")


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
    if selection == "all":
        raise ValueError("position selector 'all' is ambiguous; use 'suite' or 'arasan20-full'")
    if selection == "suite":
        return positions_by_id(positions, SUITE_POSITION_IDS)
    if selection == "arasan20-full":
        return [position for position in positions if position["id"].startswith("arasan20-")]
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


def parse_threads(raw: str) -> list[int]:
    values = [int(item.strip()) for item in raw.split(",") if item.strip()]
    if not values:
        raise ValueError("at least one thread count is required")
    if any(value <= 0 for value in values):
        raise ValueError("thread counts must be positive integers")
    return values


def effective_direct_config(args: argparse.Namespace) -> dict[str, object]:
    profile = dict(DIRECT_PROFILES[args.profile])
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


def command_run_direct_uci(args: argparse.Namespace) -> int:
    build_binary(args, "latrunculi")
    engine = args.engine or default_engine_path(args.build_preset)
    if not engine.exists():
        raise FileNotFoundError(f"engine binary not found: {engine}")

    config = effective_direct_config(args)
    epd_file = args.epd_file.resolve()
    selected_positions = select_positions(load_benchmark_positions(epd_file), str(config["positions"]))
    threads = parse_threads(str(config["threads"]))

    run_dir = make_run_dir(args.output_root, args.label)
    manifest = base_manifest(args, run_dir, DIRECT_UCI_FORMAT)
    manifest.update(
        {
            "engine_path": str(engine.resolve()),
            "epd_file": str(epd_file),
            "selected_positions": [position["id"] for position in selected_positions],
            "threads": threads,
            "movetime_ms": int(config["movetime"]),
            "hash_mb": int(config["hash"]),
            "repeats": int(config["repeats"]),
            "search_stats_enabled": args.build_preset in {"release-stats", "release-stats-dev"},
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)

    rows: list[dict[str, str]] = []
    for repeat in range(1, int(config["repeats"]) + 1):
        for position in selected_positions:
            for thread_count in threads:
                result = run_position(
                    engine,
                    position["fen"],
                    thread_count,
                    int(config["hash"]),
                    go_command=f"go movetime {int(config['movetime'])}",
                    search_timeout=max(5.0, int(config["movetime"]) / 1000.0 + 5.0),
                )
                raw_name = f"{position['id']}-t{thread_count}-r{repeat}.log"
                (run_dir / "raw" / raw_name).write_text(result.pop("raw_output", "") + "\n", encoding="utf-8")
                rows.append(
                    {
                        "result_format": DIRECT_UCI_FORMAT,
                        "position_id": position["id"],
                        "repeat": str(repeat),
                        "threads": str(thread_count),
                        "movetime_ms": str(config["movetime"]),
                        "raw_output_file": f"raw/{raw_name}",
                        **result,
                    }
                )

    write_tsv(run_dir / "results.tsv", rows, DIRECT_RESULT_COLUMNS)
    (run_dir / "summary.md").write_text(render_direct_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def command_run_fixed_depth_uci(args: argparse.Namespace) -> int:
    if args.depth <= 0:
        raise ValueError("--depth must be positive")
    if args.repeats <= 0:
        raise ValueError("--repeats must be positive")
    if args.hash <= 0:
        raise ValueError("--hash must be positive")
    if args.timeout <= 0:
        raise ValueError("--timeout must be positive")

    build_binary(args, "latrunculi")
    engine = args.engine or default_engine_path(args.build_preset)
    if not engine.exists():
        raise FileNotFoundError(f"engine binary not found: {engine}")

    epd_file = args.epd_file.resolve()
    selected_positions = select_positions(load_benchmark_positions(epd_file), args.positions)
    threads = parse_threads(args.threads)

    run_dir = make_run_dir(args.output_root, args.label)
    manifest = base_manifest(args, run_dir, FIXED_DEPTH_UCI_FORMAT)
    manifest.update(
        {
            "engine_path": str(engine.resolve()),
            "epd_file": str(epd_file),
            "selected_positions": [position["id"] for position in selected_positions],
            "threads": threads,
            "depth_target": args.depth,
            "hash_mb": args.hash,
            "repeats": args.repeats,
            "timeout_seconds": args.timeout,
            "search_stats_enabled": args.build_preset in {"release-stats", "release-stats-dev"},
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)

    rows: list[dict[str, str]] = []
    for repeat in range(1, args.repeats + 1):
        for position in selected_positions:
            for thread_count in threads:
                result = run_position(
                    engine,
                    position["fen"],
                    thread_count,
                    args.hash,
                    go_command=f"go depth {args.depth}",
                    search_timeout=args.timeout,
                )
                raw_name = f"{position['id']}-t{thread_count}-d{args.depth}-r{repeat}.log"
                (run_dir / "raw" / raw_name).write_text(result.pop("raw_output", "") + "\n", encoding="utf-8")
                rows.append(
                    {
                        "result_format": FIXED_DEPTH_UCI_FORMAT,
                        "position_id": position["id"],
                        "repeat": str(repeat),
                        "threads": str(thread_count),
                        "depth_target": str(args.depth),
                        "hash_mb": str(args.hash),
                        "raw_output_file": f"raw/{raw_name}",
                        **result,
                    }
                )

    write_tsv(run_dir / "results.tsv", rows, FIXED_DEPTH_RESULT_COLUMNS)
    (run_dir / "summary.md").write_text(render_fixed_depth_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


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
    threads: int,
    hash_mb: int,
    *,
    go_command: str,
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
            send(go_command)
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
    result.update(parse_search_stats_lines(raw_lines))
    result["raw_output"] = "\n".join(raw_lines).strip()
    return result


def parse_search_stats_lines(lines: list[str]) -> dict[str, str]:
    stats: dict[str, str] = {column: "" for column in STATS_COLUMNS}
    table_rows: list[dict[str, str]] = []
    capture = False
    for line in lines:
        stripped = line.strip()
        aspiration_match = re.search(
            r"^Aspiration:\s+fail-low=(\d+)\s+fail-high=(\d+)\s+re-searches=(\d+)",
            stripped,
        )
        if aspiration_match:
            stats["stats_aspiration_fail_low"] = aspiration_match.group(1)
            stats["stats_aspiration_fail_high"] = aspiration_match.group(2)
            stats["stats_aspiration_researches"] = aspiration_match.group(3)
            continue
        if "Depth" in stripped and "Nodes (QNode%)" in stripped and "Cutoffs" in stripped:
            capture = True
            continue
        if not capture:
            continue
        if not stripped:
            break
        parts = [part.strip() for part in line.split("|")]
        if len(parts) != 8 or not parts[0].isdigit():
            continue
        row = parse_search_stats_row(parts)
        if row:
            table_rows.append(row)
    if table_rows:
        stats.update(aggregate_search_stats_rows(table_rows))
    return stats


def parse_search_stats_row(parts: list[str]) -> dict[str, str]:
    parsed: dict[str, str] = {}
    nodes_match = re.search(r"^(\d+)\s+\(\s*([-+0-9.]+)%\)", parts[1])
    cutoff_match = re.search(r"^(\d+)\s+\(\s*([-+0-9.]+)\s*/\s*([-+0-9.]+)%\)", parts[2])
    cutoff_index_match = re.search(
        r"^([-+0-9.]+)\s*/\s*([-+0-9.]+)\s*/\s*([-+0-9.]+)\s*/\s*([-+0-9.]+)\s*/\s*([-+0-9.]+)%",
        parts[3],
    )
    pvs_match = re.search(r"^(\d+)$", parts[4])
    main_tt_match = re.search(r"([-+0-9.]+)\s*/\s*([-+0-9.]+)%", parts[5])
    q_tt_match = re.search(r"([-+0-9.]+)\s*/\s*([-+0-9.]+)%", parts[6])
    ebf_match = re.search(r"([-+0-9.]+)\s*/\s*([-+0-9.]+)", parts[7])
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
    if cutoff_index_match:
        avg_index = float(cutoff_index_match.group(1))
        one_pct = float(cutoff_index_match.group(2))
        two_pct = float(cutoff_index_match.group(3))
        three_four_pct = float(cutoff_index_match.group(4))
        five_plus_pct = float(cutoff_index_match.group(5))
        cutoffs = int(parsed.get("stats_beta_cutoffs", "0") or 0)
        parsed["stats_cutoff_avg_index"] = cutoff_index_match.group(1)
        parsed["stats_cutoff_index_total"] = f"{avg_index * cutoffs:.3f}"
        parsed["stats_cutoff_1_pct"] = cutoff_index_match.group(2)
        parsed["stats_cutoff_2_pct"] = cutoff_index_match.group(3)
        parsed["stats_cutoff_3_4_pct"] = cutoff_index_match.group(4)
        parsed["stats_cutoff_5_plus_pct"] = cutoff_index_match.group(5)
        parsed["stats_cutoff_1_count"] = f"{cutoffs * one_pct / 100.0:.3f}"
        parsed["stats_cutoff_2_count"] = f"{cutoffs * two_pct / 100.0:.3f}"
        parsed["stats_cutoff_3_4_count"] = f"{cutoffs * three_four_pct / 100.0:.3f}"
        parsed["stats_cutoff_5_plus_count"] = f"{cutoffs * five_plus_pct / 100.0:.3f}"
    if pvs_match:
        parsed["stats_pvs_researches"] = pvs_match.group(1)
    if main_tt_match:
        parsed["stats_main_tt_hit_pct"] = main_tt_match.group(1)
        parsed["stats_main_tt_cutoff_pct"] = main_tt_match.group(2)
    if q_tt_match:
        parsed["stats_q_tt_hit_pct"] = q_tt_match.group(1)
        parsed["stats_q_tt_cutoff_pct"] = q_tt_match.group(2)
    if ebf_match:
        parsed["stats_ebf"] = ebf_match.group(1)
        parsed["stats_cumulative_ebf"] = ebf_match.group(2)
    return parsed


def aggregate_search_stats_rows(rows: list[dict[str, str]]) -> dict[str, str]:
    total_nodes = sum(int(row.get("stats_nodes", "0") or 0) for row in rows)
    total_cutoffs = sum(int(row.get("stats_beta_cutoffs", "0") or 0) for row in rows)
    total_qnodes = sum(float(row.get("stats_qnodes", "0") or 0.0) for row in rows)
    total_early = sum(float(row.get("stats_cutoff_early_count", "0") or 0.0) for row in rows)
    total_late = sum(float(row.get("stats_cutoff_late_count", "0") or 0.0) for row in rows)
    total_cutoff_index = sum(float(row.get("stats_cutoff_index_total", "0") or 0.0) for row in rows)
    total_cutoff_1 = sum(float(row.get("stats_cutoff_1_count", "0") or 0.0) for row in rows)
    total_cutoff_2 = sum(float(row.get("stats_cutoff_2_count", "0") or 0.0) for row in rows)
    total_cutoff_3_4 = sum(float(row.get("stats_cutoff_3_4_count", "0") or 0.0) for row in rows)
    total_cutoff_5_plus = sum(float(row.get("stats_cutoff_5_plus_count", "0") or 0.0) for row in rows)
    total_pvs_researches = sum(int(row.get("stats_pvs_researches", "0") or 0) for row in rows)
    weighted_main_tt_hit = weighted_average(rows, "stats_main_tt_hit_pct", "stats_nodes")
    weighted_main_tt_cut = weighted_average(rows, "stats_main_tt_cutoff_pct", "stats_nodes")
    weighted_q_tt_hit = weighted_average(rows, "stats_q_tt_hit_pct", "stats_nodes")
    weighted_q_tt_cut = weighted_average(rows, "stats_q_tt_cutoff_pct", "stats_nodes")
    last = rows[-1]
    return {
        "stats_beta_cutoffs": str(total_cutoffs),
        "stats_cutoff_early_pct": f"{(100.0 * total_early / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_late_pct": f"{(100.0 * total_late / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_avg_index": f"{(total_cutoff_index / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_1_pct": f"{(100.0 * total_cutoff_1 / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_2_pct": f"{(100.0 * total_cutoff_2 / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_3_4_pct": f"{(100.0 * total_cutoff_3_4 / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_cutoff_5_plus_pct": f"{(100.0 * total_cutoff_5_plus / total_cutoffs):.1f}" if total_cutoffs else "0.0",
        "stats_pvs_researches": str(total_pvs_researches),
        "stats_main_tt_hit_pct": f"{weighted_main_tt_hit:.1f}" if weighted_main_tt_hit is not None else "",
        "stats_main_tt_cutoff_pct": f"{weighted_main_tt_cut:.1f}" if weighted_main_tt_cut is not None else "",
        "stats_q_tt_hit_pct": f"{weighted_q_tt_hit:.1f}" if weighted_q_tt_hit is not None else "",
        "stats_q_tt_cutoff_pct": f"{weighted_q_tt_cut:.1f}" if weighted_q_tt_cut is not None else "",
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


def group_direct_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str], list[dict[str, str]]]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[(row["position_id"], row["threads"])].append(row)
    return dict(sorted(grouped.items()))


def aggregate_direct_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, float | str | None]]:
    grouped = group_direct_rows(rows)
    fields = ["depth", "nodes", "nps", *STATS_COLUMNS]
    result: dict[tuple[str, str], dict[str, float | str | None]] = {}
    for key, group in grouped.items():
        result[key] = {field: average(num(row.get(field, "")) for row in group) for field in fields}
        result[key]["bestmove"] = ", ".join(sorted({row["bestmove"] for row in group if row.get("bestmove")}))
    return result


def aggregate_fixed_depth_rows(rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, float | str | None]]:
    grouped = group_direct_rows(rows)
    fields = ["depth", "nodes", "time_ms", "nps", *STATS_COLUMNS]
    result: dict[tuple[str, str], dict[str, float | str | None]] = {}
    for key, group in grouped.items():
        result[key] = {field: average(num(row.get(field, "")) for row in group) for field in fields}
        result[key]["score"] = ", ".join(
            sorted(
                {
                    f"{row.get('score_type', '')} {row.get('score_value', '')}".strip()
                    for row in group
                    if row.get("score_type") or row.get("score_value")
                }
            )
        )
        result[key]["bestmove"] = ", ".join(sorted({row["bestmove"] for row in group if row.get("bestmove")}))
    return result


def render_direct_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    grouped = aggregate_direct_rows(rows)
    lines = [
        f"# Direct-UCI benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Engine: `{manifest['engine_path']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Positions: `{manifest['selected_positions']}`",
        f"- Threads: `{','.join(str(t) for t in manifest['threads'])}`",
        f"- Movetime: `{manifest['movetime_ms']} ms`; repeats: `{manifest['repeats']}`; hash: `{manifest['hash_mb']} MiB`",
        "",
        "## Averaged results",
        "| Position | Threads | Depth | Nodes | NPS | Beta cutoffs | Early% | CutIdx | MainTT Hit/Cut% | QTT Hit/Cut% | QNode% | Bestmove(s) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---|---|---:|---|",
    ]
    for (position_id, threads), agg in grouped.items():
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {}/{} | {}/{} | {} | {} |".format(
                position_id,
                threads,
                format_num(agg["depth"]),
                format_num(agg["nodes"]),
                format_num(agg["nps"]),
                format_num(agg["stats_beta_cutoffs"]),
                format_num(agg["stats_cutoff_early_pct"]),
                format_num(agg["stats_cutoff_avg_index"]),
                format_num(agg["stats_main_tt_hit_pct"]),
                format_num(agg["stats_main_tt_cutoff_pct"]),
                format_num(agg["stats_q_tt_hit_pct"]),
                format_num(agg["stats_q_tt_cutoff_pct"]),
                format_num(agg["stats_qnode_pct"]),
                agg["bestmove"] or "",
            )
        )
    lines.extend(
        [
            "",
            "## Files",
            "- `manifest.json`: metadata needed for safe comparisons.",
            "- `results.tsv`: stable direct-UCI columns with parsed search stats.",
            "- `raw/*.log`: combined engine output, including search instrumentation diagnostics.",
        ]
    )
    return "\n".join(lines).rstrip() + "\n"


def render_fixed_depth_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    grouped = aggregate_fixed_depth_rows(rows)
    lines = [
        f"# Fixed-depth UCI benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Engine: `{manifest['engine_path']}`",
        f"- Build preset: `{manifest['build_preset']}`",
        f"- Positions: `{manifest['selected_positions']}`",
        f"- Threads: `{','.join(str(t) for t in manifest['threads'])}`",
        f"- Depth: `{manifest['depth_target']}`; repeats: `{manifest['repeats']}`; hash: `{manifest['hash_mb']} MiB`",
        "",
        "## Averaged results",
        "| Position | Threads | Depth | Nodes | Time ms | NPS | MainTT Hit/Cut% | QTT Hit/Cut% | QNode% | Score(s) | Bestmove(s) |",
        "|---|---:|---:|---:|---:|---:|---|---|---:|---|---|",
    ]
    for (position_id, threads), agg in grouped.items():
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {}/{} | {}/{} | {} | {} | {} |".format(
                position_id,
                threads,
                format_num(agg["depth"]),
                format_num(agg["nodes"]),
                format_num(agg["time_ms"]),
                format_num(agg["nps"]),
                format_num(agg["stats_main_tt_hit_pct"]),
                format_num(agg["stats_main_tt_cutoff_pct"]),
                format_num(agg["stats_q_tt_hit_pct"]),
                format_num(agg["stats_q_tt_cutoff_pct"]),
                format_num(agg["stats_qnode_pct"]),
                agg["score"] or "",
                agg["bestmove"] or "",
            )
        )
    lines.extend(
        [
            "",
            "## Files",
            "- `manifest.json`: metadata needed for safe comparisons.",
            "- `results.tsv`: stable fixed-depth UCI columns with parsed search stats.",
            "- `raw/*.log`: combined engine output, including search instrumentation diagnostics.",
        ]
    )
    return "\n".join(lines).rstrip() + "\n"


def render_direct_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv")
    _, new_rows = read_tsv(new_dir / "results.tsv")
    old = aggregate_direct_rows(old_rows)
    new = aggregate_direct_rows(new_rows)
    keys = sorted(set(old) | set(new))
    lines = [
        f"# Direct-UCI comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        "",
        "## Averaged deltas",
        "| Position | Threads | Depth old/new | Nodes delta | NPS old/new | NPS delta | Beta cutoff delta | Early delta | CutIdx delta | MainTT Hit delta | QTT Hit delta | QNode delta | Bestmove old/new |",
        "|---|---:|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---|",
    ]
    for key in keys:
        position_id, threads = key
        old_agg = old.get(key, {})
        new_agg = new.get(key, {})
        lines.append(
            "| {} | {} | {} / {} | {} | {} / {} | {} | {} | {} | {} | {} | {} | {} / {} |".format(
                position_id,
                threads,
                format_num(old_agg.get("depth")),
                format_num(new_agg.get("depth")),
                percent_delta(old_agg.get("nodes"), new_agg.get("nodes")),
                format_num(old_agg.get("nps")),
                format_num(new_agg.get("nps")),
                percent_delta(old_agg.get("nps"), new_agg.get("nps")),
                percent_delta(old_agg.get("stats_beta_cutoffs"), new_agg.get("stats_beta_cutoffs")),
                delta_points(old_agg.get("stats_cutoff_early_pct"), new_agg.get("stats_cutoff_early_pct")),
                delta_points(old_agg.get("stats_cutoff_avg_index"), new_agg.get("stats_cutoff_avg_index")),
                delta_points(old_agg.get("stats_main_tt_hit_pct"), new_agg.get("stats_main_tt_hit_pct")),
                delta_points(old_agg.get("stats_q_tt_hit_pct"), new_agg.get("stats_q_tt_hit_pct")),
                delta_points(old_agg.get("stats_qnode_pct"), new_agg.get("stats_qnode_pct")),
                old_agg.get("bestmove", ""),
                new_agg.get("bestmove", ""),
            )
        )
    return "\n".join(lines).rstrip() + "\n"


def render_fixed_depth_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv")
    _, new_rows = read_tsv(new_dir / "results.tsv")
    old = aggregate_fixed_depth_rows(old_rows)
    new = aggregate_fixed_depth_rows(new_rows)
    keys = sorted(set(old) | set(new))
    lines = [
        f"# Fixed-depth UCI comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        "",
        "## Averaged deltas",
        "| Position | Threads | Depth old/new | Nodes delta | Time old/new | Time delta | NPS old/new | NPS delta | MainTT Hit delta | QTT Hit delta | QNode delta | Score old/new | Bestmove old/new |",
        "|---|---:|---|---:|---|---:|---|---:|---:|---:|---:|---|---|",
    ]
    for key in keys:
        position_id, threads = key
        old_agg = old.get(key, {})
        new_agg = new.get(key, {})
        lines.append(
            "| {} | {} | {} / {} | {} | {} / {} | {} | {} / {} | {} | {} | {} | {} | {} / {} | {} / {} |".format(
                position_id,
                threads,
                format_num(old_agg.get("depth")),
                format_num(new_agg.get("depth")),
                percent_delta(old_agg.get("nodes"), new_agg.get("nodes")),
                format_num(old_agg.get("time_ms")),
                format_num(new_agg.get("time_ms")),
                percent_delta(old_agg.get("time_ms"), new_agg.get("time_ms")),
                format_num(old_agg.get("nps")),
                format_num(new_agg.get("nps")),
                percent_delta(old_agg.get("nps"), new_agg.get("nps")),
                delta_points(old_agg.get("stats_main_tt_hit_pct"), new_agg.get("stats_main_tt_hit_pct")),
                delta_points(old_agg.get("stats_q_tt_hit_pct"), new_agg.get("stats_q_tt_hit_pct")),
                delta_points(old_agg.get("stats_qnode_pct"), new_agg.get("stats_qnode_pct")),
                old_agg.get("score", ""),
                new_agg.get("score", ""),
                old_agg.get("bestmove", ""),
                new_agg.get("bestmove", ""),
            )
        )
    return "\n".join(lines).rstrip() + "\n"
