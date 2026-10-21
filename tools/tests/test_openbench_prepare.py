import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from openbench_client.cli import normalize_settings

from tools import openbench_prepare as prepare


DEV = "1" * 40
BASE = "2" * 40
ENGINE = '''
from pathlib import Path
import sys

trace = Path({trace!r})
with trace.open("a") as output:
    output.write(" ".join(sys.argv[1:]) + "\\n")
print("engine diagnostic", file=sys.stderr, flush=True)
if {fail!r}:
    print("partial output", flush=True)
    sys.exit(7)
print("{nodes} nodes 100 ms 1000 nps")
'''


class PreparationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="openbench checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.candidate = self.engine("candidate", 1234)
        self.baseline = self.engine("baseline", 4321)
        self.enterContext(patch.object(prepare, "revision",
                                       side_effect={"HEAD": DEV, "main": BASE}.__getitem__))

    def engine(self, label, nodes, *, fail=False):
        path = self.root / f"{label} engine"
        path.write_text(f"#!{sys.executable}\n" + ENGINE.format(
            trace=str(self.root / f"{label}.calls"), fail=fail, nodes=nodes))
        path.chmod(0o700)
        return path

    def args(self, mode="sprt", output="result", self_play=False):
        args = [mode, "--candidate", "HEAD", "--candidate-engine", self.candidate,
                "--description", 'STC: "pawn chains"\nand candidate passers',
                "--output", self.root / output]
        if mode != "corpus" and not self_play:
            args += ["--baseline", "main", "--baseline-engine", self.baseline]
        if self_play:
            args += ["--self-play"]
        if mode == "games":
            args += ["--games", "2000"]
        if mode == "corpus":
            args += ["--worker-concurrency", "12"]
        return list(map(str, args))

    def run_cli(self, args):
        with contextlib.redirect_stdout(io.StringIO()) as stdout, \
                contextlib.redirect_stderr(io.StringIO()) as stderr:
            result = prepare.main(args)
        return result, stdout.getvalue(), stderr.getvalue()

    def settings(self, output="result"):
        settings = json.loads((self.root / output / "settings.json").read_text())
        normalize_settings(settings)  # Offline compatibility check against the pinned client.
        return settings

    def test_sprt_settings_and_bench_output(self):
        result, stdout, stderr = self.run_cli(self.args())
        self.assertEqual(result, 0, stderr)
        settings = self.settings()
        self.assertEqual((settings["dev_branch"], settings["base_branch"]), (DEV, BASE))
        self.assertEqual((settings["dev_bench"], settings["base_bench"]), (1234, 4321))
        self.assertEqual(settings["test_bounds"], "[0, 3]")
        self.assertEqual(settings["test_confidence"], "[0.05, 0.05]")
        self.assertEqual(settings["test_max_games"], "N/A")
        self.assertEqual(settings["info"], 'STC: "pawn chains"\nand candidate passers')
        for side in ("candidate", "baseline"):
            self.assertEqual((self.root / f"{side}.calls").read_text().splitlines(), ["bench"])
            self.assertIn("engine diagnostic", (self.root / "result" /
                                                f"{side}-bench.stderr.txt").read_text())
        self.assertIn(f"Settings: {self.root / 'result/settings.json'}", stdout)

    def test_fixed_game_comparison(self):
        result, _, stderr = self.run_cli(self.args("games"))
        self.assertEqual(result, 0, stderr)
        settings = self.settings()
        self.assertEqual(settings["test_mode"], "GAMES")
        self.assertEqual(settings["test_max_games"], 2000)
        self.assertEqual(settings["test_bounds"], "N/A")
        self.assertEqual(settings["test_confidence"], "N/A")
        self.assertEqual(settings["dev_time_control"], "10+0.1")
        self.assertNotEqual(settings["dev_branch"], settings["base_branch"])

    def test_self_play_runs_bench_once(self):
        result, _, stderr = self.run_cli(self.args("games", self_play=True))
        self.assertEqual(result, 0, stderr)
        settings = self.settings()
        self.assertEqual(settings["dev_branch"], settings["base_branch"])
        self.assertEqual(settings["dev_bench"], settings["base_bench"])
        self.assertFalse((self.root / "result/baseline-bench.txt").exists())
        self.assertFalse((self.root / "baseline.calls").exists())
        self.assertEqual((self.root / "candidate.calls").read_text().splitlines(), ["bench"])

    def test_corpus_defaults_and_rounding(self):
        for games, workload in ((None, 875), (42002, 876)):
            with self.subTest(games=games):
                output = f"corpus-{games}"
                args = self.args("corpus", output)
                if games is not None:
                    args += ["--games", str(games)]
                result, _, stderr = self.run_cli(args)
                self.assertEqual(result, 0, stderr)
                settings = self.settings(output)
                self.assertEqual(settings["test_max_games"], games if games is not None else 42000)
                self.assertEqual(settings["workload_size"], workload)
                self.assertEqual(settings["dev_time_control"], "4+0.04")
                self.assertEqual(settings["base_time_control"], "4+0.04")
                self.assertEqual(settings["dev_branch"], settings["base_branch"])
                self.assertEqual(settings["dev_bench"], settings["base_bench"])
                self.assertFalse((self.root / output / "baseline-bench.txt").exists())

    def test_invalid_arguments_do_not_run_engines(self):
        cases = [
            self.args("games") + ["--self-play"],
            self.args("games") + ["--games", "3"],
            self.args("corpus") + ["--worker-concurrency", "0"],
        ]
        for args in cases:
            with self.subTest(args=args), self.assertRaises(SystemExit) as error:
                self.run_cli(args)
            self.assertEqual(error.exception.code, 2)
        self.assertFalse((self.root / "result").exists())
        self.assertFalse((self.root / "candidate.calls").exists())

    def test_existing_output_and_dangling_symlink_are_preserved(self):
        output = self.root / "result"
        output.mkdir()
        marker = output / "keep"
        marker.write_text("existing evidence")
        for path in (output, self.root / "link"):
            if path != output:
                path.symlink_to(self.root / "missing")
            result, _, stderr = self.run_cli(self.args(output=path.name))
            self.assertEqual(result, 1)
            self.assertIn("already exists", stderr)
        self.assertEqual(marker.read_text(), "existing evidence")
        self.assertTrue((self.root / "link").is_symlink())
        self.assertFalse((self.root / "candidate.calls").exists())

    def test_baseline_failure_keeps_diagnostics_but_no_settings(self):
        self.baseline = self.engine("broken baseline", 100, fail=True)
        result, _, _ = self.run_cli(self.args())
        self.assertEqual(result, 1)
        output = self.root / "result"
        self.assertIn("1234 nodes", (output / "candidate-bench.txt").read_text())
        self.assertIn("partial output", (output / "baseline-bench.txt").read_text())
        self.assertIn("engine diagnostic", (output / "baseline-bench.stderr.txt").read_text())
        self.assertFalse((output / "settings.json").exists())


if __name__ == "__main__":
    unittest.main()
