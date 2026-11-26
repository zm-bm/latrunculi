"""Check the Makefile contract used by OpenBench workers without compiling."""

import os
import subprocess
import unittest

from tools.paths import ROOT


class WorkerBuildTests(unittest.TestCase):
    def test_tuning_selection(self):
        env = os.environ.copy()
        for name in ('TUNING', 'MAKEFLAGS', 'MFLAGS'):
            env.pop(name, None)
        for exe, override, mode in (
            ('Latrunculi-12345678', None, 'OFF'),
            ('/tmp/Latrunculi-Tuning-12345678-ABCDEF01', None, 'ON'),
            ('/tmp/Latrunculi-Tuning-cache/latrunculi', None, 'OFF'),
            ('latrunculi', 'ON', 'ON'),
            ('Latrunculi-Tuning-12345678', 'OFF', 'OFF'),
        ):
            with self.subTest(exe=exe, override=override):
                args = ['make', '--no-print-directory', '-n', '-C', str(ROOT / 'bench'), 'EXE=' + exe]
                if override:
                    args.append('TUNING=' + override)
                command = subprocess.check_output(args, env=env, text=True)
                self.assertIn('-DLATRUNCULI_TUNING=' + mode, command)
