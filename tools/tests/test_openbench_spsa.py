import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from openbench_client import cli
from tools import openbench, openbench_spsa as spsa

CONFIG = {"OPENBENCH_SERVER": "https://bench.example", "OPENBENCH_USERNAME": "user", "OPENBENCH_PASSWORD": "secret"}


def settings():
    result = json.loads((openbench.TEMPLATES / "spsa.json").read_text())
    result.update(dev_branch="1" * 40, dev_bench=7077765, info="SPSA search",
                  spsa_iterations=12500, spsa_inputs="SeeKnight, int, 350, 250, 375, 6.25, 0.002\nLmrPvScale, float, 0.7, 0.35, 1.05, 0.035, 0.002")
    return spsa.normalize(result)


def response(values=None):
    s = values or settings()
    info = {k: v for k, v in s.items() if not k.startswith("spsa_") and k not in ("dev_branch", "dev_bench")}
    for key in cli.SIDE_FIELDS:
        if key not in ("branch", "bench"):
            info[f"base_{key}"] = s[f"dev_{key}"]
    info.update(id=99, dev={"sha": s["dev_branch"], "bench": int(s["dev_bench"])},
                base={"sha": s["dev_branch"], "bench": int(s["dev_bench"])},
                workload_size=4, max_games=0, approved=True, finished=False, deleted=False,
                error=False, use_penta=True, use_tri=False)
    return {"info": info}


def schedule_page(values=None):
    s = values or settings()
    labels = ("Alpha", "Gamma", "A-Ratio", "Reporting", "Distribution", "Pairs Per Point")
    keys = ("spsa_alpha", "spsa_gamma", "spsa_A_ratio", "spsa_reporting_type", "spsa_distribution_type", "spsa_pairs_per")
    return cli.Page("<table>" + "".join(f"<tr><td>{label}</td><td>{s[key]}</td></tr>" for label, key in zip(labels, keys))
                    + f"</table><div>0/{s['spsa_iterations']} Iterations</div>")


class SettingsTests(unittest.TestCase):
    def test_parameter_api_uses_canonical_authenticated_route(self):
        with patch.object(cli, "api_request", return_value=io.BytesIO(b"SeeKnight, 350")) as request:
            self.assertEqual(spsa.api_text(CONFIG, 99, "outputs"), "SeeKnight, 350")
            request.assert_called_once_with(CONFIG, "/api/spsa/99/outputs/")
        with patch.object(cli, "api_request", return_value=io.BytesIO(b'{"error": "no access"}')):
            with self.assertRaises(cli.OpenBenchError):
                spsa.api_text(CONFIG, 99, "outputs")

    def test_schedule_budget_and_canonical_parameters(self):
        s = settings()
        self.assertEqual(2 * int(s["spsa_iterations"]) * int(s["spsa_pairs_per"]), 100000)
        self.assertEqual(s["priority"], "-1")
        self.assertEqual(s["dev_time_control"], "10.0+0.10")
        self.assertEqual(spsa.normalize(s), s)
        self.assertEqual(spsa.parameters(s["spsa_inputs"])[0][2:], (350, 250, 375, 6.25, .002))

    def test_malformed_inputs_rejected(self):
        for inputs in ("", "bad", "X, int, nan, 0, 2, 1, .002", "X, int, 1.5, 0, 2, 1, .002",
                       "X, float, 1, 0, 2, 0, .002", "X, int, 1, 0, 2, 1, .002\nx, int, 1, 0, 2, 1, .002"):
            with self.subTest(inputs=inputs), self.assertRaises(cli.OpenBenchError):
                spsa.parameters(inputs)
        for update in ({"spsa_gamma": "nan"}, {"spsa_iterations": 0}, {"spsa_pairs_per": "4.5"},
                       {"spsa_distribution_type": "guess"}, {"surprise": 1},
                       {"dev_options": "Threads=1 Hash=32 SeeKnight=350"}):
            with self.subTest(update=update), self.assertRaises(cli.OpenBenchError):
                spsa.normalize(dict(settings(), **update))

    def test_readback_checks_engine_pair_and_schedule(self):
        session = Mock()
        session.fetch.return_value = schedule_page()
        with patch.object(spsa, "api_text", return_value=settings()["spsa_inputs"]):
            self.assertEqual(spsa.readback(CONFIG, session, 99, response()), settings())
            bad = response()
            bad["info"]["base"]["sha"] = "2" * 40
            with self.assertRaises(cli.OpenBenchError):
                spsa.readback(CONFIG, session, 99, bad)
        with self.assertRaises(cli.OpenBenchError):
            spsa.schedule_from_page(cli.Page("<html></html>"))

    def test_duplicate_detection_ignores_description_and_priority(self):
        session = Mock(base=CONFIG["OPENBENCH_SERVER"])
        listing = {"workloads": [{"id": 99, "type": "tune", "state": "active"}]}
        with patch.object(cli, "Session", return_value=session), \
                patch.object(cli, "server_config", return_value={"engines": ["Latrunculi"], "books": {settings()["book_name"]: {}}}), \
                patch.object(cli, "index_workloads", return_value=listing), \
                patch.object(cli, "workload_status", return_value=response()), \
                patch.object(spsa, "readback", return_value=settings()):
            with self.assertRaisesRegex(cli.OpenBenchError, "Equivalent"):
                spsa.preflight(CONFIG, dict(settings(), info="different", priority="0"))


class SubmissionTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.output = Path(temp.name) / "submission"
        self.session = Mock(base=CONFIG["OPENBENCH_SERVER"])
        self.sent = []
        self.lose_response = False
        self.after = {"workloads": [{"id": 99, "type": "tune", "state": "active"}]}
        self.enterContext(patch.object(spsa, "preflight", return_value=(self.session, {"settings": settings(), "index": {"workloads": []}})))
        self.enterContext(patch.object(cli, "index_workloads", side_effect=lambda *a: self.after))
        self.enterContext(patch.object(cli, "workload_status", return_value=response()))
        self.readback = self.enterContext(patch.object(spsa, "readback", return_value=settings()))
        def fetch(path, data=None, redirects=()):
            if data is not None:
                self.assertEqual(path, "/tune/new/")
                self.assertTrue(json.loads((self.output / "submission.json").read_text())["attempted"])
                self.assertEqual(data["csrfmiddlewaretoken"], "csrf-secret")
                self.assertEqual({k: v for k, v in data.items() if k != "csrfmiddlewaretoken"}, settings())
                self.sent.append(data)
                if self.lose_response:
                    raise cli.OpenBenchError("lost response")
            return cli.Page('<input name="csrfmiddlewaretoken" value="csrf-secret">')
        self.session.fetch.side_effect = fetch

    def test_submits_once_and_verifies_saved_readback(self):
        result = spsa.submit(CONFIG, settings(), self.output)
        self.assertTrue(result["verified"])
        self.assertEqual(result["url"], CONFIG["OPENBENCH_SERVER"] + "/tune/99/")
        self.assertEqual(result["state"], "approved")
        self.assertEqual(len(self.sent), 1)
        self.assertNotIn("csrf-secret", (self.output / "submission.json").read_text())
        with self.assertRaises(cli.OpenBenchError):
            spsa.submit(CONFIG, settings(), self.output)
        self.assertEqual(len(self.sent), 1)

    def test_lost_response_recovers_by_readback_without_resubmitting(self):
        self.lose_response = True
        result = spsa.submit(CONFIG, settings(), self.output)
        self.assertTrue(result["verified"])
        self.assertIn("action_error", result)
        self.assertEqual(len(self.sent), 1)

    def test_ambiguous_result_keeps_record_and_does_not_retry(self):
        self.after = {"workloads": []}
        with self.assertRaisesRegex(cli.OpenBenchError, "ambiguous"):
            spsa.submit(CONFIG, settings(), self.output)
        record = json.loads((self.output / "submission.json").read_text())
        self.assertTrue(record["attempted"])
        self.assertFalse(record["verified"])
        self.assertEqual(len(self.sent), 1)

    def test_mismatched_readback_does_not_verify(self):
        self.readback.return_value = dict(settings(), priority="0")
        with self.assertRaises(cli.OpenBenchError):
            spsa.submit(CONFIG, settings(), self.output)
        self.assertFalse(json.loads((self.output / "submission.json").read_text())["verified"])

    def test_dry_run_does_not_create_or_post(self):
        self.assertTrue(spsa.submit(CONFIG, settings(), dry_run=True)["dry_run"])
        self.assertEqual(self.sent, [])
        self.assertFalse(self.output.exists())

    def test_stop_uses_tune_route_and_recovers_after_lost_response(self):
        before = response()
        after = response()
        after["info"]["finished"] = True
        with patch.object(cli, "Session", return_value=self.session), \
                patch.object(cli, "workload_status", side_effect=[before, after]):
            self.session.fetch.side_effect = cli.OpenBenchError("lost response")
            result = spsa.stop(CONFIG, 99, self.output)
        self.assertTrue(result["verified"])
        self.session.fetch.assert_called_once_with("/tune/99/STOP/", redirects=("/index/", "/login/"))


class PreparationTests(unittest.TestCase):
    def test_prepare_exports_verifies_and_records_binary_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "engine"
            binary.write_text("binary")
            binary.chmod(0o700)
            output = Path(temp) / "prepared"
            args = openbench.parse_prepare_args(["spsa", "--candidate", "HEAD", "--candidate-engine", str(binary),
                                                "--games", "100000", "--description", "SPSA search", "--output", str(output)])
            export = Mock(stdout=settings()["spsa_inputs"])
            uci = Mock(stdout="option name SeeKnight type spin default 350 min 250 max 375\n"
                             "option name LmrPvScale type string default 0.7\nuciok\n")
            with patch.object(openbench, "revision", return_value="1" * 40), \
                    patch.object(spsa.subprocess, "run", side_effect=[export, uci]), \
                    patch.object(spsa.engine, "bench_nodes", return_value=7077765), \
                    contextlib.redirect_stdout(io.StringIO()):
                path = openbench.prepare(args)
            self.assertEqual(json.loads(path.read_text()), settings())
            self.assertEqual(len(json.loads((output / "candidate.json").read_text())["sha256"]), 64)
            self.assertEqual(len(spsa.parameters((output / "parameters.txt").read_text())), 2)

    def test_wrong_uci_type_rejected_before_bench(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "engine"
            binary.touch()
            binary.chmod(0o700)
            output = Path(temp) / "prepared"
            args = openbench.parse_prepare_args(["spsa", "--candidate", "HEAD", "--candidate-engine", str(binary),
                                                "--games", "100000", "--description", "SPSA", "--output", str(output)])
            with patch.object(openbench, "revision", return_value="1" * 40), \
                    patch.object(spsa.subprocess, "run", side_effect=[Mock(stdout=settings()["spsa_inputs"]), Mock(stdout="uciok\n")]), \
                    patch.object(spsa.engine, "bench_nodes") as bench:
                with self.assertRaisesRegex(ValueError, "disagrees"):
                    openbench.prepare(args)
                bench.assert_not_called()
                self.assertFalse(output.exists())
