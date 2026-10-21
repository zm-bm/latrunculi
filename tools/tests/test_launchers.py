"""Exercise the shell entry points with a fake venv interpreter."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


BIN = Path(__file__).resolve().parents[2] / "bin"
ENTRIES = {
    ("benchmark",): "tools.benchmark",
    ("tune",): "tools.tuning.tune",
    ("openbench",): "openbench_client",
    ("openbench", "prepare"): "tools.openbench_prepare",
}
INTERPRETER = '''
import json
import os
import sys

print(json.dumps({"argv": sys.argv[1:], "cwd": os.getcwd()}))
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
        for name in {command[0] for command in ENTRIES}:
            shutil.copy2(BIN / name, self.bin / name)
        self.work = self.root / "another working directory"
        self.work.mkdir()

    def test_entries_preserve_arguments_streams_working_directory_and_exit(self):
        arguments = ["--config", "two words", "submit", "", "a'b", 'a"b',
                     "$(exit 9)", "`exit 8`", "*; &"]
        for command, entry in ENTRIES.items():
            with self.subTest(command=command):
                run = subprocess.run(
                    [str(self.bin / command[0]), *command[1:], *arguments], cwd=self.work,
                    env={**os.environ, "LATRUNCULI_LAUNCHER_TEST_EXIT": "37"},
                    capture_output=True, text=True, timeout=10,
                )
                self.assertEqual(run.returncode, 37)
                result = json.loads(run.stdout)
                self.assertEqual(result["argv"], ["-P", "-m", entry, *arguments])
                self.assertEqual(result["cwd"], str(self.work))
                self.assertEqual(run.stderr, "child stderr\n")

    def test_missing_environment_fails(self):
        self.python.unlink()
        for name in {command[0] for command in ENTRIES}:
            with self.subTest(launcher=name):
                result = subprocess.run([str(self.bin / name)], cwd=self.work,
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 1)
                self.assertEqual(result.stdout, "")
                self.assertTrue(result.stderr)

    def test_module_resolution_uses_checkout_instead_of_working_directory(self):
        self.python.unlink()
        self.python.symlink_to(sys.executable)
        for package in (self.tools, self.work / "tools"):
            package.mkdir(parents=True, exist_ok=True)
            (package / "__init__.py").write_text("")
        (self.work / "tools/__init__.py").write_text('raise RuntimeError("wrong checkout")\n')
        (self.tools / "benchmark.py").write_text(
            'import json, os, sys\nprint(json.dumps([os.getcwd(), sys.argv[1:]]))\n')
        result = subprocess.run([str(self.bin / "benchmark"), "relative/input", "two words"],
                                cwd=self.work, env={**os.environ, "PYTHONPATH": str(self.work)},
                                capture_output=True, text=True, check=True, timeout=10)
        self.assertEqual(json.loads(result.stdout), [str(self.work), ["relative/input", "two words"]])


if __name__ == "__main__":
    unittest.main()
