"""Exercise the shell entry points with a fake venv interpreter."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parent
BIN = TOOLS.parent / "bin"
ENTRIES = {
    "bench": "tools.analysis.bench",
    "tune": "tools.tuning.tune",
    "openbench-client": "openbench_client",
    "openbench-submit": "tools.openbench.submit",
    "uci-probe": "tools.analysis.uci_probe",
}
INTERPRETER = '''
import json
import os
import sys

print(json.dumps({"argv": sys.argv[1:], "cwd": os.getcwd(), "pid": os.getpid(),
                  "pythonpath": os.environ["PYTHONPATH"]}))
print("child stderr", file=sys.stderr)
sys.exit(int(os.environ.get("LATRUNCULI_LAUNCHER_TEST_EXIT", "0")))
'''


class LauncherTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="launcher checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.tools = self.root / "checkout with spaces/tools"
        self.bin = self.tools.parent / "bin"
        self.bin.mkdir(parents=True)
        self.python = self.tools / ".venv/bin/python"
        self.python.parent.mkdir(parents=True)
        self.python.write_text(f"#!{sys.executable}\n" + INTERPRETER)
        self.python.chmod(0o755)
        for name in ENTRIES:
            shutil.copy2(BIN / name, self.bin / name)
        self.work = self.root / "another working directory"
        self.work.mkdir()

    def test_entries_preserve_arguments_streams_working_directory_and_exit(self):
        arguments = ["--flag", "two words", "", "a'b", 'a"b', "$(exit 9)", "`exit 8`", "*; &"]
        for name, entry in ENTRIES.items():
            with self.subTest(launcher=name):
                with subprocess.Popen(
                    [str(self.bin / name), *arguments], cwd=self.work,
                    env={**os.environ, "LATRUNCULI_LAUNCHER_TEST_EXIT": "37", "PYTHONPATH": "/existing/path"},
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                ) as child:
                    stdout, stderr = child.communicate(timeout=10)
                    self.assertEqual(child.returncode, 37)
                    result = json.loads(stdout)
                    self.assertEqual(result["pid"], child.pid)  # exec replaces the shell.
                self.assertEqual(result["argv"], ["-P", "-m", entry, *arguments])
                self.assertEqual(result["cwd"], str(self.work))
                self.assertEqual(result["pythonpath"], f"{self.tools.parent}:/existing/path")
                self.assertEqual(stderr, "child stderr\n")

    def test_relative_invocation_locates_venv_without_activation(self):
        for name in ENTRIES:
            with self.subTest(launcher=name):
                result = subprocess.run([f"./bin/{name}", "--help"], cwd=self.tools.parent,
                                        env={**os.environ, "PYTHONPATH": ""},
                                        capture_output=True, text=True, check=True, timeout=10)
                self.assertEqual(json.loads(result.stdout)["cwd"], str(self.tools.parent))
                self.assertEqual(json.loads(result.stdout)["pythonpath"], str(self.tools.parent))

    def test_missing_or_nonexecutable_environment_reports_setup(self):
        for missing in (False, True):
            if missing:
                self.python.unlink()
            else:
                self.python.chmod(0o644)
            for name in ENTRIES:
                with self.subTest(launcher=name, missing=missing):
                    result = subprocess.run([str(self.bin / name)], cwd=self.work,
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("Python environment unavailable", result.stderr)
                    self.assertIn("python3 -m venv tools/.venv", result.stderr)
                    self.assertIn("-m pip install -r tools/requirements.txt", result.stderr)

    def test_module_resolution_uses_checkout_instead_of_working_directory(self):
        self.python.unlink()
        self.python.symlink_to(sys.executable)
        for package in (self.tools, self.tools / "analysis", self.work / "tools"):
            package.mkdir(parents=True, exist_ok=True)
            (package / "__init__.py").write_text("")
        (self.work / "tools/__init__.py").write_text('raise RuntimeError("wrong checkout")\n')
        (self.tools / "analysis/bench.py").write_text(
            'import json, os, sys\nprint(json.dumps([os.getcwd(), sys.argv[1:]]))\n')
        result = subprocess.run([str(self.bin / "bench"), "relative/input", "two words"],
                                cwd=self.work, env={**os.environ, "PYTHONPATH": str(self.work)},
                                capture_output=True, text=True, check=True, timeout=10)
        self.assertEqual(json.loads(result.stdout), [str(self.work), ["relative/input", "two words"]])


if __name__ == "__main__":
    unittest.main()
