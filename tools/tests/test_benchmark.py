import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools import benchmark, paths


FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
CHILD = r'''
import csv
import json
from pathlib import Path
import sys

trace = Path(TRACE)
previous = trace.read_text().splitlines() if trace.exists() else []
call = sum(json.loads(line)[0] == LABEL for line in previous)
with trace.open("a") as output:
    output.write(json.dumps([LABEL, sys.argv[1:]]) + "\n")
if MODE == "failure":
    print("partial output", flush=True)
    print("child failed", file=sys.stderr, flush=True)
    sys.exit(7)
def option(name):
    return sys.argv[sys.argv.index(name) + 1]
writer = csv.DictWriter(sys.stdout, fieldnames=FIELDS, delimiter="\t")
writer.writeheader()
for case in ("one", "two"):
    writer.writerow(dict(result_format="search_measurement_v5", case=case,
        fen=FEN, limit_type="depth", limit_value=option("--depth"),
        threads=option("--threads"), hash_mb=option("--hash"),
        completed_depth=option("--depth"), static_score=0,
        score=10 + call if MODE == "changing" else 10, nodes=90 if MODE == "tree_change" else 100,
        best_move="e2e4", pv="e2e4 e7e5",
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
            f'{" ".join(FEN.split()[:4])} id "{case}"; c0 "annotation"; bm ignored;'
            for case in ("one", "two")) + "\n")

    def child(self, label="candidate", mode="ok"):
        path = self.root / f"{label} engine"
        fields = ["result_format", "case", "fen", *benchmark.CONFIG,
                  *benchmark.SIGNATURE, "total_ns"]
        path.write_text(f"#!{sys.executable}\nMODE={mode!r}\nLABEL={label!r}\n"
                        f"TRACE={str(self.trace)!r}\nFEN={FEN!r}\nFIELDS={fields!r}\n" + CHILD)
        path.chmod(0o700)
        return path

    def run_cli(self, args):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            benchmark.main(list(map(str, args)))
        return output.getvalue()

    def test_refuses_existing_output_before_running_a_child(self):
        output = self.root / "existing"
        output.mkdir()
        marker = output / "keep"
        marker.write_text("existing measurements")
        link = self.root / "tools/output/linked"
        link.parent.mkdir(parents=True)
        link.symlink_to(self.root / "absent")
        binary = self.child()
        with patch.object(paths, "ROOT", self.root), patch.object(benchmark, "require_untraced"):
            for command in (["run", binary], ["compare", binary, binary]):
                for destination in (["--output", output], ["--name", "linked"]):
                    with self.subTest(command=command[0], destination=destination[0]), \
                            self.assertRaisesRegex(SystemExit, "exists"):
                        self.run_cli(command + ["--suite", self.suite] + destination)
        self.assertFalse(self.trace.exists())
        self.assertEqual(marker.read_text(), "existing measurements")
        self.assertTrue(link.is_symlink())
        self.assertFalse((self.root / "absent").exists())

    def test_named_and_explicit_comparisons_match_outside_checkout(self):
        checkout = self.root / "checkout with spaces"
        binaries = []
        for name in ("my-base", "release-dev"):
            binary = checkout / "build" / name / "latrunculi-benchmark"
            binary.parent.mkdir(parents=True)
            self.child(name).rename(binary)
            binaries.append(binary)
        outputs = [checkout / "tools/output/default", self.root / "explicit"]
        selections = (["my-base", "--name", "default"],
                      [binaries[0], binaries[1], "--output", "explicit"])
        with patch.object(paths, "ROOT", checkout), patch.object(benchmark, "require_untraced"), \
                contextlib.chdir(self.root):
            for selection in selections:
                self.run_cli(["compare", *selection, "--suite", self.suite.name,
                              "--pairs", "2", "--idle-seconds", "0.001"])
        self.assertEqual(len({(directory / "summary.txt").read_bytes() for directory in outputs}), 1)

    def test_corpus_names_and_dot_paths_preserve_caller_relative_paths(self):
        checkout = self.root / "checkout"
        binary = checkout / "build/release-dev/latrunculi-benchmark"
        binary.parent.mkdir(parents=True)
        self.child().rename(binary)
        (self.root / "release-dev").write_bytes(binary.read_bytes())
        (self.root / "release-dev").chmod(0o700)
        reference = checkout / "tools/output/named/corpus-1.tsv"
        with patch.object(paths, "ROOT", checkout), contextlib.chdir(self.root):
            self.run_cli(["run", "release-dev", "--name", "named",
                          "--suite", self.suite.name, "--repeats", "2"])
            self.run_cli(["run", "./release-dev", "--output", "explicit",
                          "--suite", self.suite.name,
                          "--reference", reference.relative_to(self.root)])
        self.assertEqual(reference.read_bytes(), (self.root / "explicit/corpus-1.tsv").read_bytes())
        self.assertEqual(reference.read_bytes(), reference.with_name("corpus-2.tsv").read_bytes())

    def comparison_args(self, mode="ok"):
        return ["compare", self.child("baseline", "warmup_noise"), self.child(mode=mode),
                "--suite", self.suite, "--output", self.root / "comparison",
                "--idle-seconds", "0.001"]

    def test_run_defaults_to_one_pass_and_accepts_custom_settings(self):
        self.run_cli(["run", self.child(), "--suite", self.suite,
                      "--depth", "8", "--hash", "64", "--threads", "2",
                      "--output", self.root / "single"])
        self.assertEqual(len(self.trace.read_text().splitlines()), 1)
        measured = benchmark.load_run(self.root / "single/corpus-1.tsv")
        self.assertEqual(measured.config, ("depth", "8", "2", "64"))

    def test_compare_defaults_to_six_pairs_and_excludes_warmups(self):
        with patch.object(benchmark, "require_untraced"):
            output = self.run_cli(self.comparison_args() + ["--exact-tree"])
        self.assertIn("Candidate search time: +0.00% (median balanced ratio)", output)
        calls = [json.loads(line)[0] for line in self.trace.read_text().splitlines()]
        self.assertEqual(calls, ["baseline", "candidate"] +
                         ["baseline", "candidate", "candidate", "baseline"] * 3)
        summary = (self.root / "comparison/summary.txt").read_text()
        self.assertIn("median_balanced_search_time_ratio=1.000000000", summary)
        self.assertIn("pairs=6", summary)

    def test_short_tree_changing_comparison_and_saved_summary_agree(self):
        with patch.object(benchmark, "require_untraced"):
            output = self.run_cli(self.comparison_args("tree_change") + ["--pairs", "2", "--depth", "8"])
        summary = (self.root / "comparison/summary.txt").read_text()
        self.assertIn("geometric_mean_node_ratio=0.900000000", summary)
        self.assertEqual(output.split("\n\n", 1)[1], summary)

    def test_compare_preserves_failure_without_summary(self):
        for mode in ("changing", "failure"):
            with self.subTest(mode=mode), patch.object(benchmark, "require_untraced"):
                args = self.comparison_args(mode) + ["--pairs", "2"]
                args[args.index("--output") + 1] = self.root / mode
                with contextlib.redirect_stdout(io.StringIO()) as output:
                    with self.assertRaises(SystemExit):
                        benchmark.main(list(map(str, args)))
                self.assertNotIn("Candidate search time:", output.getvalue())
                self.assertTrue((self.root / mode / "warmup-candidate.tsv").exists())
                self.assertFalse((self.root / mode / "summary.txt").exists())

    def test_exact_tree_mismatch_stops_after_warmups(self):
        with patch.object(benchmark, "require_untraced"), \
                self.assertRaisesRegex(SystemExit, "exact-tree warmups"):
            self.run_cli(self.comparison_args("tree_change") + ["--pairs", "2", "--exact-tree"])
        self.assertEqual(len(self.trace.read_text().splitlines()), 2)
        self.assertFalse((self.root / "comparison/summary.txt").exists())


if __name__ == "__main__":
    unittest.main()
