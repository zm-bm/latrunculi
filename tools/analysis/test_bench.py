import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.analysis import bench, bench_results


FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
CHILD = r'''
import csv
import json
from pathlib import Path
import sys
import time

trace = Path(TRACE)
previous = trace.read_text().splitlines() if trace.exists() else []
call = sum(json.loads(line)[0] == LABEL for line in previous)
with trace.open("a") as output:
    output.write(json.dumps([LABEL, sys.argv[1:]]) + "\n")
if MODE == "failure":
    print("partial output", flush=True)
    print("child failed", file=sys.stderr, flush=True)
    sys.exit(7)
if MODE == "timeout":
    print("started", flush=True)
    time.sleep(10)
if sys.argv[1:] == ["bench"]:
    if MODE == "missing_fingerprint":
        print("no benchmark result")
    else:
        print(f"{1234 + (call if MODE == 'changing' else 0)} nodes 999 nps")
    sys.exit(0)
def option(name):
    return sys.argv[sys.argv.index(name) + 1]
writer = csv.DictWriter(sys.stdout, fieldnames=FIELDS, delimiter="\t")
writer.writeheader()
for case in ("one", "two"):
    if MODE == "truncated" and case == "two":
        break
    writer.writerow(dict(result_format="search_measurement_v5", case=case,
        fen=FEN, limit_type="depth", limit_value=option("--depth"),
        threads=option("--threads"), hash_mb="64" if MODE == "settings" else option("--hash"),
        completed_depth=option("--depth"), static_score=0,
        score=10 + call if MODE == "changing" else 10, nodes=90 if MODE == "tree_change" else 100,
        best_move="e2e4", pv="e2e4 e7e6 e4e6" if MODE == "illegal" else "e2e4 e7e5",
        total_ns=1000000 if MODE == "warmup_noise" and call == 0 else 1000))
'''


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="search checks ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.trace = self.root / "calls.jsonl"
        self.suite = self.root / "suite.epd"
        self.suite.write_text("\n".join(
            f'{" ".join(FEN.split()[:4])} id "{case}"; c0 "annotation; id ignored";'
            for case in ("one", "two")) + "\n")

    def child(self, label="candidate", mode="ok"):
        path = self.root / f"{label} engine"
        fields = ["result_format", "case", "fen", *bench_results.CONFIG,
                  *bench_results.SIGNATURE, "total_ns"]
        path.write_text(f"#!{sys.executable}\nMODE={mode!r}\nLABEL={label!r}\n"
                        f"TRACE={str(self.trace)!r}\nFEN={FEN!r}\nFIELDS={fields!r}\n" + CHILD)
        path.chmod(0o700)
        return path

    def run_cli(self, args):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            bench.main(list(map(str, args)))
        return output.getvalue()

    def collect_corpus(self, label="corpus", mode="ok", *extra):
        output = self.root / label
        self.run_cli(["run", self.child(mode=mode), "--suite", self.suite,
                      "--repeats", "2",
                      "--output", output, *extra])
        return output

    def test_corpus_repeats_and_reference_with_paths_containing_spaces(self):
        first = self.collect_corpus()
        reference = first / "corpus-1.tsv"
        second = self.collect_corpus("second", "ok", "--reference", reference)
        self.assertEqual(reference.read_bytes(), (second / "corpus-2.tsv").read_bytes())
        self.assertEqual(len(self.trace.read_text().splitlines()), 4)

    def test_rejects_partial_illegal_changed_and_misconfigured_runs(self):
        for mode, message in (("truncated", "case set"), ("illegal", "illegal PV"),
                              ("changing", "signature mismatches"),
                              ("settings", "settings differ")):
            with self.subTest(mode=mode), self.assertRaisesRegex(SystemExit, message):
                self.collect_corpus(mode, mode)
            self.assertTrue((self.root / mode / "corpus-1.tsv").is_file())

    def test_preserves_failure_and_timeout_output(self):
        for mode, message in (("failure", "exit status 7"), ("timeout", "timed out")):
            with self.subTest(mode=mode), self.assertRaisesRegex(SystemExit, message):
                self.collect_corpus(mode, mode, "--timeout", "0.2")
            raw = (self.root / mode / "corpus-1.tsv").read_text()
            self.assertTrue(raw.strip())
        self.assertIn("child failed", (self.root / "failure/corpus-1.stderr.txt").read_text())

    def test_refuses_existing_output_before_running_a_child(self):
        output = self.collect_corpus()
        before = self.trace.read_bytes()
        with self.assertRaisesRegex(SystemExit, "exists"):
            self.run_cli(["run", self.child(), "--suite", self.suite,
                          "--output", output])
        self.assertEqual(before, self.trace.read_bytes())

    def test_fingerprint_repeats_reference_and_failures(self):
        engine = self.child()
        first = self.root / "fingerprint"
        self.run_cli(["fingerprint", engine, "--output", first])
        self.assertEqual(bench.fingerprint_nodes(first / "fingerprint-2.txt"), 1234)
        self.run_cli(["fingerprint", engine, "--output", self.root / "second",
                      "--reference", first / "fingerprint-1.txt"])
        for mode, message in (("changing", "fingerprint differs"),
                              ("missing_fingerprint", "expected one positive")):
            with self.subTest(mode=mode), self.assertRaisesRegex(SystemExit, message):
                self.run_cli(["fingerprint", self.child(mode=mode),
                              "--output", self.root / mode])

    def comparison_args(self, mode="ok"):
        return ["compare", self.child("baseline", "warmup_noise"), self.child(mode=mode),
                "--suite", self.suite, "--output", self.root / "comparison",
                "--idle-seconds", "0.001"]

    def test_run_defaults_to_one_pass_and_accepts_custom_settings(self):
        self.run_cli(["run", self.child(), "--suite", self.suite,
                      "--depth", "8", "--hash", "64", "--threads", "2",
                      "--output", self.root / "single"])
        self.assertEqual(len(self.trace.read_text().splitlines()), 1)
        measured = bench_results.load_run(self.root / "single/corpus-1.tsv")
        self.assertEqual(measured.config, ("depth", "8", "2", "64"))

    def test_compare_defaults_to_six_pairs_and_excludes_warmups(self):
        with patch.object(bench, "require_untraced"):
            self.run_cli(self.comparison_args() + ["--exact-tree"])
        calls = [json.loads(line)[0] for line in self.trace.read_text().splitlines()]
        self.assertEqual(calls, ["baseline", "candidate"] +
                         ["baseline", "candidate", "candidate", "baseline"] * 3)
        summary = (self.root / "comparison/summary.txt").read_text()
        self.assertIn("median_balanced_search_time_ratio=1.000000000", summary)
        self.assertIn("pairs=6", summary)
        metrics = (self.root / "comparison/timing-load.jsonl").read_text().splitlines()
        self.assertEqual(len(metrics), 15)
        self.assertEqual(json.loads(metrics[0])["run"], "idle")

    def test_short_tree_changing_comparison_and_saved_summary_agree(self):
        with patch.object(bench, "require_untraced"):
            self.run_cli(self.comparison_args("tree_change") + ["--pairs", "2", "--depth", "8"])
        summary = (self.root / "comparison/summary.txt").read_text()
        self.assertIn("geometric_mean_node_ratio=0.900000000", summary)
        args = ["summarize"]
        for pair, order in ((1, "BC"), (2, "CB")):
            args.extend(["--pair", order, self.root / f"comparison/pair-{pair}-baseline.tsv",
                         self.root / f"comparison/pair-{pair}-candidate.tsv"])
        self.assertEqual(self.run_cli(args), summary)

    def test_compare_rejects_bad_pair_count_before_running_children(self):
        with self.assertRaisesRegex(SystemExit, "even pair count"):
            self.run_cli(self.comparison_args() + ["--pairs", "3"])
        self.assertFalse(self.trace.exists())

    def test_compare_preserves_failure_without_summary(self):
        for mode in ("changing", "truncated", "illegal", "failure", "timeout", "settings"):
            with self.subTest(mode=mode), patch.object(bench, "require_untraced"):
                args = self.comparison_args(mode) + ["--pairs", "2", "--timeout", "0.2"]
                args[args.index("--output") + 1] = self.root / mode
                with self.assertRaises(SystemExit):
                    self.run_cli(args)
                self.assertTrue((self.root / mode / "warmup-candidate.tsv").exists())
                self.assertFalse((self.root / mode / "summary.txt").exists())

    def test_exact_tree_mismatch_stops_after_warmups(self):
        with patch.object(bench, "require_untraced"), \
                self.assertRaisesRegex(SystemExit, "exact-tree warmups"):
            self.run_cli(self.comparison_args("tree_change") + ["--pairs", "2", "--exact-tree"])
        self.assertEqual(len(self.trace.read_text().splitlines()), 2)
        self.assertFalse((self.root / "comparison/summary.txt").exists())

    def test_tracing_and_affinity_detection(self):
        with patch.object(Path, "exists", return_value=True), \
                patch.object(Path, "read_text", return_value="TracerPid:\t123\n"):
            with self.assertRaisesRegex(ValueError, "without tracing"):
                bench.require_untraced()
        with patch.object(os, "sched_getaffinity", return_value={3, 11}, create=True), \
                patch.object(bench.shutil, "which", return_value="/usr/bin/taskset"), \
                patch.object(Path, "exists", return_value=True), \
                patch.object(Path, "read_text", return_value="3,11\n"):
            prefix, siblings = bench.affinity(3)
            self.assertEqual(prefix, ["/usr/bin/taskset", "--cpu-list", "3"])
            self.assertEqual(siblings, [11])
            with self.assertRaisesRegex(ValueError, "not available"):
                bench.affinity(2)

    def test_suite_rejects_duplicate_ids(self):
        self.suite.write_text(self.suite.read_text().replace('id "two"', 'id "one"'))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            bench.load_suite(self.suite)


if __name__ == "__main__":
    unittest.main()
