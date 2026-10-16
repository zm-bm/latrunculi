#!/usr/bin/env python3
"""Prepare a standard Latrunculi workload and delegate to the OpenBench client."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

if not __package__:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.analysis import bench


TEMPLATES = Path(__file__).resolve().parent


def even_games(value):
    number = bench.positive_int(value)
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
    parser = argparse.ArgumentParser(description=__doc__)
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
                          help="new directory for settings, fingerprints, and submission")
        mode.add_argument("--config", type=Path, help="credential config forwarded to the client")
        mode.add_argument("--timeout", type=bench.finite_seconds, default=600,
                          help="seconds per fingerprint run (default: 600)")
        mode.add_argument("--submit", action="store_true",
                          help="submit once; default is preparation and client dry run")
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
            mode.add_argument("--worker-concurrency", type=bench.positive_int, required=True,
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
    """Publish complete settings without replacing an existing file."""
    descriptor, name = tempfile.mkstemp(dir=path.parent, prefix=".settings-")
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            json.dump(settings, output, indent=2)
            output.write("\n")
        os.link(temporary, path)
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
    candidate = bench.executable(args.candidate_engine)
    baseline = candidate if self_play else bench.executable(args.baseline_engine)
    settings["info"] = args.description
    output.mkdir(parents=True, exist_ok=False)
    settings["dev_bench"] = bench.fingerprint_engine(candidate, output / "candidate", timeout=args.timeout)
    settings["base_bench"] = settings["dev_bench"] if self_play else bench.fingerprint_engine(
        baseline, output / "baseline", timeout=args.timeout)
    path = output / "settings.json"
    write_settings(path, settings)
    print(f"Settings: {path}", flush=True)
    return path


def client_command(settings, config=None):
    command = [sys.executable, "-m", "openbench_client"]
    if config is not None:
        command.extend(["--config", str(config)])
    return command + ["submit", str(settings)]


def display_client_command(command):
    launcher = Path(__file__).resolve().parents[1] / "openbench-client"
    location = "./tools/openbench-client" if Path.cwd() == launcher.parent.parent else str(launcher)
    return shlex.join([location, *command[3:]])


def run_client(command):
    # Inherit output; the client owns credentials, preflight, readback, and recovery records.
    return subprocess.run(command, check=False).returncode


def main(argv=None):
    args = parse_args(argv)
    try:
        settings = prepare(args)
        command = client_command(settings, args.config)
        submission = settings.parent / "submission"
        submit_command = command + ["--output", str(submission)]
        try:
            result = run_client(submit_command if args.submit else command + ["--dry-run"])
        except (OSError, KeyboardInterrupt):
            if args.submit:
                print(f"Submission interrupted or could not complete. Inspect {submission / 'submission.json'} "
                      "if present and the server state before another attempt.", file=sys.stderr)
            raise
        if result:
            if args.submit:
                print(f"Submission failed. Inspect {submission / 'submission.json'} if present and the "
                      "server state before another attempt; do not retry blindly.", file=sys.stderr)
            else:
                print("Dry run failed; settings and fingerprints were retained. After resolving the error, "
                      "retry the client dry run:\n" + display_client_command(command + ["--dry-run"]),
                      file=sys.stderr)
            return result if result > 0 else 1
        if not args.submit:
            print("Dry run passed. Submit these saved settings without repeating fingerprints:\n"
                  + display_client_command(submit_command), flush=True)
        return 0
    except (OSError, ValueError, KeyError, argparse.ArgumentTypeError, subprocess.SubprocessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("error: interrupted; any completed settings and diagnostic files were retained", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
