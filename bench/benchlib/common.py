from __future__ import annotations

import argparse
import csv
import json
import math
import re
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Iterable

BENCH_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = BENCH_DIR.parent
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "scratch/bench-runs"
DEFAULT_CPP_BUILD_PRESET = "release-dev"
DEFAULT_DIRECT_BUILD_PRESET = "release-stats-dev"
FORMAT_VERSION = 1


def add_common_run_args(parser: argparse.ArgumentParser, *, build_preset: str) -> None:
    parser.add_argument("--label", required=True, help="short label included in the run directory name")
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--repo", type=Path, default=REPO_ROOT)
    parser.add_argument("--build-preset", default=build_preset)
    parser.add_argument("--skip-build", action="store_true", help="skip configure/build")


def run_command(command: list[str], *, cwd: Path) -> str:
    completed = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if completed.returncode != 0:
        raise RuntimeError(f"command failed ({' '.join(command)}):\n{completed.stdout}")
    return completed.stdout


def run_capture(command: list[str], *, cwd: Path) -> tuple[str, str]:
    completed = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({' '.join(command)}):\nstdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed.stdout, completed.stderr


def git_text(repo: Path, args: list[str]) -> str:
    try:
        return run_command(["git", *args], cwd=repo).strip()
    except RuntimeError:
        return ""


def git_dirty(repo: Path) -> bool:
    return bool(git_text(repo, ["status", "--short"]))


def slugify(label: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", label.strip()).strip("-")
    return slug or "run"


def make_run_dir(output_root: Path, label: str) -> Path:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base = output_root / f"{timestamp}-{slugify(label)}"
    run_dir = base
    index = 2
    while run_dir.exists():
        run_dir = Path(f"{base}-{index}")
        index += 1
    run_dir.mkdir(parents=True)
    (run_dir / "raw").mkdir()
    return run_dir


def build_binary(args: argparse.Namespace, target: str) -> None:
    if args.skip_build:
        return
    run_command(["cmake", "--preset", args.build_preset], cwd=args.repo)
    run_command(["cmake", "--build", "--preset", args.build_preset, "--target", target], cwd=args.repo)


def default_benchmark_path(build_preset: str) -> Path:
    return REPO_ROOT / "build" / build_preset / "bin" / "benchmark"


def default_engine_path(build_preset: str) -> Path:
    return REPO_ROOT / "build" / build_preset / "bin" / "latrunculi"


def base_manifest(args: argparse.Namespace, run_dir: Path, result_format: str) -> dict[str, object]:
    return {
        "format_version": FORMAT_VERSION,
        "result_format": result_format,
        "suite": args.suite,
        "label": args.label,
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "run_dir": str(run_dir),
        "repo_path": str(args.repo.resolve()),
        "git_revision": git_text(args.repo, ["rev-parse", "HEAD"]),
        "git_dirty": git_dirty(args.repo),
        "build_preset": args.build_preset,
    }


def write_manifest(path: Path, manifest: dict[str, object]) -> None:
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def read_manifest(run_dir: Path) -> dict[str, object]:
    return json.loads((run_dir / "manifest.json").read_text(encoding="utf-8"))


def read_tsv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            raise ValueError(f"missing TSV header in {path}")
        return list(reader.fieldnames), [dict(row) for row in reader]


def write_tsv(path: Path, rows: Iterable[dict[str, str]], columns: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({column: row.get(column, "") for column in columns})


def num(value: object) -> float | None:
    if value in ("", None):
        return None
    try:
        return float(str(value))
    except ValueError:
        return None


def average(values: Iterable[float | None]) -> float | None:
    clean = [value for value in values if value is not None]
    return sum(clean) / len(clean) if clean else None


def median(values: Iterable[float | None]) -> float | None:
    clean = sorted(value for value in values if value is not None)
    if not clean:
        return None
    midpoint = len(clean) // 2
    if len(clean) % 2:
        return clean[midpoint]
    return (clean[midpoint - 1] + clean[midpoint]) / 2.0


def format_num(value: float | str | None, *, digits: int = 1) -> str:
    value = num(value)
    if value is None or math.isnan(value):
        return ""
    if abs(value - round(value)) < 0.05:
        return str(int(round(value)))
    return f"{value:.{digits}f}"


def percent_delta(old: float | str | None, new: float | str | None) -> str:
    old_num = num(old)
    new_num = num(new)
    if old_num in (None, 0.0) or new_num is None:
        return ""
    return f"{((new_num - old_num) / old_num) * 100.0:+.1f}%"


def delta_points(old: float | str | None, new: float | str | None) -> str:
    old_num = num(old)
    new_num = num(new)
    if old_num is None or new_num is None:
        return ""
    return f"{new_num - old_num:+.1f}"
