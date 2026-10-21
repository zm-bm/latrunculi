#!/usr/bin/env python3
"""Generate OpenBench settings from a Latrunculi template, revisions, and binaries."""

import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import sys

from . import engine


TEMPLATES = Path(__file__).resolve().parent / "resources/openbench"


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def finite_seconds(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return number


def even_games(value):
    number = positive_int(value)
    if number % 2:
        raise argparse.ArgumentTypeError("must be a positive, even game count")
    return number


def revision(value):
    result = subprocess.run(
        ["git", "rev-parse", "--verify", "--end-of-options", f"{value}^{{commit}}"],
        capture_output=True, text=True, check=False,
    )
    sha = result.stdout.strip()
    if result.returncode or not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError(f"cannot resolve commit {value!r} in the active checkout")
    return sha


def parse_args(argv):
    parser = argparse.ArgumentParser(prog="openbench prepare", description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    for name, help_text in (
        ("sprt", "strength comparison with the standard [0,3] bounds"),
        ("games", "fixed-game comparison or explicit self-play"),
        ("corpus", "self-play corpus using one worker and one match runner"),
    ):
        mode = modes.add_parser(name, help=help_text, description=help_text)
        mode.add_argument("--candidate", required=True, metavar="REV",
                          help="commit or Git reference for Dev (both sides in self-play)")
        mode.add_argument("--candidate-engine", type=Path, required=True, metavar="FILE",
                          help="already-built engine corresponding to --candidate")
        mode.add_argument("--description", required=True, help="workload description")
        mode.add_argument("--output", type=Path, required=True, metavar="DIR",
                          help="new directory for settings and bench output")
        mode.add_argument("--timeout", type=finite_seconds, default=600,
                          help="seconds per bench run (default: 600)")
        if name != "corpus":
            mode.add_argument("--baseline", required=name == "sprt", metavar="REV")
            mode.add_argument("--baseline-engine", type=Path, required=name == "sprt",
                              metavar="FILE", help="already-built engine corresponding to --baseline")
        if name == "games":
            mode.add_argument("--self-play", action="store_true",
                              help="use the candidate for both sides instead of a baseline")
        if name != "sprt":
            mode.add_argument("--games", type=even_games, required=name == "games",
                              help="positive, even game count" +
                              (" (default: 42000)" if name == "corpus" else ""))
        if name == "corpus":
            mode.add_argument("--worker-concurrency", type=positive_int, required=True,
                              help="concurrency of the single worker running the corpus")
    args = parser.parse_args(argv)
    if not args.description.strip() or len(args.description) > 1024:
        parser.error("--description must contain 1 to 1024 characters, including non-whitespace")
    if args.mode == "games":
        if args.self_play and (args.baseline is not None or args.baseline_engine is not None):
            parser.error("--self-play cannot be combined with baseline arguments")
        if not args.self_play and (args.baseline is None or args.baseline_engine is None):
            parser.error("games requires --baseline and --baseline-engine, or --self-play")
    return args


def write_settings(path, settings):
    """Publish complete settings inside the newly created output directory."""
    text = json.dumps(settings, indent=2) + "\n"
    temporary = path.with_name(path.name + ".partial")
    try:
        temporary.write_text(text, encoding="utf-8")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def prepare(args):
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f"output already exists: {output}; inspect it or choose a new directory")
    template = TEMPLATES / f"{args.mode}.json"
    settings = json.loads(template.read_text(encoding="utf-8"))
    if not isinstance(settings, dict):
        raise ValueError(f"{template}: expected a JSON object")
    if args.mode != "sprt":
        settings["test_max_games"] = even_games(
            args.games if args.games is not None else settings["test_max_games"])
    if args.mode == "corpus":
        assignment = 4 * args.worker_concurrency
        settings["workload_size"] = (settings["test_max_games"] + assignment - 1) // assignment
    self_play = args.mode == "corpus" or (args.mode == "games" and args.self_play)
    settings["dev_branch"] = revision(args.candidate)
    settings["base_branch"] = settings["dev_branch"] if self_play else revision(args.baseline)
    candidate = engine.executable(args.candidate_engine)
    baseline = candidate if self_play else engine.executable(args.baseline_engine)
    settings["info"] = args.description
    output.mkdir(parents=True, exist_ok=False)
    settings["dev_bench"] = engine.bench_nodes(candidate, output / "candidate-bench.txt", timeout=args.timeout)
    settings["base_bench"] = settings["dev_bench"] if self_play else engine.bench_nodes(
        baseline, output / "baseline-bench.txt", timeout=args.timeout)
    path = output / "settings.json"
    write_settings(path, settings)
    print(f"Settings: {path}", flush=True)
    return path


def main(argv=None):
    args = parse_args(argv)
    try:
        prepare(args)
        return 0
    except (OSError, ValueError, KeyError, argparse.ArgumentTypeError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("error: interrupted; any completed settings and diagnostic files were retained", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
