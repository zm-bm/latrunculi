from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from bench.benchlib.common import FORMAT_VERSION
from bench.benchlib.evaluation import (
    EVALUATION_THROUGHPUT_COLUMNS,
    EVALUATION_THROUGHPUT_FORMAT,
    parse_evaluation_rows,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
BENCH_SCRIPT = REPO_ROOT / "bench/bench.py"


def evaluation_output(*, candidate: bool = False) -> str:
    header = "\t".join(EVALUATION_THROUGHPUT_COLUMNS)
    timings = ((1000, "20.000", "50000000.000"), (1100, "22.000", "45454545.455"))
    if candidate:
        timings = ((900, "18.000", "55555555.556"), (990, "19.800", "50505050.505"))
    rows = []
    for sample, (total_ns, ns_per_eval, rate) in enumerate(timings, start=1):
        rows.append(
            "\t".join(
                (
                    EVALUATION_THROUGHPUT_FORMAT,
                    "1",
                    "GCC test",
                    "release",
                    str(sample),
                    "2",
                    "24",
                    "1",
                    "2",
                    "48",
                    "3992",
                    str(total_ns),
                    ns_per_eval,
                    rate,
                )
            )
        )
    return "\n".join((header, *rows)) + "\n"


class EvaluationBenchmarkTest(unittest.TestCase):
    def test_parser_validates_schema_and_sample_consistency(self) -> None:
        output = evaluation_output()
        self.assertEqual(len(parse_evaluation_rows(output)), 2)

        cases = {
            "header": output.replace("result_format", "format", 1),
            "sample sequence": output.replace("\trelease\t2\t2\t", "\trelease\t3\t2\t", 1),
            "checksum": output.rsplit("\t3992\t", 1)[0]
            + "\t3993\t"
            + output.rsplit("\t3992\t", 1)[1],
            "evaluation count": output.replace("\t48\t", "\t47\t", 1),
        }
        for name, invalid in cases.items():
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                parse_evaluation_rows(invalid)

    def test_compare_dispatches_and_renders_evaluation_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline = root / "baseline"
            candidate = root / "candidate"
            baseline.mkdir()
            candidate.mkdir()

            manifest = {
                "format_version": FORMAT_VERSION,
                "result_format": EVALUATION_THROUGHPUT_FORMAT,
                "suite": "eval",
                "label": "baseline",
                "corpus_sha256": "abc",
                "corpus_version": "1",
                "corpus_size": 24,
                "warmup_repetitions": 1,
                "repetitions": 2,
                "samples": 2,
            }
            (baseline / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (candidate / "manifest.json").write_text(
                json.dumps({**manifest, "label": "candidate"}), encoding="utf-8"
            )
            (baseline / "results.tsv").write_text(evaluation_output(), encoding="utf-8")
            (candidate / "results.tsv").write_text(
                evaluation_output(candidate=True), encoding="utf-8"
            )

            result = subprocess.run(
                [sys.executable, str(BENCH_SCRIPT), "compare", str(baseline), str(candidate)],
                cwd=REPO_ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            report = Path(result.stdout.strip()).read_text(encoding="utf-8")
            self.assertIn("Evaluation comparison: baseline vs candidate", report)
            self.assertIn("Nanoseconds/evaluation", report)
            self.assertIn("Checksums: `3992` / `3992` (match)", report)


if __name__ == "__main__":
    unittest.main()
