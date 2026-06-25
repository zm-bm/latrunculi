from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

from .common import (
    DEFAULT_CPP_BUILD_PRESET,
    REPO_ROOT,
    add_common_run_args,
    base_manifest,
    build_binary,
    default_engine_path,
    format_num,
    make_run_dir,
    num,
    run_capture,
    write_manifest,
    write_tsv,
)

MATCH_FORMAT = "match"
DEFAULT_BOOK = REPO_ROOT / "data/book-ply4-unifen-Q-0.0-0.25.pgn"

MATCH_COLUMNS = [
    "result_format",
    "game",
    "white",
    "black",
    "result",
    "latrunculi_score",
]


def add_match_parser(subparsers: argparse._SubParsersAction[argparse.ArgumentParser]) -> None:
    parser = subparsers.add_parser("match", help="run a local cutechess-cli match")
    add_common_run_args(parser, build_preset=DEFAULT_CPP_BUILD_PRESET)
    parser.add_argument("--engine", type=Path, help="local latrunculi engine path")
    parser.add_argument("--opponent", type=Path, required=True, help="opponent UCI engine path")
    parser.add_argument("--opponent-name", default="opponent")
    parser.add_argument("--tc", default="40/60")
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--games", type=int, default=2)
    parser.add_argument("--concurrency", type=int, default=1)
    parser.add_argument("--book", type=Path, default=DEFAULT_BOOK)
    parser.add_argument("--no-book", action="store_true")
    parser.add_argument("--resign-movecount", type=int, default=5)
    parser.add_argument("--resign-score", type=int, default=1000)


def command_run_match(args: argparse.Namespace) -> int:
    if args.rounds <= 0:
        raise ValueError("--rounds must be positive")
    if args.games <= 0:
        raise ValueError("--games must be positive")
    if args.concurrency <= 0:
        raise ValueError("--concurrency must be positive")
    if args.resign_movecount <= 0:
        raise ValueError("--resign-movecount must be positive")
    if args.resign_score <= 0:
        raise ValueError("--resign-score must be positive")

    build_binary(args, "latrunculi")
    cutechess = shutil.which("cutechess-cli")
    if cutechess is None:
        raise FileNotFoundError("cutechess-cli not found on PATH")

    engine = args.engine or default_engine_path(args.build_preset)
    opponent = args.opponent
    if not engine.exists():
        raise FileNotFoundError(f"engine binary not found: {engine}")
    if not opponent.exists():
        raise FileNotFoundError(f"opponent binary not found: {opponent}")

    run_dir = make_run_dir(args.output_root, args.label)
    pgn_path = run_dir / "raw" / "games.pgn"
    book_path = args.book.resolve()
    book_used = not args.no_book and book_path.exists()

    command = [
        cutechess,
        "-debug",
        "-engine",
        "name=latrunculi",
        f"cmd={engine}",
        "-engine",
        f"name={args.opponent_name}",
        f"cmd={opponent}",
        "-each",
        "proto=uci",
        f"tc={args.tc}",
        "-repeat",
        "-rounds",
        str(args.rounds),
        "-games",
        str(args.games),
        "-concurrency",
        str(args.concurrency),
        "-resign",
        f"movecount={args.resign_movecount}",
        f"score={args.resign_score}",
        "-pgnout",
        str(pgn_path),
    ]
    if book_used:
        command.extend(["-openings", f"file={book_path}", "policy=round"])

    stdout, stderr = run_capture(command, cwd=args.repo)
    (run_dir / "raw" / "cutechess.stdout").write_text(stdout, encoding="utf-8")
    (run_dir / "raw" / "cutechess.stderr").write_text(stderr, encoding="utf-8")
    if not pgn_path.exists():
        raise RuntimeError("cutechess-cli did not write a PGN file")

    rows = parse_match_rows(pgn_path, "latrunculi")
    write_tsv(run_dir / "results.tsv", rows, MATCH_COLUMNS)

    manifest = base_manifest(args, run_dir, MATCH_FORMAT)
    manifest.update(
        {
            "engine_path": str(engine.resolve()),
            "opponent_path": str(opponent.resolve()),
            "opponent_name": args.opponent_name,
            "command": command,
            "tc": args.tc,
            "rounds": args.rounds,
            "games": args.games,
            "concurrency": args.concurrency,
            "book_path": str(book_path),
            "book_used": book_used,
            "resign_movecount": args.resign_movecount,
            "resign_score": args.resign_score,
        }
    )
    write_manifest(run_dir / "manifest.json", manifest)
    (run_dir / "summary.md").write_text(render_match_summary(manifest, rows), encoding="utf-8")
    print(run_dir)
    return 0


def parse_match_rows(pgn_path: Path, local_name: str) -> list[dict[str, str]]:
    games: list[dict[str, str]] = []
    current: dict[str, str] = {}
    header_pattern = re.compile(r'^\[(White|Black|Result)\s+"([^"]*)"\]$')
    for raw_line in pgn_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if raw_line.startswith("[Event ") and current.get("result"):
            games.append(current)
            current = {}
        match = header_pattern.match(raw_line.strip())
        if not match:
            continue
        key, value = match.groups()
        current[key.lower()] = value
    if current.get("result"):
        games.append(current)
    if not games:
        raise RuntimeError(f"no games parsed from {pgn_path}")

    rows: list[dict[str, str]] = []
    for index, game in enumerate(games, start=1):
        white = game.get("white", "")
        black = game.get("black", "")
        result = game.get("result", "")
        rows.append(
            {
                "result_format": MATCH_FORMAT,
                "game": str(index),
                "white": white,
                "black": black,
                "result": result,
                "latrunculi_score": format_num(score_for_engine(result, white, black, local_name), digits=1),
            }
        )
    return rows


def score_for_engine(result: str, white: str, black: str, engine_name: str) -> float | None:
    if engine_name not in {white, black}:
        return None
    if result == "1/2-1/2":
        return 0.5
    if result == "1-0":
        return 1.0 if white == engine_name else 0.0 if black == engine_name else None
    if result == "0-1":
        return 1.0 if black == engine_name else 0.0 if white == engine_name else None
    return None


def render_match_summary(manifest: dict[str, object], rows: list[dict[str, str]]) -> str:
    scores = [num(row.get("latrunculi_score")) for row in rows]
    clean_scores = [score for score in scores if score is not None]
    wins = sum(1 for score in clean_scores if score == 1.0)
    draws = sum(1 for score in clean_scores if score == 0.5)
    losses = sum(1 for score in clean_scores if score == 0.0)
    score_pct = 100.0 * sum(clean_scores) / len(clean_scores) if clean_scores else None
    lines = [
        f"# Match benchmark: {manifest['label']}",
        "",
        "## Metadata",
        f"- Run directory: `{manifest['run_dir']}`",
        f"- Git revision: `{manifest['git_revision']}`",
        f"- Git dirty: `{manifest['git_dirty']}`",
        f"- Engine: `{manifest['engine_path']}`",
        f"- Opponent: `{manifest['opponent_name']}` (`{manifest['opponent_path']}`)",
        f"- TC: `{manifest['tc']}`",
        f"- Rounds: `{manifest['rounds']}`; games: `{manifest['games']}`; concurrency: `{manifest['concurrency']}`",
        f"- Book used: `{manifest['book_used']}` (`{manifest['book_path']}`)",
        f"- Command: `{' '.join(str(item) for item in manifest['command'])}`",
        "",
        "## Summary",
        f"- Games parsed: `{len(rows)}`",
        f"- W/D/L from latrunculi perspective: `{wins}/{draws}/{losses}`",
        f"- Score: `{format_num(score_pct)}%`",
        "",
        "## Games",
        "| Game | White | Black | Result | Latrunculi score |",
        "|---:|---|---|---:|---:|",
    ]
    for row in rows:
        lines.append(
            "| {} | {} | {} | {} | {} |".format(
                row["game"],
                row["white"],
                row["black"],
                row["result"],
                row["latrunculi_score"],
            )
        )
    return "\n".join(lines).rstrip() + "\n"
