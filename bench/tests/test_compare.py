from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from bench.benchlib.common import FORMAT_VERSION, read_tsv
from bench.benchlib.evaluation import EVALUATION_THROUGHPUT_FORMAT


REPO_ROOT = Path(__file__).resolve().parents[2]
BENCH_SCRIPT = REPO_ROOT / "bench/bench.py"


class ComparisonContractTest(unittest.TestCase):
    def test_rejects_incompatible_manifests(self) -> None:
        manifest = {
            "format_version": FORMAT_VERSION,
            "result_format": "perft",
            "suite": "perft",
            "profile": "smoke",
        }
        cases = {
            "missing version": (
                {key: value for key, value in manifest.items() if key != "format_version"},
                manifest,
                "baseline run without format_version",
            ),
            "unsupported version": (
                {**manifest, "format_version": FORMAT_VERSION + 1},
                manifest,
                "baseline run with unsupported format_version",
            ),
            "different versions": (
                manifest,
                {**manifest, "format_version": FORMAT_VERSION + 1},
                "candidate run with unsupported format_version",
            ),
            "different profiles": (
                manifest,
                {**manifest, "profile": "standard"},
                "runs with different profile",
            ),
            "different evaluation corpora": (
                {
                    **manifest,
                    "result_format": EVALUATION_THROUGHPUT_FORMAT,
                    "suite": "eval",
                    "corpus_sha256": "baseline",
                    "corpus_version": "1",
                    "corpus_size": 24,
                    "warmup_repetitions": 1,
                    "repetitions": 2,
                    "samples": 2,
                },
                {
                    **manifest,
                    "result_format": EVALUATION_THROUGHPUT_FORMAT,
                    "suite": "eval",
                    "corpus_sha256": "candidate",
                    "corpus_version": "1",
                    "corpus_size": 24,
                    "warmup_repetitions": 1,
                    "repetitions": 2,
                    "samples": 2,
                },
                "runs with different corpus_sha256",
            ),
        }

        for name, (baseline, candidate, expected_error) in cases.items():
            with self.subTest(name=name):
                with tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    baseline_dir = root / "baseline"
                    candidate_dir = root / "candidate"
                    baseline_dir.mkdir()
                    candidate_dir.mkdir()
                    (baseline_dir / "manifest.json").write_text(
                        json.dumps(baseline), encoding="utf-8"
                    )
                    (candidate_dir / "manifest.json").write_text(
                        json.dumps(candidate), encoding="utf-8"
                    )

                    result = subprocess.run(
                        [
                            sys.executable,
                            str(BENCH_SCRIPT),
                            "compare",
                            str(baseline_dir),
                            str(candidate_dir),
                        ],
                        cwd=REPO_ROOT,
                        text=True,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                    )

                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(expected_error, result.stderr)

    def test_rejects_incompatible_tsv_header(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.tsv"
            path.write_text("case\tnodes\nstartpos\t20\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "unexpected TSV header"):
                read_tsv(path, ["result_format", "case", "nodes"])


if __name__ == "__main__":
    unittest.main()
