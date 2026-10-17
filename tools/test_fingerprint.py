import contextlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from . import fingerprint


class FingerprintTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="fingerprint paths ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))

    def engine(self, program):
        path = self.root / "fake engine"
        path.write_text(f"#!{sys.executable}\nimport sys\nassert sys.argv[1:] == ['bench']\n"
                        "print('diagnostic', file=sys.stderr, flush=True)\n" + program + "\n")
        path.chmod(0o700)
        return path

    def test_parser_requires_one_positive_node_count(self):
        path = self.root / "bench.txt"
        path.write_text("engine diagnostic\n 1234 nodes 10 ms 123400 nps\n")
        self.assertEqual(fingerprint.fingerprint_nodes(path), 1234)
        for output in ("", "0 nodes\n", "-1 nodes\n", "123 nodes\n456 nodes\n"):
            with self.subTest(output=output):
                path.write_text(output)
                with self.assertRaisesRegex(ValueError, "one positive bench node count"):
                    fingerprint.fingerprint_nodes(path)

    def test_collect_repeats_reference_and_existing_output_refusal(self):
        engine = self.engine("print('1234 nodes 10 ms 123400 nps')")
        output = self.root / "first"
        self.assertEqual(fingerprint.fingerprint_engine(engine, output), 1234)
        self.assertEqual((output / "fingerprint-2.stderr.txt").read_text(), "diagnostic\n")
        reference = output / "fingerprint-1.txt"
        self.assertEqual(fingerprint.fingerprint_engine(
            engine, self.root / "second", reference=reference), 1234)
        before = {path.name: path.read_bytes() for path in output.iterdir()}
        with self.assertRaises(FileExistsError):
            fingerprint.fingerprint_engine(engine, output)
        self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()}, before)
        wrong_reference = self.root / "wrong.txt"
        wrong_reference.write_text("4321 nodes\n")
        with self.assertRaisesRegex(ValueError, "fingerprint differs"):
            fingerprint.fingerprint_engine(engine, self.root / "mismatch", reference=wrong_reference)

    def test_changed_fingerprint_is_rejected(self):
        engine = self.engine('''from pathlib import Path
counter = Path(__file__).with_suffix('.count')
count = int(counter.read_text()) if counter.exists() else 0
counter.write_text(str(count + 1))
print(f'{1234 + count} nodes')''')
        output = self.root / "changed"
        with self.assertRaisesRegex(ValueError, "fingerprint differs"):
            fingerprint.fingerprint_engine(engine, output)
        self.assertEqual((output / "fingerprint-2.txt").read_text(), "1235 nodes\n")

    def test_malformed_output_failures_and_timeouts_preserve_diagnostics(self):
        for name, program, error in (
            ("malformed", "print('no fingerprint')", ValueError),
            ("failure", "print('partial', flush=True)\nsys.exit(7)", subprocess.CalledProcessError),
            ("timeout", "print('partial', flush=True)\nimport time\ntime.sleep(10)", subprocess.TimeoutExpired),
        ):
            with self.subTest(name=name):
                output = self.root / name
                with self.assertRaises(error):
                    fingerprint.fingerprint_engine(self.engine(program), output, timeout=0.5)
                self.assertTrue((output / "fingerprint-1.txt").read_text().strip())
                self.assertEqual((output / "fingerprint-1.stderr.txt").read_text(), "diagnostic\n")


if __name__ == "__main__":
    unittest.main()
