import contextlib
import io
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from openbench_client.cli import normalize_settings

from tools.openbench import submit


DEV = "1" * 40
BASE = "2" * 40
ENGINE = '''
from pathlib import Path
import sys
import time

trace = Path({trace!r})
calls = len(trace.read_text().splitlines()) if trace.exists() else 0
with trace.open("a") as output:
    output.write(" ".join(sys.argv[1:]) + "\\n")
print("engine diagnostic", file=sys.stderr, flush=True)
if {mode!r} == "failure":
    print("partial output", flush=True)
    sys.exit(7)
if {mode!r} == "timeout":
    print("started", flush=True)
    time.sleep(10)
if {mode!r} == "malformed":
    print("no fingerprint")
else:
    nodes = {nodes} + (calls if {mode!r} == "changing" else 0)
    print(f"{{nodes}} nodes 100 ms 1000 nps")
'''


class SubmissionTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="openbench checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.candidate = self.engine("candidate", 1234)
        self.baseline = self.engine("baseline", 4321)
        self.client = self.enterContext(patch.object(submit, "run_client", return_value=0))
        self.resolve = self.enterContext(patch.object(submit, "revision",
                                                    side_effect={"HEAD": DEV, "main": BASE}.__getitem__))

    def engine(self, label, nodes, mode="ok"):
        path = self.root / f"{label} engine"
        path.write_text(f"#!{sys.executable}\n" + ENGINE.format(
            trace=str(self.root / f"{label}.calls"), mode=mode, nodes=nodes))
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
            result = submit.main(args)
        return result, stdout.getvalue(), stderr.getvalue()

    def settings(self, output="result"):
        settings = json.loads((self.root / output / "settings.json").read_text())
        normalize_settings(settings)  # Offline compatibility check against the pinned client.
        return settings

    def test_sprt_defaults_and_saved_settings_handoff(self):
        config = self.root / "credentials with spaces.env"
        result, stdout, stderr = self.run_cli(self.args() + ["--config", str(config)])
        self.assertEqual(result, 0, stderr)
        settings = self.settings()
        self.assertEqual((settings["dev_branch"], settings["base_branch"]), (DEV, BASE))
        self.assertEqual((settings["dev_bench"], settings["base_bench"]), (1234, 4321))
        self.assertEqual(settings["test_bounds"], "[0, 3]")
        self.assertEqual(settings["test_confidence"], "[0.05, 0.05]")
        self.assertEqual(settings["test_max_games"], "N/A")
        self.assertEqual(settings["info"], 'STC: "pawn chains"\nand candidate passers')
        for side in ("candidate", "baseline"):
            self.assertEqual((self.root / f"{side}.calls").read_text().splitlines(), ["bench", "bench"])
            self.assertIn("engine diagnostic", (self.root / "result" / side /
                                                "fingerprint-2.stderr.txt").read_text())
        command = self.client.call_args.args[0]
        self.assertEqual(command, [sys.executable, "-m", "openbench_client", "--config", str(config),
                                   "submit", str(self.root / "result/settings.json"), "--dry-run"])
        self.client.assert_called_once()
        handoff = shlex.split(stdout.splitlines()[-1])
        launcher = Path(submit.__file__).resolve().parents[1] / "openbench-client"
        expected_launcher = "./tools/openbench-client" if Path.cwd() == launcher.parent.parent else str(launcher)
        self.assertEqual(handoff, [expected_launcher, *command[3:-1],
                                   "--output", str(self.root / "result/submission")])
        self.assertFalse((self.root / "result/submission").exists())

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

    def test_self_play_fingerprints_one_binary_twice(self):
        result, _, stderr = self.run_cli(self.args("games", self_play=True))
        self.assertEqual(result, 0, stderr)
        settings = self.settings()
        self.assertEqual(settings["dev_branch"], settings["base_branch"])
        self.assertEqual(settings["dev_bench"], settings["base_bench"])
        self.assertFalse((self.root / "result/baseline").exists())
        self.assertFalse((self.root / "baseline.calls").exists())
        self.assertEqual((self.root / "candidate.calls").read_text().splitlines(), ["bench", "bench"])
        self.resolve.assert_called_once_with("HEAD")

    def test_corpus_defaults_and_rounding(self):
        for games, workload in ((None, 875), (42002, 876), (2, 1)):
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
                self.assertFalse((self.root / output / "baseline").exists())

    def test_submit_delegates_once_without_separate_dry_run(self):
        result, stdout, stderr = self.run_cli(self.args() + ["--submit"])
        self.assertEqual(result, 0, stderr)
        self.client.assert_called_once_with([sys.executable, "-m", "openbench_client", "submit",
                                            str(self.root / "result/settings.json"), "--output",
                                            str(self.root / "result/submission")])
        self.assertNotIn("Dry run passed", stdout)

    def test_invalid_arguments_do_not_run_engines_or_client(self):
        cases = [
            self.args("games") + ["--self-play"],
            self.args("games", self_play=True) + ["--baseline", "main"],
            self.args("games", self_play=True) + ["--baseline-engine", str(self.baseline)],
            self.args("games", self_play=True)[:-3] + ["--games", "2000"],
            self.args("games") + ["--games", "3"],
            self.args("games") + ["--games", "0"],
            self.args("corpus") + ["--worker-concurrency", "0"],
            self.args("corpus") + ["--baseline", "main"],
            self.args() + ["--games", "100"],
            self.args() + ["--timeout", "nan"],
            self.args() + ["--description", "  "],
        ]
        for args in cases:
            with self.subTest(args=args), self.assertRaises(SystemExit) as error:
                self.run_cli(args)
            self.assertEqual(error.exception.code, 2)
        self.assertFalse((self.root / "result").exists())
        self.assertFalse((self.root / "candidate.calls").exists())
        self.client.assert_not_called()

    def test_invalid_inputs_fail_before_engines_run(self):
        self.baseline.unlink()
        result, _, stderr = self.run_cli(self.args())
        self.assertEqual(result, 1)
        self.assertIn("not an executable", stderr)
        self.assertFalse((self.root / "result").exists())
        self.assertFalse((self.root / "candidate.calls").exists())
        self.client.assert_not_called()

    def test_invalid_revision_fails_before_engines_run(self):
        self.resolve.side_effect = ValueError("cannot resolve commit")
        result, _, stderr = self.run_cli(self.args())
        self.assertEqual(result, 1)
        self.assertIn("cannot resolve commit", stderr)
        self.assertFalse((self.root / "candidate.calls").exists())
        self.client.assert_not_called()

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
        self.client.assert_not_called()

    def test_bad_templates_fail_before_engines_run(self):
        for text in ("{broken", "[]"):
            with self.subTest(text=text), patch.object(submit, "TEMPLATES", self.root):
                (self.root / "sprt.json").write_text(text)
                result, _, _ = self.run_cli(self.args())
                self.assertEqual(result, 1)
        self.assertFalse((self.root / "candidate.calls").exists())
        self.client.assert_not_called()

    def test_fingerprint_failures_keep_diagnostics_and_no_settings(self):
        for mode in ("malformed", "changing", "failure", "timeout"):
            with self.subTest(mode=mode):
                self.candidate = self.engine(mode, 100, mode)
                args = self.args(output=mode + "-output") + ["--timeout", "0.2"]
                result, _, stderr = self.run_cli(args)
                self.assertEqual(result, 1, stderr)
                output = self.root / (mode + "-output")
                self.assertFalse((output / "settings.json").exists())
                self.assertTrue((output / "candidate/fingerprint-1.txt").exists())
                self.assertIn("engine diagnostic", (output / "candidate/fingerprint-1.stderr.txt").read_text())
        self.client.assert_not_called()

    def test_baseline_failure_keeps_candidate_but_no_settings(self):
        self.baseline = self.engine("broken baseline", 100, "failure")
        result, _, _ = self.run_cli(self.args())
        self.assertEqual(result, 1)
        self.assertTrue((self.root / "result/candidate/fingerprint-2.txt").exists())
        self.assertFalse((self.root / "result/settings.json").exists())
        self.client.assert_not_called()

    def test_client_failure_or_interruption_never_retries(self):
        for actual in (False, True):
            for interrupted in (False, True):
                with self.subTest(actual=actual, interrupted=interrupted):
                    self.client.reset_mock()
                    self.client.side_effect = KeyboardInterrupt if interrupted else None
                    self.client.return_value = 7
                    output = f"client-{actual}-{interrupted}"
                    args = self.args(output=output) + (["--submit"] if actual else [])
                    result, stdout, stderr = self.run_cli(args)
                    self.assertEqual(result, 130 if interrupted else 7)
                    self.client.assert_called_once()
                    self.assertTrue((self.root / output / "settings.json").exists())
                    self.assertNotIn("Dry run passed", stdout)
                    if actual:
                        self.assertIn("server state", stderr)
                    elif not interrupted:
                        self.assertIn("retry the client dry run", stderr)
                        retry = shlex.split(stderr.splitlines()[-1])
                        self.assertTrue(retry[0].endswith("/tools/openbench-client"))
                        self.assertEqual(retry[1:], self.client.call_args.args[0][3:])

    def test_interrupted_settings_write_does_not_publish_partial_json(self):
        path = self.root / "settings.json"
        with patch.object(submit.json, "dump", side_effect=KeyboardInterrupt), \
                self.assertRaises(KeyboardInterrupt):
            submit.write_settings(path, {"info": "description"})
        self.assertFalse(path.exists())
        self.assertEqual(list(self.root.glob(".settings-*")), [])


class CommandTests(unittest.TestCase):
    def test_display_command_uses_relative_launcher_only_at_repository_root(self):
        with tempfile.TemporaryDirectory(prefix="command paths ") as directory:
            root = Path(directory).resolve()
            fake_file = root / "tools/openbench/submit.py"
            command = [sys.executable, "-m", "openbench_client", "--config", "config with 'quotes'",
                       "submit", "settings with spaces.json", "--dry-run"]
            with patch.object(submit, "__file__", str(fake_file)):
                for cwd, expected in ((root, "./tools/openbench-client"),
                                      (root / "elsewhere", str(root / "tools/openbench-client"))):
                    with self.subTest(cwd=cwd), patch.object(submit.Path, "cwd", return_value=cwd):
                        rendered = submit.display_client_command(command)
                        self.assertEqual(shlex.split(rendered), [expected, *command[3:]])

    def test_revision_resolution_uses_commit_and_end_of_options(self):
        with patch.object(submit.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, DEV + "\n", "")
            self.assertEqual(submit.revision("HEAD"), DEV)
            self.assertEqual(run.call_args.args[0],
                             ["git", "rev-parse", "--verify", "--end-of-options", "HEAD^{commit}"])
            run.return_value = subprocess.CompletedProcess([], 128, "", "fatal: unknown revision")
            with self.assertRaisesRegex(ValueError, "cannot resolve commit"):
                submit.revision("--bad-revision")

    def test_client_invocation_inherits_output_without_shell(self):
        command = [sys.executable, "-m", "openbench_client", "submit", "settings.json", "--dry-run"]
        with patch.object(submit.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess(command, 4)
            self.assertEqual(submit.run_client(command), 4)
            run.assert_called_once_with(command, check=False)


if __name__ == "__main__":
    unittest.main()
