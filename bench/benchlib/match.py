from __future__ import annotations

import argparse
import hashlib
import math
import os
import re
import shutil
import subprocess
from collections import Counter
from pathlib import Path
from typing import Iterable

from .common import (
    REPO_ROOT,
    add_common_run_args,
    base_manifest,
    build_binary,
    default_engine_path,
    format_num,
    make_run_dir,
    run_command,
    write_manifest,
    write_tsv,
)

MATCH_FORMAT = "match_v1"
OPENING_BOOK = REPO_ROOT / "data/book-ply4-unifen-Q-0.0-0.25.pgn"
OPENING_BOOK_SHA256 = "a9c223edf1592cddca3ac20c62374b1f8b1d18a2ae6270de9042155bd3764d17"
OPENING_COUNT = 5_472

CANDIDATE_NAME = "candidate"
BASELINE_NAME = "baseline"

MATCH_COLUMNS = [
    "result_format",
    "game",
    "pair",
    "candidate_color",
    "white",
    "black",
    "result",
    "candidate_score",
    "termination",
]

FINISHED_GAME_PATTERN = re.compile(
    r"^Finished game (?P<game>\d+) \((?P<white>.+) vs (?P<black>.+)\): "
    r"(?P<result>1-0|0-1|1/2-1/2) \{(?P<termination>.*)\}$"
)
ELO_PATTERN = re.compile(
    r"^Elo difference: (?P<elo>[+-]?(?:\d+(?:\.\d+)?|inf|nan)) "
    r"\+/- (?P<error>[+-]?(?:\d+(?:\.\d+)?|inf|nan)),"
)
ABNORMAL_TERMINATIONS = (
    "disconnect",
    "stall",
    "illegal",
    "time forfeit",
    "on time",
    "invalid claim",
    "wrong result",
    "doesn't respond",
)


def add_match_parser(
    subparsers: argparse._SubParsersAction[argparse.ArgumentParser],
) -> None:
    parser = subparsers.add_parser("match", help="run a paired Cute Chess strength match")
    add_common_run_args(parser)
    parser.add_argument("--profile", choices=("smoke", "standard"), required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--baseline-revision", required=True)
    parser.add_argument("--baseline-dirty", action="store_true")
    parser.add_argument("--pairs", type=int)
    parser.add_argument("--concurrency", type=int, default=1)
    parser.add_argument("--cutechess", type=Path)


def profile_settings(profile: str) -> dict[str, object]:
    if profile == "smoke":
        return {
            "default_pairs": 1,
            "time_control": "inf",
            "depth": 1,
            "time_margin_ms": None,
            "draw": None,
            "resign": None,
            "max_moves": None,
        }
    if profile == "standard":
        return {
            "default_pairs": 1_000,
            "time_control": "10+0.1",
            "depth": None,
            "time_margin_ms": 250,
            "draw": ("movenumber=34", "movecount=8", "score=20"),
            "resign": ("movecount=3", "score=600", "twosided=true"),
            "max_moves": 200,
        }
    raise ValueError(f"unknown match profile: {profile}")


def build_match_command(
    *,
    cutechess: Path,
    candidate: Path,
    baseline: Path,
    openings: Path,
    pgn: Path,
    profile: str,
    pairs: int,
    concurrency: int,
) -> list[str]:
    settings = profile_settings(profile)
    command = [
        str(cutechess),
        "-engine",
        f"name={CANDIDATE_NAME}",
        f"cmd={candidate}",
        "-engine",
        f"name={BASELINE_NAME}",
        f"cmd={baseline}",
        "-each",
        "proto=uci",
        f"tc={settings['time_control']}",
        "option.Hash=32",
        "option.Threads=1",
    ]
    if settings["depth"] is not None:
        command.append(f"depth={settings['depth']}")
    if settings["time_margin_ms"] is not None:
        command.append(f"timemargin={settings['time_margin_ms']}")

    command.extend(
        [
            "-games",
            "2",
            "-rounds",
            str(pairs),
            "-repeat",
            "-concurrency",
            str(concurrency),
            "-openings",
            f"file={openings}",
            "format=pgn",
            "order=sequential",
            "start=1",
            "policy=round",
            "-srand",
            "1",
            "-recover",
        ]
    )
    if settings["draw"] is not None:
        command.extend(("-draw", *settings["draw"]))
    if settings["resign"] is not None:
        command.extend(("-resign", *settings["resign"]))
    if settings["max_moves"] is not None:
        command.extend(("-maxmoves", str(settings["max_moves"])))
    command.extend(("-pgnout", str(pgn), "-resultformat", "wide3"))
    return command


def parse_finished_games(lines: Iterable[str], expected_pairs: int) -> list[dict[str, str]]:
    games: dict[int, dict[str, str]] = {}
    for line in lines:
        match = FINISHED_GAME_PATTERN.fullmatch(line.strip())
        if match is None:
            continue
        game = int(match.group("game"))
        if game in games:
            raise RuntimeError(f"duplicate completed game: {game}")
        white = match.group("white")
        black = match.group("black")
        if {white, black} != {CANDIDATE_NAME, BASELINE_NAME}:
            raise RuntimeError(f"unexpected engines in game {game}")
        result = match.group("result")
        candidate_color = "white" if white == CANDIDATE_NAME else "black"
        games[game] = {
            "result_format": MATCH_FORMAT,
            "game": str(game),
            "pair": str((game + 1) // 2),
            "candidate_color": candidate_color,
            "white": white,
            "black": black,
            "result": result,
            "candidate_score": format_num(candidate_score(result, candidate_color)),
            "termination": match.group("termination"),
        }

    expected_games = expected_pairs * 2
    if sorted(games) != list(range(1, expected_games + 1)):
        raise RuntimeError(
            f"expected {expected_games} completed games, found {len(games)}"
        )

    rows = [games[game] for game in sorted(games)]
    for pair in range(expected_pairs):
        pair_rows = rows[pair * 2 : pair * 2 + 2]
        if {row["candidate_color"] for row in pair_rows} != {"white", "black"}:
            raise RuntimeError(f"match pair {pair + 1} did not swap colors")
    return rows


def candidate_score(result: str, candidate_color: str) -> float:
    if result == "1/2-1/2":
        return 0.5
    candidate_won = (result == "1-0") == (candidate_color == "white")
    return 1.0 if candidate_won else 0.0


def summarize_results(rows: list[dict[str, str]]) -> dict[str, object]:
    scores = [float(row["candidate_score"]) for row in rows]
    wins = scores.count(1.0)
    draws = scores.count(0.5)
    losses = scores.count(0.0)
    pair_scores = [scores[index] + scores[index + 1] for index in range(0, len(scores), 2)]
    pentanomial = Counter(pair_scores)
    terminations = Counter(row["termination"] for row in rows)
    failures = [row for row in rows if is_abnormal_termination(row["termination"])]
    return {
        "wins": wins,
        "draws": draws,
        "losses": losses,
        "score_percent": 100.0 * sum(scores) / len(scores),
        "pentanomial": [pentanomial[value] for value in (0.0, 0.5, 1.0, 1.5, 2.0)],
        "terminations": terminations,
        "failures": failures,
    }


def is_abnormal_termination(termination: str) -> bool:
    lowered = termination.lower()
    return any(marker in lowered for marker in ABNORMAL_TERMINATIONS)


def final_elo_line(lines: Iterable[str]) -> str:
    result = "unavailable"
    for line in lines:
        line = line.strip()
        if line.startswith("Elo difference:"):
            result = line
    return result


def strength_conclusion(profile: str, elo_line: str) -> str:
    if profile == "smoke":
        return "not assessed by the smoke profile"
    match = ELO_PATTERN.match(elo_line)
    if match is None:
        return "inconclusive (no numeric Cute Chess interval)"
    elo = float(match.group("elo"))
    error = float(match.group("error"))
    if not math.isfinite(elo) or not math.isfinite(error):
        return "inconclusive (no finite Cute Chess interval)"
    if elo - error > 0:
        return "candidate improvement supported"
    if elo + error < 0:
        return "candidate regression supported"
    return "inconclusive"


def render_match_summary(
    manifest: dict[str, object],
    statistics: dict[str, object],
    elo_line: str,
) -> str:
    pentanomial = "/".join(str(value) for value in statistics["pentanomial"])
    failures = statistics["failures"]
    decision = (
        "invalid due to abnormal engine termination"
        if failures
        else strength_conclusion(str(manifest["profile"]), elo_line)
    )
    termination_lines = [
        f"- `{reason}`: `{count}`"
        for reason, count in sorted(statistics["terminations"].items())
    ]
    lines = [
        f"# Engine match: {manifest['label']}",
        "",
        *(
            ["> **Invalid strength run:** an abnormal engine termination occurred.", ""]
            if failures
            else []
        ),
        "## Configuration",
        f"- Profile: `{manifest['profile']}`",
        f"- Candidate: `{manifest['candidate_path']}` (`{manifest['candidate_revision']}`)",
        f"- Baseline: `{manifest['baseline_path']}` (`{manifest['baseline_revision']}`)",
        f"- Opening pairs: `{manifest['pairs']}`; concurrency: `{manifest['concurrency']}`",
        f"- Time control: `{manifest['time_control']}`; Threads: `1`; Hash: `32 MB`",
        f"- Opening SHA-256: `{manifest['openings_sha256']}`",
        f"- Cute Chess: `{manifest['cutechess_version']}`",
        "",
        "## Results",
        "- Candidate W/D/L: `{}/{}/{}`".format(
            statistics["wins"], statistics["draws"], statistics["losses"]
        ),
        f"- Candidate score: `{format_num(statistics['score_percent'])}%`",
        f"- Pentanomial [0 / 0.5 / 1 / 1.5 / 2]: `{pentanomial}`",
        f"- {elo_line}",
        f"- Decision: `{decision}`",
        "",
        "## Terminations",
        *termination_lines,
        "",
        "## Command",
        "```text",
        " ".join(str(part) for part in manifest["command"]),
        "```",
        "",
    ]
    return "\n".join(lines)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_cutechess(path: Path | None) -> Path:
    if path is None:
        executable = shutil.which("cutechess-cli")
        if executable is None:
            raise FileNotFoundError("cutechess-cli not found on PATH")
        return Path(executable).resolve()
    executable = path.expanduser().resolve()
    require_executable(executable, "Cute Chess")
    return executable


def require_executable(path: Path, description: str) -> None:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise FileNotFoundError(f"{description} is not executable: {path}")


def command_run_match(args: argparse.Namespace) -> int:
    settings = profile_settings(args.profile)
    pairs = args.pairs if args.pairs is not None else int(settings["default_pairs"])
    if pairs <= 0 or pairs > OPENING_COUNT:
        raise ValueError(f"--pairs must be between 1 and {OPENING_COUNT}")
    if args.concurrency <= 0:
        raise ValueError("--concurrency must be positive")
    baseline_revision = args.baseline_revision.strip()
    if not baseline_revision:
        raise ValueError("--baseline-revision must not be empty")

    baseline = args.baseline.expanduser().resolve()
    require_executable(baseline, "baseline engine")
    if not OPENING_BOOK.is_file():
        raise FileNotFoundError(f"opening book not found: {OPENING_BOOK}")
    openings_sha256 = file_sha256(OPENING_BOOK)
    if openings_sha256 != OPENING_BOOK_SHA256:
        raise RuntimeError(
            f"opening book checksum mismatch: expected {OPENING_BOOK_SHA256}, "
            f"got {openings_sha256}"
        )
    cutechess = resolve_cutechess(args.cutechess)
    cutechess_version = run_command([str(cutechess), "-version"], cwd=args.repo).splitlines()[0]

    build_binary(args, "latrunculi")
    candidate = default_engine_path(args.build_preset).resolve()
    require_executable(candidate, "candidate engine")

    run_dir = make_run_dir(args.output_root, args.label)
    stdout_path = run_dir / "raw/cutechess.stdout"
    stderr_path = run_dir / "raw/cutechess.stderr"
    pgn_path = run_dir / "raw/games.pgn"
    command = build_match_command(
        cutechess=cutechess,
        candidate=candidate,
        baseline=baseline,
        openings=OPENING_BOOK.resolve(),
        pgn=pgn_path.resolve(),
        profile=args.profile,
        pairs=pairs,
        concurrency=args.concurrency,
    )

    manifest = base_manifest(args, run_dir, MATCH_FORMAT)
    manifest.update(
        {
            "status": "running",
            "profile": args.profile,
            "pairs": pairs,
            "games": pairs * 2,
            "concurrency": args.concurrency,
            "time_control": settings["time_control"],
            "depth": settings["depth"],
            "time_margin_ms": settings["time_margin_ms"],
            "draw": settings["draw"],
            "resign": settings["resign"],
            "max_moves": settings["max_moves"],
            "threads": 1,
            "hash_mb": 32,
            "candidate_path": str(candidate),
            "candidate_sha256": file_sha256(candidate),
            "candidate_revision": manifest["git_revision"],
            "candidate_dirty": manifest["git_dirty"],
            "baseline_path": str(baseline),
            "baseline_sha256": file_sha256(baseline),
            "baseline_revision": baseline_revision,
            "baseline_dirty": args.baseline_dirty,
            "cutechess_path": str(cutechess),
            "cutechess_version": cutechess_version,
            "openings_path": str(OPENING_BOOK.resolve()),
            "openings_sha256": openings_sha256,
            "openings_count": OPENING_COUNT,
            "command": command,
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)

    try:
        with stdout_path.open("w", encoding="utf-8") as stdout, stderr_path.open(
            "w", encoding="utf-8"
        ) as stderr:
            completed = subprocess.run(
                command, cwd=args.repo, text=True, stdout=stdout, stderr=stderr
            )
        if completed.returncode != 0:
            raise RuntimeError(f"Cute Chess exited with status {completed.returncode}")
        if not pgn_path.is_file():
            raise RuntimeError("Cute Chess did not produce a PGN")

        with stdout_path.open(encoding="utf-8", errors="replace") as output:
            rows = parse_finished_games(output, pairs)
        statistics = summarize_results(rows)
        with stdout_path.open(encoding="utf-8", errors="replace") as output:
            elo_line = final_elo_line(output)
        write_tsv(run_dir / "results.tsv", rows, MATCH_COLUMNS)
        manifest.update(
            {
                "status": "invalid" if statistics["failures"] else "complete",
                "completed_games": len(rows),
                "candidate_wins": statistics["wins"],
                "candidate_draws": statistics["draws"],
                "candidate_losses": statistics["losses"],
                "pentanomial": statistics["pentanomial"],
                "abnormal_terminations": len(statistics["failures"]),
            }
        )
        write_manifest(run_dir / "manifest.json", manifest)
        (run_dir / "summary.md").write_text(
            render_match_summary(manifest, statistics, elo_line), encoding="utf-8"
        )
        if statistics["failures"]:
            raise RuntimeError("match contains abnormal engine terminations")
    except Exception as error:
        if manifest["status"] == "running":
            manifest.update({"status": "invalid", "error": str(error)})
            write_manifest(run_dir / "manifest.json", manifest)
            (run_dir / "summary.md").write_text(
                f"# Engine match failed\n\n- Error: `{error}`\n- Artifacts: `{run_dir}`\n",
                encoding="utf-8",
            )
        raise RuntimeError(f"{error}; artifacts preserved in {run_dir}") from error

    print(run_dir)
    return 0
