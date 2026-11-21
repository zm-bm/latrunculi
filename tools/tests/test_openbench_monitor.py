import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import openbench_monitor as monitor


def settings():
    return {'spsa_iterations': '10000', 'spsa_pairs_per': '4', 'dev_branch': 'a' * 40}


def snapshot(games, *, finished=False, error=False, deleted=False):
    return {'settings': settings(), 'target_games': 80000,
            'info': dict(id=106, games=games, finished=finished, error=error, deleted=deleted)}


class SpsaMonitorTests(unittest.TestCase):
    def test_terminal_states_distinguish_budget_from_premature_stop(self):
        cases = [
            (snapshot(15936), 'observing'),
            (snapshot(15936, finished=True), 'premature-stop'),
            (snapshot(80000, finished=True), 'completed'),
            (snapshot(80008, finished=True), 'completed'),
            (snapshot(80000), 'budget-stop'),
            (snapshot(15936, error=True), 'correctness-stop'),
            (snapshot(15936, finished=True, error=True), 'correctness-stop'),
            (snapshot(15936, deleted=True), 'deleted'),
        ]
        for value, expected in cases:
            with self.subTest(expected=expected):
                self.assertEqual(monitor.classify(value, 106, settings()), expected)

    def test_mismatched_identity_or_malformed_progress_refuses_to_act(self):
        for key, value in [('id', 107), ('games', -1), ('games', True), ('finished', 1)]:
            bad = snapshot(15936); bad['info'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                monitor.classify(bad, 106, settings())
        for key, value in [('settings', {**settings(), 'dev_branch': 'b' * 40}), ('target_games', 16000)]:
            bad = snapshot(15936); bad[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                monitor.classify(bad, 106, settings())

    def test_actual_106_failure_is_a_nonzero_exit_and_never_restarted(self):
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(monitor.spsa, 'inspect', return_value=snapshot(15936, finished=True)), \
                patch.object(monitor.spsa, 'stop') as stop:
            out = Path(temp) / 'evidence'
            self.assertEqual(monitor.watch({}, 106, settings(), out, 60), 2)
            self.assertEqual(json.loads((out / 'state.json').read_text())['status'], 'premature-stop')
            stop.assert_not_called()

    def test_correctness_failure_stops_only_verified_active_workload(self):
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(monitor.spsa, 'inspect', return_value=snapshot(200, error=True)), \
                patch.object(monitor.spsa, 'stop') as stop:
            out = Path(temp) / 'evidence'
            self.assertEqual(monitor.watch({}, 106, settings(), out, 60), 2)
            stop.assert_called_once_with({}, 106, out / 'stop')

    def test_changed_settings_are_recorded_and_prevent_a_stop(self):
        bad = snapshot(200, error=True); bad['settings']['dev_branch'] = 'b' * 40
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(monitor.spsa, 'inspect', return_value=bad), \
                patch.object(monitor.spsa, 'stop') as stop:
            out = Path(temp) / 'evidence'
            self.assertEqual(monitor.watch({}, 106, settings(), out, 60), 2)
            self.assertEqual(json.loads((out / 'state.json').read_text())['status'], 'identity-error')
            stop.assert_not_called()

    def test_failed_stop_is_retained_without_an_automatic_retry(self):
        with tempfile.TemporaryDirectory() as temp, \
                patch.object(monitor.spsa, 'inspect', return_value=snapshot(200, error=True)), \
                patch.object(monitor.spsa, 'stop', side_effect=OSError('connection lost')) as stop:
            out = Path(temp) / 'evidence'
            self.assertEqual(monitor.watch({}, 106, settings(), out, 60), 2)
            self.assertEqual(json.loads((out / 'state.json').read_text())['status'], 'stop-error')
            stop.assert_called_once()
