"""Collect repeatable engine fingerprints and retain subprocess diagnostics."""

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


def fingerprint_nodes(path):
    matches = re.findall(r"^\s*(\d+) nodes(?:\s|$)", path.read_text(), re.MULTILINE)
    if len(matches) != 1 or int(matches[0]) <= 0:
        raise ValueError(f"{path}: expected one positive bench node count")
    return int(matches[0])


def fingerprint_engine(engine, output, *, repeats=2, timeout=600, reference=None):
    """Collect repeatable bench outputs and return their node count."""
    binary = executable(engine)
    reference = fingerprint_nodes(reference) if reference else None
    output.mkdir(parents=True, exist_ok=False)
    for repeat in range(1, repeats + 1):
        path = output / f"fingerprint-{repeat}.txt"
        collect([str(binary), "bench"], path, timeout)
        nodes = fingerprint_nodes(path)
        if reference is not None and nodes != reference:
            raise ValueError(f"{path}: fingerprint differs: {nodes} != {reference}")
        reference = nodes
        print(f"{path.name}: {nodes} nodes; checks pass", flush=True)
    return reference
