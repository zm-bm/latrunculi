"""Collect engine bench node counts and retain subprocess diagnostics."""

import os
import re
import subprocess
import time


def executable(path):
    path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise ValueError(f"not an executable: {path}")
    return path


def collect(command, output, timeout):
    """Keep raw stdout/stderr even when the child fails or times out."""
    print(f"Running {output.name}", flush=True)
    with output.open("x") as stdout, output.with_suffix(".stderr.txt").open("x") as stderr:
        start = time.monotonic()
        subprocess.run(command, stdout=stdout, stderr=stderr, check=True, timeout=timeout)
    return time.monotonic() - start


def bench_nodes(binary, output, *, timeout=600):
    """Run bench once, retain stdout/stderr, and return the node count."""
    binary = executable(binary)
    collect([str(binary), "bench"], output, timeout)
    matches = re.findall(r"^\s*(\d+) nodes(?:\s|$)", output.read_text(), re.MULTILINE)
    if len(matches) != 1 or int(matches[0]) <= 0:
        raise ValueError(f"{output}: expected one positive bench node count")
    return int(matches[0])
