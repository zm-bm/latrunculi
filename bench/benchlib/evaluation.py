from __future__ import annotations

import argparse
import csv
import difflib
import hashlib
import sys
from pathlib import Path

from .common import (
    BENCH_DIR,
    DEFAULT_BUILD_PRESET,
    REPO_ROOT,
    add_common_run_args,
    base_manifest,
    build_binary,
    default_benchmark_path,
    format_num,
    make_run_dir,
    median,
    percent_delta,
    read_tsv,
    run_capture,
    write_manifest,
    write_tsv,
)

EVALUATION_CORPUS = BENCH_DIR / "eval/corpus.tsv"
EVALUATION_BASELINE = BENCH_DIR / "eval/baseline.tsv"
EVALUATION_THROUGHPUT_FORMAT = "evaluation_throughput_v1"

EVALUATION_THROUGHPUT_COLUMNS = [
    "result_format",
    "corpus_version",
    "compiler",
    "build_mode",
    "sample",
    "samples",
    "corpus_size",
    "warmup_repetitions",
    "repetitions",
    "evaluations",
    "checksum",
    "total_ns",
    "ns_per_evaluation",
    "evaluations_per_second",
]


def add_evaluation_parser(
    subparsers: argparse._SubParsersAction[argparse.ArgumentParser],
) -> None:
    parser = subparsers.add_parser(
        "eval", help="validate and manage deterministic evaluation snapshots"
    )
    actions = parser.add_subparsers(dest="eval_action", required=True)
    for name, help_text in (
        ("emit", "write the current evaluation snapshot to stdout"),
        ("verify", "compare the current snapshot with the checked-in baseline"),
        ("regenerate", "replace the checked-in baseline explicitly"),
    ):
        action = actions.add_parser(name, help=help_text)
        action.add_argument(
            "--benchmark", type=Path, help="benchmark binary; bypasses the configured build"
        )
    parser.set_defaults(
        repo=REPO_ROOT,
        build_preset=DEFAULT_BUILD_PRESET,
        skip_build=False,
    )


def add_evaluation_run_parser(
    subparsers: argparse._SubParsersAction[argparse.ArgumentParser],
) -> None:
    parser = subparsers.add_parser("eval", help="measure isolated evaluation throughput")
    add_common_run_args(parser)
    parser.add_argument("--warmup", type=int)
    parser.add_argument("--repetitions", type=int)
    parser.add_argument("--samples", type=int)
    parser.add_argument(
        "--benchmark", type=Path, help="benchmark binary; bypasses the configured build"
    )


def command_evaluation(args: argparse.Namespace) -> int:
    benchmark = resolve_benchmark(args)
    command = [
        str(benchmark),
        "eval",
        "--corpus",
        str(EVALUATION_CORPUS.resolve()),
    ]

    snapshot, stderr = run_capture(command, cwd=args.repo)
    if stderr:
        print(stderr, file=sys.stderr, end="")
    if not snapshot:
        raise RuntimeError("evaluation snapshot runner produced no output")

    if args.eval_action == "emit":
        sys.stdout.write(snapshot)
        return 0
    if args.eval_action == "verify":
        verify_snapshot(snapshot)
        print(EVALUATION_BASELINE)
        return 0
    if args.eval_action == "regenerate":
        replace_baseline(snapshot)
        print(EVALUATION_BASELINE)
        return 0
    raise ValueError(f"unknown evaluation action: {args.eval_action}")


def command_run_evaluation(args: argparse.Namespace) -> int:
    benchmark = resolve_benchmark(args)
    corpus = EVALUATION_CORPUS.resolve()
    command = [
        str(benchmark),
        "eval",
        "throughput",
        "--corpus",
        str(corpus),
    ]
    for option, value in (
        ("--warmup", args.warmup),
        ("--repetitions", args.repetitions),
        ("--samples", args.samples),
    ):
        if value is not None:
            command.extend((option, str(value)))

    stdout, stderr = run_capture(command, cwd=args.repo)
    rows = parse_evaluation_rows(stdout)
    first = rows[0]

    run_dir = make_run_dir(args.output_root, args.label)
    (run_dir / "raw" / "benchmark.stdout").write_text(stdout, encoding="utf-8")
    (run_dir / "raw" / "benchmark.stderr").write_text(stderr, encoding="utf-8")

    manifest = base_manifest(args, run_dir, EVALUATION_THROUGHPUT_FORMAT)
    manifest.update(
        {
            "benchmark_path": str(benchmark),
            "command": command,
            "corpus_path": str(corpus),
            "corpus_sha256": hashlib.sha256(corpus.read_bytes()).hexdigest(),
            "corpus_version": first["corpus_version"],
            "corpus_size": int(first["corpus_size"]),
            "warmup_repetitions": int(first["warmup_repetitions"]),
            "repetitions": int(first["repetitions"]),
            "samples": int(first["samples"]),
            "compiler": first["compiler"],
            "build_mode": first["build_mode"],
        }
    )
    write_tsv(run_dir / "results.tsv", rows, EVALUATION_THROUGHPUT_COLUMNS)
    write_manifest(run_dir / "manifest.json", manifest)
    (run_dir / "summary.md").write_text(
        render_evaluation_summary(manifest, rows), encoding="utf-8"
    )
    print(run_dir)
    return 0


def resolve_benchmark(args: argparse.Namespace) -> Path:
    if args.benchmark is not None:
        benchmark = args.benchmark.expanduser().resolve()
    else:
        build_binary(args, "benchmark")
        benchmark = default_benchmark_path(args.build_preset)
    if not benchmark.exists():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")
    return benchmark.resolve()


def verify_snapshot(snapshot: str) -> None:
    if not EVALUATION_BASELINE.exists():
        raise FileNotFoundError(f"evaluation baseline not found: {EVALUATION_BASELINE}")
    baseline = EVALUATION_BASELINE.read_text(encoding="utf-8")
    if snapshot == baseline:
        return

    diff = list(
        difflib.unified_diff(
            baseline.splitlines(keepends=True),
            snapshot.splitlines(keepends=True),
            fromfile=str(EVALUATION_BASELINE),
            tofile="current evaluation snapshot",
            n=3,
        )
    )
    limit = 200
    preview = "".join(diff[:limit])
    if len(diff) > limit:
        preview += f"... diff truncated after {limit} lines\n"
    raise RuntimeError(f"evaluation snapshot differs from baseline:\n{preview}")


def replace_baseline(snapshot: str) -> None:
    EVALUATION_BASELINE.parent.mkdir(parents=True, exist_ok=True)
    temporary = EVALUATION_BASELINE.with_suffix(".tmp")
    temporary.write_text(snapshot, encoding="utf-8")
    temporary.replace(EVALUATION_BASELINE)


def parse_evaluation_rows(output: str) -> list[dict[str, str]]:
    reader = csv.DictReader(output.splitlines(), delimiter="\t")
    if reader.fieldnames != EVALUATION_THROUGHPUT_COLUMNS:
        raise RuntimeError("evaluation benchmark produced unexpected TSV header")
    rows = [dict(row) for row in reader]
    validate_evaluation_rows(rows)
    return rows


def validate_evaluation_rows(rows: list[dict[str, str]]) -> None:
    if not rows:
        raise RuntimeError("evaluation benchmark produced no TSV rows")

    constant_fields = (
        "corpus_version",
        "compiler",
        "build_mode",
        "samples",
        "corpus_size",
        "warmup_repetitions",
        "repetitions",
        "evaluations",
        "checksum",
    )
    first = rows[0]
    try:
        samples = int(first["samples"])
        corpus_size = int(first["corpus_size"])
        repetitions = int(first["repetitions"])
        evaluations = int(first["evaluations"])
        for expected_sample, row in enumerate(rows, start=1):
            if row["result_format"] != EVALUATION_THROUGHPUT_FORMAT:
                raise RuntimeError("evaluation benchmark produced unexpected result format")
            for field in constant_fields:
                if row[field] != first[field]:
                    raise RuntimeError(f"evaluation benchmark produced inconsistent {field}")
            if int(row["sample"]) != expected_sample:
                raise RuntimeError("evaluation benchmark produced an invalid sample sequence")
    except (KeyError, ValueError) as error:
        raise RuntimeError("evaluation benchmark produced invalid fields") from error

    if samples == 0 or samples != len(rows):
        raise RuntimeError("evaluation benchmark produced an inconsistent sample count")
    if corpus_size == 0 or repetitions == 0 or evaluations != corpus_size * repetitions:
        raise RuntimeError("evaluation benchmark produced an inconsistent evaluation count")


def metric_distribution(rows: list[dict[str, str]], field: str) -> tuple[float, float, float]:
    values = [float(row[field]) for row in rows]
    middle = median(values)
    assert middle is not None
    return middle, min(values), max(values)


def format_distribution(distribution: tuple[float, float, float]) -> str:
    middle, minimum, maximum = distribution
    return "{} [{}–{}]".format(
        format_num(middle, digits=3),
        format_num(minimum, digits=3),
        format_num(maximum, digits=3),
    )


def render_evaluation_summary(
    manifest: dict[str, object], rows: list[dict[str, str]]
) -> str:
    ns_per_evaluation = metric_distribution(rows, "ns_per_evaluation")
    evaluations_per_second = metric_distribution(rows, "evaluations_per_second")
    return "\n".join(
        [
            f"# Evaluation benchmark: {manifest['label']}",
            "",
            "## Metadata",
            f"- Run directory: `{manifest['run_dir']}`",
            f"- Git revision: `{manifest['git_revision']}`",
            f"- Git dirty: `{manifest['git_dirty']}`",
            f"- Benchmark: `{manifest['benchmark_path']}`",
            "- Build: `{}` / `{}` / `{}`".format(
                manifest["build_preset"], manifest["build_mode"], manifest["compiler"]
            ),
            "- Corpus: `{}` positions, version `{}`".format(
                manifest["corpus_size"], manifest["corpus_version"]
            ),
            f"- Workload: `{manifest['warmup_repetitions']}` warmup repetitions; "
            f"`{manifest['repetitions']}` repetitions × `{manifest['samples']}` samples",
            f"- Checksum: `{rows[0]['checksum']}`",
            "",
            "## Median results (min–max)",
            "- Nanoseconds/evaluation: `{}` (lower is better)".format(
                format_distribution(ns_per_evaluation)
            ),
            "- Evaluations/second: `{}` (higher is better)".format(
                format_distribution(evaluations_per_second)
            ),
            "",
        ]
    )


def render_evaluation_compare(
    old_dir: Path,
    new_dir: Path,
    old_manifest: dict[str, object],
    new_manifest: dict[str, object],
) -> str:
    _, old_rows = read_tsv(old_dir / "results.tsv", EVALUATION_THROUGHPUT_COLUMNS)
    _, new_rows = read_tsv(new_dir / "results.tsv", EVALUATION_THROUGHPUT_COLUMNS)
    validate_evaluation_rows(old_rows)
    validate_evaluation_rows(new_rows)

    old_ns = metric_distribution(old_rows, "ns_per_evaluation")
    new_ns = metric_distribution(new_rows, "ns_per_evaluation")
    old_rate = metric_distribution(old_rows, "evaluations_per_second")
    new_rate = metric_distribution(new_rows, "evaluations_per_second")
    old_checksum = old_rows[0]["checksum"]
    new_checksum = new_rows[0]["checksum"]
    checksum_status = "match" if old_checksum == new_checksum else "differ; run `eval verify`"

    lines = [
        f"# Evaluation comparison: {old_manifest.get('label')} vs {new_manifest.get('label')}",
        "",
        "## Runs",
        f"- Baseline: `{old_dir}`",
        f"- Candidate: `{new_dir}`",
        f"- Checksums: `{old_checksum}` / `{new_checksum}` ({checksum_status})",
        "",
        "## Median deltas",
        "| Metric | Baseline median [min–max] | Candidate median [min–max] | Delta |",
        "|---|---:|---:|---:|",
        "| Nanoseconds/evaluation (lower is better) | {} | {} | {} |".format(
            format_distribution(old_ns),
            format_distribution(new_ns),
            percent_delta(old_ns[0], new_ns[0]),
        ),
        "| Evaluations/second (higher is better) | {} | {} | {} |".format(
            format_distribution(old_rate),
            format_distribution(new_rate),
            percent_delta(old_rate[0], new_rate[0]),
        ),
    ]
    return "\n".join(lines).rstrip() + "\n"
