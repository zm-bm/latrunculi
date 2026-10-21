import contextlib
import io
import json
import pathlib
import tempfile
import types
import unittest
from unittest import mock

import numpy as np

from tools import paths
from tools.tuning import dataset, model, fit as fitting, artifacts, tune
from .fixtures import (
    fitting_schema,
    mocked_optimizer,
    prepared_fixture,
    verification_fixture,
    write_prepared_dataset,
)


def fit_jobs(optimizer):
    return [(frozenset(call.args[0].groups), call.args[-1]) for call in optimizer.call_args_list]


class PrepareTest(unittest.TestCase):
    def test_prepare_reuses_matching_dataset_and_rejects_changed_inputs(self):
        with tempfile.TemporaryDirectory(prefix="tune selection ") as directory:
            root = pathlib.Path(directory)
            checkout = root / "checkout"
            engine = checkout / "build/release-dev/latrunculi"
            engine.parent.mkdir(parents=True)
            pgn = root / "games.pgn"
            engine.write_text("exporter")
            (root / "latrunculi").write_bytes(engine.read_bytes())
            pgn.write_text("games")
            output = checkout / "tools/tuning/output/pawns-data"
            current = fitting_schema()
            arguments = ["prepare", "--engine", "release-dev", "--name", "pawns-data", pgn.name]
            with (
                mock.patch.object(paths, "ROOT", checkout),
                contextlib.chdir(root),
                mock.patch.dict(dataset.PREPARATION_POLICY, minimum_games=1, minimum_groups=1),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(
                    dataset, "atomic_build",
                    side_effect=lambda binary, paths, target: write_prepared_dataset(target, current, binary, paths),
                ) as build,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(tune.main(arguments), 0)
                self.assertEqual(tune.main([
                    "prepare", "--engine", "./latrunculi", "--output", str(output), pgn.name,
                ]), 0)
                build.assert_called_once_with(engine, [pgn], output)
                pgn.write_text("different games")
                with self.assertRaisesRegex(ValueError, "changed"):
                    tune.prepare_command(tune.parse_args(arguments))


class FitTest(unittest.TestCase):
    def test_completed_fit_is_reused_without_optimization(self):
        with tempfile.TemporaryDirectory(prefix="tune selection ") as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "tools/tuning/output/pawns-fit"
            output.parent.mkdir(parents=True)
            prepared = prepared.rename(output.parent / "pawns-data")
            with (
                mock.patch.object(paths, "ROOT", root),
                contextlib.chdir(output.parent),
                mocked_optimizer() as (optimizer, calibrate),
            ):
                self.assertEqual(tune.main(["fit", "--dataset", "pawns-data", "--name", "pawns-fit"]), 0)
                candidate = artifacts.read_artifact(output / "candidate.json")
                self.assertTrue(candidate["cross_validation_supported"])
                candidate_bytes = (output / "candidate.json").read_bytes()
                optimizer.reset_mock()
                calibrate.reset_mock()
                self.assertEqual(tune.main([
                    "fit", "--dataset", "./pawns-data", "--output", "pawns-fit",
                ]), 0)
            optimizer.assert_not_called()
            calibrate.assert_not_called()
            self.assertEqual(candidate_bytes, (output / "candidate.json").read_bytes())

    def test_interrupted_fit_resumes_with_relocated_data_and_matches_fresh_fit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, engine, pgn = prepared_fixture(root)
            output = root / "fit"
            penalties = fitting.FIT_POLICY["fit"]["regularization"]
            with mocked_optimizer(interrupt_at=2) as (first, _):
                with contextlib.redirect_stderr(io.StringIO()) as stderr:
                    self.assertEqual(tune.main([
                        "fit", "--dataset", str(prepared), "--output", str(output),
                    ]), 130)
                self.assertEqual(stderr.getvalue(), "tune: interrupted\n")
            fold = output / "cross-validation" / "fold-0"
            completed = fold / artifacts.fit_filename(penalties[0])
            completed_bytes = completed.read_bytes()
            incomplete = fold / (artifacts.fit_filename(penalties[1]) + ".partial")
            incomplete.write_text("interrupted write")
            engine.unlink()
            pgn.unlink()
            prepared = prepared.rename(root / "relocated")
            with mocked_optimizer() as (resumed, _):
                self.assertEqual(tune.main([
                    "fit", "--dataset", str(prepared), "--output", str(output),
                ]), 0)
            self.assertEqual(completed.read_bytes(), completed_bytes)
            self.assertFalse(incomplete.exists())
            fresh = root / "fresh-fit"
            with mocked_optimizer() as (fresh_optimizer, _):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=fresh))
            self.assertCountEqual(fit_jobs(first)[:-1] + fit_jobs(resumed), fit_jobs(fresh_optimizer))
            self.assertEqual((output / "candidate.json").read_bytes(), (fresh / "candidate.json").read_bytes())

    def test_unsupported_fit_retains_baseline_without_full_data_refit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, current, _, _ = prepared_fixture(root)
            output = root / "fit"
            with mocked_optimizer(improve=False):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
            candidate = artifacts.read_artifact(output / "candidate.json")
            self.assertFalse(candidate["cross_validation_supported"])
            self.assertIsNone(candidate["selected_regularization"])
            self.assertEqual(candidate["changes"], [])
            np.testing.assert_array_equal(artifacts.weights_from_artifact(candidate, current), model.baseline_weights(current))
            self.assertFalse((output / "fit.json").exists())
            self.assertFalse((output / "calibration.json").exists())
            with self.assertRaisesRegex(ValueError, "cross-validation"):
                tune.verify_command(types.SimpleNamespace(output=output, engine=root / "engine"))

    def test_resume_rejects_changed_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            with mocked_optimizer():
                tune.fit_command(args)
                with (
                    mock.patch.object(artifacts, "tool_record", return_value={"tuning/tune.py": "different"}),
                    self.assertRaisesRegex(ValueError, "context mismatch"),
                ):
                    tune.fit_command(args)

    def test_fit_preserves_unrelated_nonempty_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "existing-output"
            output.mkdir()
            marker = output / "keep.txt"
            marker.write_text("unrelated work")
            with self.assertRaisesRegex(ValueError, "prepare and fit into new output directories"):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
            self.assertEqual(list(output.iterdir()), [marker])
            self.assertEqual(marker.read_text(), "unrelated work")


class VerificationTest(unittest.TestCase):
    def test_verify_is_repeatable_for_equivalent_binaries_without_dataset_or_mutation(self):
        with tempfile.TemporaryDirectory(prefix="tune selection ") as directory:
            root = pathlib.Path(directory)
            output, current, candidate = verification_fixture(root / "tools/tuning/output")
            first = root / "build/release-dev/latrunculi"
            first.parent.mkdir(parents=True)
            second = root / "rebuilt-engine"
            first.write_text("first build")
            second.write_text("equivalent rebuild")
            before = {path.name: path.read_bytes() for path in output.iterdir()}
            reports = []
            for selection, engine in (("fit", "release-dev"), ("./fit", str(second))):
                stream = io.StringIO()
                with (
                    mock.patch.object(paths, "ROOT", root),
                    contextlib.chdir(output.parent),
                    mock.patch.object(tune, "read_engine_schema", return_value=current),
                    contextlib.redirect_stdout(stream),
                ):
                    self.assertEqual(tune.main(["verify", selection, "--engine", engine]), 0)
                reports.append(json.loads(stream.getvalue()))
            self.assertEqual(reports[0]["candidate_id"], candidate["artifact_id"])
            self.assertEqual(reports[0]["schema_sha256"], reports[1]["schema_sha256"])
            self.assertNotEqual(reports[0]["engine_sha256"], reports[1]["engine_sha256"])
            self.assertEqual(before, {path.name: path.read_bytes() for path in output.iterdir()})

    def test_verify_rejects_compiled_weight_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            output, current, _ = verification_fixture(root)
            engine = root / "engine"
            engine.write_text("build")
            current["features"][5]["eg"] += 1
            with (
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                self.assertRaisesRegex(ValueError, "does not match"),
            ):
                tune.verify_command(types.SimpleNamespace(output=output, engine=engine))


if __name__ == "__main__":
    unittest.main()
