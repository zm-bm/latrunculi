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
ENTRIES = {
    "bench": ["analysis/bench.py"],
    "tune": ["tuning/tune.py"],
    "openbench-client": ["-m", "openbench_client"],
    "openbench-submit": ["openbench/submit.py"],
    "uci-probe": ["analysis/uci_probe.py"],
}
INTERPRETER = '''
import json
import os
import sys

print(json.dumps({"argv": sys.argv[1:], "cwd": os.getcwd(), "pid": os.getpid()}))
print("child stderr", file=sys.stderr)
sys.exit(int(os.environ.get("LATRUNCULI_LAUNCHER_TEST_EXIT", "0")))
'''


class LauncherTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="launcher checks ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.tools = self.root / "checkout with spaces/tools"
        self.python = self.tools / ".venv/bin/python"
        self.python.parent.mkdir(parents=True)
        self.python.write_text(f"#!{sys.executable}\n" + INTERPRETER)
        self.python.chmod(0o755)
        for name in ENTRIES:
            shutil.copy2(TOOLS / name, self.tools / name)
        self.work = self.root / "another working directory"
        self.work.mkdir()

    def test_entries_preserve_arguments_streams_working_directory_and_exit(self):
        arguments = ["--flag", "two words", "", "a'b", 'a"b', "$(exit 9)", "`exit 8`", "*; &"]
        for name, entry in ENTRIES.items():
            with self.subTest(launcher=name):
                with subprocess.Popen(
                    [str(self.tools / name), *arguments], cwd=self.work,
                    env={**os.environ, "LATRUNCULI_LAUNCHER_TEST_EXIT": "37"},
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                ) as child:
                    stdout, stderr = child.communicate(timeout=10)
                    self.assertEqual(child.returncode, 37)
                    result = json.loads(stdout)
                    self.assertEqual(result["pid"], child.pid)  # exec replaces the shell.
                expected = entry if entry[0] == "-m" else [str(self.tools / entry[0])]
                self.assertEqual(result["argv"], expected + arguments)
                self.assertEqual(result["cwd"], str(self.work))
                self.assertEqual(stderr, "child stderr\n")

    def test_relative_invocation_locates_venv_without_activation(self):
        for name in ENTRIES:
            with self.subTest(launcher=name):
                result = subprocess.run([f"./tools/{name}", "--help"], cwd=self.tools.parent,
                                        capture_output=True, text=True, check=True, timeout=10)
                self.assertEqual(json.loads(result.stdout)["cwd"], str(self.tools.parent))

    def test_missing_or_nonexecutable_environment_reports_setup(self):
        for missing in (False, True):
            if missing:
                self.python.unlink()
            else:
                self.python.chmod(0o644)
            for name in ENTRIES:
                with self.subTest(launcher=name, missing=missing):
                    result = subprocess.run([str(self.tools / name)], cwd=self.work,
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("Python environment unavailable", result.stderr)
                    self.assertIn("python3 -m venv tools/.venv", result.stderr)
                    self.assertIn("-m pip install -r tools/requirements.txt", result.stderr)


if __name__ == "__main__":
    unittest.main()
