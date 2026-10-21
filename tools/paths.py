"""Resolve CLI build names and output names within this checkout."""

import argparse
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent


def name(value):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", value):
        raise argparse.ArgumentTypeError(
            "use letters, digits, dots, underscores, or hyphens; start with a letter or digit")
    return value


def build_binary(value, filename):
    # Inspect the original string: Path('./engine') would lose the './'.
    if "/" in value:
        return Path(value)
    return ROOT / "build" / name(value) / filename


def output_directory(output_name, output):
    # Leave explicit paths intact so mkdir still refuses existing symlinks.
    return ROOT / "tools/output" / output_name if output_name is not None else output
