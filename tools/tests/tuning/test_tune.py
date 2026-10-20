import contextlib
import io
import json
import pathlib
import tempfile
import types
import unittest
from unittest import mock

import numpy as np

from tools.tuning import dataset, model, fit as fitting, artifacts, tune
from .fixtures import (
    base_schema,
    fitting_schema,
    mocked_optimizer,
    prepared_fixture,
    verification_fixture,
    write_prepared_dataset,
)


def fit_jobs(optimizer):
    return [(frozenset(call.args[0].groups), call.args[-1]) for call in optimizer.call_args_list]


class PrepareTest(unittest.TestCase):
    def test_preparation_does_not_depend_on_fitting_constraints(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine = root / "engine"
            pgn = root / "games.pgn"
            engine.write_text("exporter")
            pgn.write_text("games")
            output = root / "prepared"
            current = base_schema([("pawn.isolated", 0, 0), ("pawn.backward", 0, 0)])
            args = types.SimpleNamespace(engine=engine, output=output, pgn=[pgn])
            with (
                mock.patch.dict(dataset.PREPARATION_POLICY, minimum_games=1, minimum_groups=1),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(dataset, "atomic_build", side_effect=lambda binary, paths, target:
                                  write_prepared_dataset(target, current, binary, paths)),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.prepare_command(args)
                tune.prepare_command(args)
            with self.assertRaisesRegex(ValueError, "invalid anchor"):
                fitting.validate_constraints(current, fitting.FIT_POLICY)

    def test_prepare_reuses_matching_dataset_and_rejects_changed_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine = root / "engine"
            pgn = root / "games.pgn"
            engine.write_text("exporter")
            pgn.write_text("games")
            output = root / "prepared"
            current = fitting_schema()
            args = types.SimpleNamespace(engine=engine, output=output, pgn=[pgn])
            with (
                mock.patch.dict(dataset.PREPARATION_POLICY, minimum_games=1, minimum_groups=1),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(
                    dataset, "atomic_build",
                    side_effect=lambda binary, paths, target: write_prepared_dataset(target, current, binary, paths),
                ) as build,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.prepare_command(args)
                tune.prepare_command(args)
                build.assert_called_once()
                pgn.write_text("different games")
                with self.assertRaisesRegex(ValueError, "changed"):
                    tune.prepare_command(args)

    def test_prepare_rejects_an_unsupported_exporter_schema_before_building(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine = root / "engine"
            pgn = root / "games.pgn"
            engine.write_text("exporter")
            pgn.write_text("games")
            current = fitting_schema()
            current["version"] = 99
            with (
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(dataset, "atomic_build") as build,
                self.assertRaisesRegex(ValueError, "schema"),
            ):
                tune.prepare_command(types.SimpleNamespace(engine=engine, output=root / "prepared", pgn=[pgn]))
            build.assert_not_called()


class FitTest(unittest.TestCase):
    def test_fit_reuses_all_checkpoints_and_preserves_fold_constraints_for_refit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            with mocked_optimizer() as (optimizer, calibrate):
                split = model.load_dataset(prepared).development
                policy = fitting.FIT_POLICY
                assignments = fitting.fold_assignments(split, policy["validation"])
                training_groups = [frozenset(split.groups[assignments != fold])
                                   for fold in range(policy["validation"]["folds"])]
                tune.fit_command(args)
                candidate = artifacts.read_artifact(output / "candidate.json")
                self.assertTrue(candidate["cross_validation_supported"])
                self.assertCountEqual(fit_jobs(optimizer), [
                    (groups, penalty) for groups in training_groups
                    for penalty in policy["fit"]["regularization"]
                ] + [(frozenset(split.groups), candidate["selected_regularization"])])
                self.assertCountEqual(
                    [frozenset(call.args[0].groups) for call in calibrate.call_args_list],
                    training_groups + [frozenset(split.groups)],
                )
                parameters = optimizer.call_args_list[0].args[2]
                for call in optimizer.call_args_list:
                    actual = call.args[2]
                    self.assertEqual((actual.names, actual.bounds, actual.frozen),
                                     (parameters.names, parameters.bounds, parameters.frozen))
                self.assertIn("pawn.backward.mg", parameters.frozen)
                self.assertIn("pawn.backward.eg", parameters.frozen)
                candidate_bytes = (output / "candidate.json").read_bytes()
                optimizer.reset_mock()
                calibrate.reset_mock()
                tune.fit_command(args)
            optimizer.assert_not_called()
            calibrate.assert_not_called()
            self.assertEqual(candidate_bytes, (output / "candidate.json").read_bytes())

    def test_interrupted_fit_resumes_after_last_complete_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            penalties = fitting.FIT_POLICY["fit"]["regularization"]
            with mocked_optimizer(interrupt_at=2) as (first, first_calibration):
                compute = first.side_effect

                def interrupt(*args):
                    try:
                        return compute(*args)
                    except RuntimeError:
                        raise KeyboardInterrupt from None

                first.side_effect = interrupt
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
            with mocked_optimizer() as (resumed, calibrate):
                tune.fit_command(args)
            self.assertEqual(completed.read_bytes(), completed_bytes)
            self.assertFalse(incomplete.exists())
            fresh = root / "fresh-fit"
            with mocked_optimizer() as (fresh_optimizer, fresh_calibration):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=fresh))
            self.assertCountEqual(fit_jobs(first)[:-1] + fit_jobs(resumed), fit_jobs(fresh_optimizer))
            self.assertCountEqual(
                [frozenset(call.args[0].groups) for call in first_calibration.call_args_list + calibrate.call_args_list],
                [frozenset(call.args[0].groups) for call in fresh_calibration.call_args_list],
            )
            self.assertEqual((output / "candidate.json").read_bytes(), (fresh / "candidate.json").read_bytes())

    def test_fit_resume_accepts_relocated_dataset_without_originals(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, engine, pgn = prepared_fixture(root)
            output = root / "fit"
            with mocked_optimizer(interrupt_at=2) as (first, _):
                with self.assertRaises(RuntimeError):
                    tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
            engine.unlink()
            pgn.unlink()
            relocated = root / "relocated"
            prepared.rename(relocated)
            with mocked_optimizer() as (optimizer, _):
                tune.fit_command(types.SimpleNamespace(dataset=relocated, output=output))
            self.assertNotIn(fit_jobs(first)[0], fit_jobs(optimizer))
            self.assertTrue(artifacts.read_artifact(output / "candidate.json")["cross_validation_supported"])

    def test_unsupported_fit_retains_baseline_without_full_data_refit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, current, _, _ = prepared_fixture(root)
            output = root / "fit"
            with mocked_optimizer(improve=False) as (optimizer, calibrate):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
                folds = fitting.FIT_POLICY["validation"]["folds"]
                penalties = fitting.FIT_POLICY["fit"]["regularization"]
                self.assertEqual(optimizer.call_count, folds * len(penalties))
                self.assertEqual(calibrate.call_count, folds)
            candidate = artifacts.read_artifact(output / "candidate.json")
            self.assertFalse(candidate["cross_validation_supported"])
            self.assertIsNone(candidate["selected_regularization"])
            self.assertEqual(candidate["changes"], [])
            np.testing.assert_array_equal(artifacts.weights_from_artifact(candidate, current), model.baseline_weights(current))
            self.assertFalse((output / "fit.json").exists())
            self.assertFalse((output / "calibration.json").exists())

    def test_resume_rejects_changed_policy_tools_dependencies_or_prepared_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            with mocked_optimizer():
                tune.fit_command(args)
                for module, target, change in (
                    (fitting, "FIT_POLICY", {**fitting.FIT_POLICY, "optimizer": {**fitting.FIT_POLICY["optimizer"], "maximum_iterations": 99}}),
                    (artifacts, "tool_record", {"tuning/tune.py": "different"}),
                    (artifacts, "dependency_versions", {"python": "different"}),
                ):
                    with self.subTest(target=target):
                        replacement = mock.Mock(return_value=change) if callable(getattr(module, target)) else change
                        with mock.patch.object(module, target, replacement), self.assertRaisesRegex(ValueError, "context mismatch"):
                            tune.fit_command(args)
                manifest = artifacts.load_json(prepared / "manifest.json")
                manifest["identity"]["inputs"][0]["name"] = "renamed-games.pgn"
                artifacts.atomic_write_json(prepared / "manifest.json", manifest)
                with self.assertRaisesRegex(ValueError, "context mismatch"):
                    tune.fit_command(args)

    def test_resume_rejects_corrupt_or_wrong_fold_checkpoint(self):
        for corruption in ("hash", "fold"):
            with self.subTest(corruption=corruption), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                prepared, _, _, _ = prepared_fixture(root)
                output = root / "fit"
                args = types.SimpleNamespace(dataset=prepared, output=output)
                with mocked_optimizer(interrupt_at=2):
                    with self.assertRaises(RuntimeError):
                        tune.fit_command(args)
                path = output / "cross-validation" / "fold-0" / "lambda-1e-09.json"
                artifact = artifacts.read_artifact(path)
                artifact["fold"] = 1
                if corruption == "fold":
                    artifact["artifact_id"] = artifacts.artifact_id(artifact)
                artifacts.atomic_write_json(path, artifact)
                with mocked_optimizer(), self.assertRaisesRegex(ValueError, "hash mismatch|context mismatch"):
                    tune.fit_command(args)

    def test_fit_detects_prepared_data_changed_during_computation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            with mocked_optimizer() as (optimizer, _):
                compute = optimizer.side_effect

                def change_data(*args):
                    result = compute(*args)
                    if optimizer.call_count == 1:
                        with (prepared / dataset.DATA_FILE).open("a") as stream:
                            stream.write("\n")
                    return result

                optimizer.side_effect = change_data
                with self.assertRaisesRegex(ValueError, "changed during fitting"):
                    tune.fit_command(types.SimpleNamespace(dataset=prepared, output=root / "fit"))

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
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            output, current, candidate = verification_fixture(root)
            first = root / "first-engine"
            second = root / "rebuilt-engine"
            first.write_text("first build")
            second.write_text("equivalent rebuild")
            before = {path.name: path.read_bytes() for path in output.iterdir()}
            reports = []
            for engine in (first, second):
                stream = io.StringIO()
                with (
                    mock.patch.object(tune, "read_engine_schema", return_value=current),
                    contextlib.redirect_stdout(stream),
                ):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=engine))
                reports.append(json.loads(stream.getvalue()))
            self.assertEqual(reports[0]["candidate_id"], candidate["artifact_id"])
            self.assertEqual(reports[0]["schema_sha256"], reports[1]["schema_sha256"])
            self.assertNotEqual(reports[0]["engine_sha256"], reports[1]["engine_sha256"])
            self.assertEqual(before, {path.name: path.read_bytes() for path in output.iterdir()})

    def test_verify_rejects_weight_or_evaluation_invariant_mismatch(self):
        for field in ("weight", "tempo"):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                output, current, _ = verification_fixture(root)
                engine = root / "engine"
                engine.write_text("build")
                if field == "weight":
                    current["features"][5]["eg"] += 1
                else:
                    current["tempo"] += 1
                with (
                    mock.patch.object(tune, "read_engine_schema", return_value=current),
                    self.assertRaisesRegex(ValueError, "does not match"),
                ):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=engine))

    def test_verify_rejects_unsupported_proposal_and_wrong_run_identity(self):
        for failure in ("unsupported", "run"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                output, _, candidate = verification_fixture(root, supported=failure != "unsupported")
                if failure == "run":
                    candidate["run_id"] = "another-run"
                    candidate["artifact_id"] = artifacts.artifact_id(candidate)
                    artifacts.atomic_write_json(output / "candidate.json", candidate)
                with self.assertRaisesRegex(ValueError, "cross-validation|context mismatch"):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=root / "engine"))

    def test_verify_rejects_noninteger_proposal_weights(self):
        for value in (10.5, True):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                output, current, candidate = verification_fixture(root)
                candidate["weights"][5]["candidate"]["mg"] = value
                candidate["artifact_id"] = artifacts.artifact_id(candidate)
                artifacts.atomic_write_json(output / "candidate.json", candidate)
                engine = root / "engine"
                engine.write_text("build")
                with (
                    mock.patch.object(tune, "read_engine_schema", return_value=current),
                    self.assertRaisesRegex(ValueError, "integers"),
                ):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=engine))


if __name__ == "__main__":
    unittest.main()
