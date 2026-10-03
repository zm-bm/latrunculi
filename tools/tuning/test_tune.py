import copy
import contextlib
import io
import json
import pathlib
import tempfile
import types
import unittest
from unittest import mock

import numpy as np
from scipy import sparse

import tune


def schema(features):
    return {
        "type": "schema",
        "version": 2,
        "perspective": {
            "coefficients": "white",
            "fixed": "white",
            "eval": "side_to_move",
        },
        "result": "1=white_win,0=draw,-1=black_win",
        "phase_counts": ["knight", "bishop", "rook", "queen"],
        "pawn_counts": ["white", "black"],
        "phase_limit": 128,
        "phase_material_min": 0,
        "phase_material_max": 13000,
        "scale_limit": 64,
        "scale_base": 48,
        "scale_per_pawn": 4,
        "tempo": 20,
        "features": [
            {"id": feature_id, "name": name, "mg": mg, "eg": eg}
            for feature_id, (name, mg, eg) in enumerate(features)
        ],
    }


def base_schema(extra=()):
    return schema(
        [
            ("material.pawn", 100, 166),
            ("material.knight", 600, 680),
            ("material.bishop", 650, 740),
            ("material.rook", 1000, 1100),
            ("material.queen", 2000, 2150),
            *extra,
        ]
    )


def record(
    coefficients,
    *,
    source="group:game:1",
    result=0,
    fixed=(0, 0),
    phase_counts=(2, 2, 2, 1),
    pawns=(4, 4),
    turn="w",
):
    return {
        "type": "position",
        "version": 2,
        "source": source,
        "result": result,
        "fen": "",
        "turn": turn,
        "phase_counts": list(phase_counts),
        "pawn_counts": list(pawns),
        "fixed": list(fixed),
        "coefficients": list(coefficients),
        "eval": 0,
    }


def make_split(records, feature_count):
    rows = []
    columns = []
    values = []
    for row, item in enumerate(records):
        for feature_id, coefficient in item["coefficients"]:
            rows.append(row)
            columns.append(feature_id)
            values.append(coefficient)
    names = np.asarray(
        [tune.dataset.group_from_source(item["source"]) for item in records], dtype=object
    )
    group_names, groups = np.unique(names, return_inverse=True)
    groups = groups.astype(np.int32)
    return tune.Split(
        coefficients=sparse.csr_matrix(
            (values, (rows, columns)),
            shape=(len(records), feature_count),
            dtype=np.int32,
        ),
        fixed=np.asarray([item["fixed"] for item in records], dtype=np.int32).reshape((-1, 2)),
        phase_counts=np.asarray(
            [item["phase_counts"] for item in records], dtype=np.int16
        ).reshape((-1, 4)),
        pawn_counts=np.asarray(
            [item["pawn_counts"] for item in records], dtype=np.int8
        ).reshape((-1, 2)),
        turns=np.asarray(
            [1 if item["turn"] == "w" else -1 for item in records], dtype=np.int8
        ),
        targets=np.asarray([(item["result"] + 1) / 2 for item in records]),
        exported=np.asarray([item["eval"] for item in records], dtype=np.int32),
        groups=groups,
        group_names=group_names,
        weights=tune.group_weights(groups),
    )


def experiment(*, anchors=(), fixed=("material.pawn.mg",), mirrored=(), support=1):
    return {
        "support": {"minimum_groups": support},
        "constraints": {
            "fixed": list(fixed),
            "anchors": list(anchors),
            "mirror_files": list(mirrored),
        },
        "fit": {
            "delta_bounds": [-100, 100],
            "bounds": [],
            "regularization": [1e-9],
        },
        "optimizer": {
            "method": "L-BFGS-B",
            "maximum_iterations": 200,
            "gradient_tolerance": 1e-12,
            "function_tolerance": 1e-15,
            "maximum_line_search_steps": 50,
        },
        "validation": {"folds": 5},
    }


class ObjectiveTest(unittest.TestCase):
    def test_group_weights_give_every_group_equal_total_weight(self):
        groups = np.asarray(["a", "a", "a", "b"], dtype=object)
        weights = tune.group_weights(groups)
        np.testing.assert_allclose(weights, [1 / 3, 1 / 3, 1 / 3, 1])
        self.assertAlmostEqual(weights[:3].sum(), weights[3:].sum())

    def test_white_perspective_flips_the_complete_production_evaluation(self):
        records = [record([], source="a:x:1", turn="w"), record([], source="b:x:1", turn="b")]
        records[0]["eval"] = 30
        records[1]["eval"] = 30
        split = make_split(records, 0)

        np.testing.assert_array_equal(tune.white_exported(split), [30, -30])
        self.assertAlmostEqual(
            float(tune.expected_score(30, 0.7)),
            1 - float(tune.expected_score(-30, 0.7)),
        )

    def test_calibration_recovers_a_known_scale_with_group_weights(self):
        evaluations = np.arange(-800, 801, 100, dtype=np.float64)
        records = [record([], source=f"g{i}:x:1") for i in range(len(evaluations))]
        split = make_split(records, 0)
        split.exported = evaluations
        split.targets = tune.expected_score(evaluations, 0.75)

        result = tune.calibrate_scale(
            split,
            {"bounds": [0.0, 2.0], "absolute_tolerance": 1e-12, "maximum_iterations": 256},
        )
        self.assertAlmostEqual(result.x, 0.75, places=7)

    def test_calibration_rejects_an_optimum_at_the_search_boundary(self):
        split = make_split([record([])], 0)
        result = types.SimpleNamespace(x=4.0, fun=0.1, success=True)
        with (
            mock.patch.object(tune.optimize, "minimize_scalar", return_value=result),
            self.assertRaisesRegex(ValueError, "search bound"),
        ):
            tune.calibrate_scale(
                split,
                {
                    "bounds": [0.0, 4.0],
                    "absolute_tolerance": 1e-12,
                    "maximum_iterations": 256,
                },
            )


class ModelTest(unittest.TestCase):
    def test_candidate_schema_recalculates_phase_maximum(self):
        current = base_schema()
        weights = tune.baseline_weights(current).astype(np.int64)
        weights[1, 0] = 700
        candidate = tune.schema_with_weights(current, weights)
        self.assertEqual(candidate["phase_material_max"], 4 * 700 + 4 * 650 + 4 * 1000 + 2 * 2000)

    def test_exact_evaluation_uses_cpp_negative_division_and_turn_tempo(self):
        current = base_schema()
        position = record(
            [[0, -1]],
            fixed=(0, -1),
            phase_counts=(0, 0, 0, 0),
            pawns=(0, 0),
            turn="b",
        )
        split = make_split([position], len(current["features"]))
        values = tune.exact_evaluations(split, current, tune.baseline_weights(current).astype(int))
        self.assertEqual(values.tolist(), [-145.0])

    def test_vectorized_exact_evaluation_matches_scalar_reconstruction(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record(
                [[0, pawn_coefficient], [5, 2]],
                fixed=fixed,
                phase_counts=counts,
                pawns=pawns,
                turn=turn,
            )
            for counts, pawns, turn, pawn_coefficient, fixed in (
                ((0, 0, 0, 0), (8, 0), "w", -1, (-3, -7)),
                ((1, 1, 2, 0), (4, 8), "b", 1, (3, 7)),
                ((2, 2, 2, 1), (8, 4), "w", -1, (-3, -7)),
                ((4, 4, 3, 1), (8, 1), "b", 1, (3, 7)),
                ((4, 4, 4, 2), (1, 8), "w", -1, (-3, -7)),
                ((8, 8, 8, 4), (8, 1), "b", 1, (3, 7)),
            )
        ]
        split = make_split(records, len(current["features"]))
        parent = tune.baseline_weights(current).astype(int)
        candidate = parent.copy()
        candidate[1, 0] += 37
        for index, weights in enumerate((parent, candidate)):
            actual, phases = tune.exact_evaluations_and_phases(split, current, weights)
            candidate_schema = tune.schema_with_weights(current, weights)
            expected = []
            expected_phases = []
            for item in records:
                value, phase = tune.dataset.reconstruct(candidate_schema, item)
                expected.append(value if item["turn"] == "w" else -value)
                expected_phases.append(phase)
            np.testing.assert_array_equal(actual, expected)
            np.testing.assert_array_equal(phases, expected_phases)
            if index == 0:
                self.assertEqual(phases.tolist(), [0, 32, 64, 98, 128, 128])

    def test_continuous_gradient_matches_finite_differences(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record(
                [[0, 2], [1, 1], [5, -2]],
                source="a:x:1",
                result=1,
                fixed=(13, -7),
                pawns=(3, 6),
            ),
            record(
                [[2, -1], [3, 1], [4, -1], [5, 3]],
                source="b:x:1",
                result=-1,
                fixed=(-11, 9),
                phase_counts=(1, 1, 1, 1),
                pawns=(7, 2),
                turn="b",
            ),
        ]
        split = make_split(records, len(current["features"]))
        weights = tune.baseline_weights(current)
        _, analytic = tune.continuous_loss_gradient(split, current, weights, 0.7)
        numeric = np.empty_like(weights)
        step = 1e-4
        for feature_id in range(len(weights)):
            for phase in range(2):
                upper = weights.copy()
                lower = weights.copy()
                upper[feature_id, phase] += step
                lower[feature_id, phase] -= step
                upper_loss = tune.mean_squared_error(
                    tune.continuous_evaluation(split, current, upper),
                    split.targets,
                    0.7,
                    split.weights,
                )
                lower_loss = tune.mean_squared_error(
                    tune.continuous_evaluation(split, current, lower),
                    split.targets,
                    0.7,
                    split.weights,
                )
                numeric[feature_id, phase] = (upper_loss - lower_loss) / (2 * step)
        np.testing.assert_allclose(analytic, numeric, rtol=2e-5, atol=1e-10)

    def test_development_folds_keep_groups_together(self):
        current = base_schema()
        split = make_split(
            [
                record([], source=f"g{group}:p{position}:1")
                for group in range(50)
                for position in range(2)
            ],
            len(current["features"]),
        )
        policy = {"folds": 5, "fold_seed": 7}
        assignments = tune.fold_assignments(split, policy)
        np.testing.assert_array_equal(
            assignments, tune.fold_assignments(split, policy)
        )

        testing_positions = 0
        for fold in range(policy["folds"]):
            training = tune.subset_split(split, assignments != fold)
            testing = tune.subset_split(split, assignments == fold)
            testing_positions += len(testing.targets)
            self.assertFalse(set(training.groups) & set(testing.groups))
        self.assertEqual(testing_positions, len(split.targets))


class SupportTest(unittest.TestCase):
    def test_support_counts_distinct_groups(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record([[5, 1]], source="a:x:1"),
            record([[5, -1]], source="a:y:2"),
            record([[0, 1]], source="b:x:1"),
        ]
        split = make_split(records, len(current["features"]))
        report = tune.feature_support(split, current, 2)
        self.assertEqual(report[5]["positions"], 2)
        self.assertEqual(report[5]["groups"], 1)
        self.assertEqual(report[5]["status"], "insufficient")


class ParameterMapTest(unittest.TestCase):
    def test_joint_map_selects_every_unfixed_coordinate(self):
        current = base_schema()
        split = make_split(
            [record([[feature_id, 1] for feature_id in range(5)], source="a:x:1")],
            len(current["features"]),
        )

        parameters = tune.build_parameter_map(
            current, tune.baseline_weights(current), split, experiment()
        )

        self.assertEqual(len(parameters.members), 9)
        self.assertEqual(parameters.frozen, {"material.pawn.mg": "fixed"})

    def test_ties_use_union_support_and_penalize_each_coordinate(self):
        current = base_schema(
            [
                ("psqt.knight.a1", -10, -20),
                ("psqt.knight.h1", -10, -20),
                ("psqt.knight.b1", -5, -8),
                ("psqt.knight.g1", -5, -8),
            ]
        )
        records = []
        for index in range(40):
            records.append(
                record(
                    [[7 if index % 2 else 8, 1]],
                    source=f"g{index}:x:1",
                )
            )
        split = make_split(records, len(current["features"]))
        settings = experiment(
            anchors=("psqt.knight.a1",),
            mirrored=("knight",),
            support=32,
        )
        parameters = tune.build_parameter_map(
            current, tune.baseline_weights(current), split, settings
        )

        self.assertEqual(parameters.names, ["psqt.knight.b1.mg", "psqt.knight.b1.eg"])
        self.assertEqual(parameters.support["psqt.knight.b1.mg"], 40)
        np.testing.assert_array_equal(parameters.multiplicity, [2, 2])
        self.assertEqual(parameters.frozen["psqt.knight.a1.mg"], "anchor")

    def test_support_must_reach_the_minimum_in_every_training_fold(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        split = make_split(
            [
                *[
                    record([[1, 1], [5, 1]], source=f"a{index}:x:1")
                    for index in range(3)
                ],
                record([[1, 1], [5, 1]], source="b0:x:1"),
                record([[1, 1]], source="b1:x:1"),
            ],
            len(current["features"]),
        )
        settings = experiment(support=2)
        settings["validation"]["folds"] = 2
        assignments = np.asarray([0, 0, 0, 1, 1], dtype=np.int8)

        parameters = tune.build_parameter_map(
            current,
            tune.baseline_weights(current),
            split,
            settings,
            assignments,
        )

        self.assertEqual(parameters.support["pawn.isolated.mg"], 1)
        self.assertEqual(parameters.frozen["pawn.isolated.mg"], "support")
        self.assertEqual(parameters.frozen["pawn.isolated.eg"], "support")

    def test_bounds_and_rounding_preserve_ties(self):
        current = base_schema(
            [
                ("psqt.knight.a1", -10, -20),
                ("psqt.knight.h1", -10, -20),
                ("psqt.knight.b1", -5, -8),
                ("psqt.knight.g1", -5, -8),
            ]
        )
        records = [record([[7, 1]], source="a:x:1")]
        split = make_split(records, len(current["features"]))
        settings = experiment(
            anchors=("psqt.knight.a1",),
            mirrored=("knight",),
        )
        settings["fit"]["bounds"] = [
            {"pattern": "psqt.knight.b1.mg", "minimum": -6, "maximum": -3}
        ]
        parameters = tune.build_parameter_map(
            current, tune.baseline_weights(current), split, settings
        )
        self.assertEqual(parameters.bounds, [(-1.0, 2.0), (-100.0, 100.0)])
        rounded = parameters.rounded([2.6, -0.5])
        self.assertEqual(rounded[5:9].tolist(), [[-10, -20], [-10, -20], [-3, -9], [-3, -9]])


class OptimizerTest(unittest.TestCase):
    def test_joint_fit_is_deterministic(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record(
                [[5, coefficient]],
                source=f"g{index}:x:1",
                phase_counts=(0, 0, 0, 0),
            )
            for index, coefficient in enumerate(range(-4, 5))
        ]
        train = make_split(records, len(current["features"]))
        target = tune.baseline_weights(current)
        target[5, 1] = 30
        train.targets = tune.expected_score(
            tune.continuous_evaluation(train, current, target), 0.7
        )
        settings = experiment(fixed=("material.pawn.mg", "pawn.isolated.mg"))
        parameters = tune.build_parameter_map(
            current, tune.baseline_weights(current), train, settings
        )

        first = tune.fit_parameters(train, current, parameters, settings, 0.7, 0.0)
        second = tune.fit_parameters(train, current, parameters, settings, 0.7, 0.0)
        np.testing.assert_array_equal(first["deltas"], second["deltas"])
        self.assertAlmostEqual(first["rounded"][5, 1], 30, delta=1)

    def test_checkpoint_selection_uses_exact_rounded_loss(self):
        current = base_schema([("pawn.isolated", 0, 0)])
        split = make_split([record([[5, 1]])], len(current["features"]))
        parent = tune.baseline_weights(current)
        parameters = tune.ParameterMap(
            parent=parent,
            members=[[(5, 1)]],
            names=["pawn.isolated.eg"],
            bounds=[(-100, 100)],
            support={"pawn.isolated.eg": 1},
            frozen={},
            multiplicity=np.asarray([1.0]),
        )

        def minimize(*_, callback, **__):
            callback(np.asarray([1.51]))
            callback(np.asarray([0.49]))
            return types.SimpleNamespace(
                x=np.asarray([0.49]),
                nit=2,
                nfev=2,
                njev=2,
                fun=0.0,
                success=True,
                message="ok",
            )

        def exact_metric(_, __, weights, ___):
            return {"mean_squared_error": 0.1 if weights[5, 1] == 2 else 0.2}

        with (
            mock.patch.object(tune.optimize, "minimize", side_effect=minimize),
            mock.patch.object(tune, "split_metric", side_effect=exact_metric),
            mock.patch.object(tune, "continuous_evaluation", return_value=np.asarray([0.0])),
        ):
            result = tune.fit_parameters(
                split, current, parameters, experiment(), 0.7, 0.0
            )

        self.assertEqual(result["rounded"][5, 1], 2)
        self.assertEqual(result["selected_iteration"], 1)
        self.assertEqual(result["optimizer"].x[0], 0.49)

    def test_checkpoint_selection_penalizes_the_rounded_candidate(self):
        current = base_schema([("pawn.isolated", 0, 0)])
        split = make_split([record([[5, 1]])], len(current["features"]))
        parameters = tune.ParameterMap(
            parent=tune.baseline_weights(current),
            members=[[(5, 1)]],
            names=["pawn.isolated.eg"],
            bounds=[(-100, 100)],
            support={"pawn.isolated.eg": 1},
            frozen={},
            multiplicity=np.asarray([1.0]),
        )

        def minimize(*_, callback, **__):
            callback(np.asarray([1.51]))
            callback(np.asarray([0.51]))
            return types.SimpleNamespace(
                x=np.asarray([1.51]),
                nit=2,
                nfev=2,
                njev=2,
                fun=0.0,
                success=True,
                message="ok",
            )

        def exact_metric(_, __, weights, ___):
            return {
                "mean_squared_error": {0: 0.3, 1: 0.12, 2: 0.1}[weights[5, 1]]
            }

        with (
            mock.patch.object(tune.optimize, "minimize", side_effect=minimize),
            mock.patch.object(tune, "split_metric", side_effect=exact_metric),
            mock.patch.object(tune, "continuous_evaluation", return_value=np.asarray([0.0])),
        ):
            result = tune.fit_parameters(
                split, current, parameters, experiment(), 0.7, 0.02
            )

        self.assertEqual(result["rounded"][5, 1], 1)
        self.assertAlmostEqual(result["exact_objective"], 0.14)


class ValidationTest(unittest.TestCase):
    def test_bootstrap_is_deterministic_and_uses_groups(self):
        values = np.asarray([0.1, 0.2, 0.3])
        first = tune.bootstrap_interval(values, 200, 0.9, 7)
        self.assertEqual(first, tune.bootstrap_interval(values, 200, 0.9, 7))
        self.assertEqual(first["groups"], 3)
        self.assertAlmostEqual(first["mean"], 0.2)

    def test_comparison_reports_all_phase_buckets_and_qualifies_clear_gain(self):
        current = base_schema([("pawn.isolated", 0, 0)])
        records = []
        for index in range(160):
            coefficient = 1 if index % 2 == 0 else -1
            result = 1 if coefficient > 0 else -1
            records.append(
                record(
                    [[5, coefficient]],
                    source=f"g{index}:x:1",
                    result=result,
                    phase_counts=(0, 0, 0, 0),
                    pawns=(0, 0),
                )
            )
        split = make_split(records, len(current["features"]))
        parent = tune.baseline_weights(current).astype(int)
        candidate = parent.copy()
        candidate[5] = [100, 100]
        report = tune.comparison_report(
            [(split, current, parent, candidate, 0.7)],
            {
                "bootstrap_samples": 500,
                "confidence": 0.9,
                "phase_buckets": [0, 32, 64, 96, 129],
                "minimum_phase_groups": 128,
            },
            9,
        )
        self.assertTrue(report["qualified"])
        self.assertLess(
            report["mean_squared_error"]["candidate"],
            report["mean_squared_error"]["baseline"],
        )
        self.assertEqual(
            [item["bucket"] for item in report["phases"]],
            ["0-31", "32-63", "64-95", "96-128"],
        )

    def test_selects_the_strongest_eligible_penalty_within_one_standard_error(self):
        reports = [
            {
                "regularization": 1e-9,
                "mean_improvement": 0.00068,
                "standard_error": 0.00005,
                "eligible": True,
            },
            {
                "regularization": 3e-9,
                "mean_improvement": 0.00065,
                "standard_error": 0.00004,
                "eligible": True,
            },
            {
                "regularization": 1e-8,
                "mean_improvement": 0.00059,
                "standard_error": 0.00003,
                "eligible": True,
            },
            {
                "regularization": 3e-8,
                "mean_improvement": 0.00067,
                "standard_error": 0.00003,
                "eligible": False,
            },
        ]

        best, threshold, selected = tune.select_regularization(reports)

        self.assertEqual(best["regularization"], 1e-9)
        self.assertAlmostEqual(threshold, 0.00063)
        self.assertEqual(selected["regularization"], 3e-9)

    def test_regularization_selection_falls_back_to_the_baseline(self):
        best, threshold, selected = tune.select_regularization(
            [
                {
                    "regularization": 1e-9,
                    "mean_improvement": -0.001,
                    "standard_error": 0.0001,
                    "eligible": False,
                }
            ]
        )

        self.assertIsNone(best)
        self.assertIsNone(threshold)
        self.assertIsNone(selected)

    def test_candidate_reports_support_and_bound_hits(self):
        current = base_schema([("pawn.isolated", 0, 0)])
        development = make_split(
            [record([[5, 1]], source="g:x:1")], len(current["features"])
        )
        parent = tune.baseline_weights(current).astype(int)
        weights = parent.copy()
        weights[5, 1] = 100
        data = tune.TuningData("manifest", current, development, "data")
        cross_validation = {
            "artifact_id": "cross-validation",
            "supported": True,
            "selected_regularization": 3e-9,
        }
        fit = {
            "constraints": {
                "variables": ["pawn.isolated.eg"],
                "variable_support": {
                    "pawn.isolated.eg": 160,
                    "pawn.backward.eg": 12,
                },
                "bounds": [[-100, 100]],
            },
            "weights": tune.weight_records(current, parent, weights),
        }

        candidate = tune.candidate_artifact(
            data,
            experiment(support=128),
            cross_validation,
            {"objective": {"scale": 0.7}},
            fit,
        )

        self.assertTrue(candidate["cross_validation_supported"])
        self.assertEqual(candidate["selected_regularization"], 3e-9)
        self.assertEqual(candidate["review"]["bound_hits"], ["pawn.isolated.eg"])
        self.assertEqual(
            candidate["review"]["sparse_support"],
            [{"name": "pawn.backward.eg", "groups": 12}],
        )


class ArtifactTest(unittest.TestCase):
    def test_artifact_output_is_deterministic_and_self_identifying(self):
        artifact = {"kind": "test", "metrics": {"train": 1.0}}
        with tempfile.TemporaryDirectory() as directory:
            first = pathlib.Path(directory) / "first.json"
            second = pathlib.Path(directory) / "second.json"
            tune.write_artifact(first, artifact)
            tune.write_artifact(second, artifact)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(
                tune.read_artifact(first)["artifact_id"],
                tune.artifact_id(json.loads(first.read_text())),
            )
            self.assertNotIn("artifact_id", artifact)

    def test_checkpoint_reuses_completed_output_without_a_registry(self):
        context = {
            "run_id": "run", "kind": "fit", "fold": 0,
            "regularization": 1e-9, "calibration_id": "scale",
        }
        compute = mock.Mock(return_value={"weights": [1, 2]})
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            path = root / "fit.json"
            first = tune.checkpoint(path, context, compute)
            path.with_name("fit.json.partial").write_text("interrupted write")
            second = tune.checkpoint(path, context, compute)
            self.assertEqual(first, second)
            self.assertFalse((root / "state.json").exists())
        compute.assert_called_once_with()

    def test_checkpoint_rejects_a_valid_artifact_from_another_context(self):
        context = {
            "run_id": "run", "kind": "fit", "fold": 0,
            "regularization": 1e-9, "calibration_id": "scale",
        }
        for field, value in (
            ("run_id", "other-run"), ("kind", "calibration"), ("fold", 1),
            ("regularization", 3e-9), ("calibration_id", "other-scale"),
        ):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                path = pathlib.Path(directory) / "fit.json"
                tune.write_artifact(path, {**context, field: value})
                compute = mock.Mock()
                with self.assertRaisesRegex(ValueError, "context mismatch"):
                    tune.checkpoint(path, context, compute)
                compute.assert_not_called()

    def test_checkpoint_retries_an_incomplete_atomic_write(self):
        context = {"run_id": "run", "kind": "calibration", "fold": None}
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "calibration.json"
            path.with_name("calibration.json.partial").write_text("partial")
            result = tune.checkpoint(path, context, lambda: {"scale": 0.7})
            self.assertEqual(result["scale"], 0.7)
            self.assertFalse(path.with_name("calibration.json.partial").exists())

    def test_corrupt_and_legacy_artifacts_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "fit.json"
            artifact = tune.write_artifact(path, {"kind": "fit", "value": 1})
            artifact["value"] = 2
            tune.atomic_write_json(path, artifact)
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                tune.read_artifact(path)
            artifact["format_version"] = 3
            artifact["artifact_id"] = tune.artifact_id(artifact)
            tune.atomic_write_json(path, artifact)
            with self.assertRaisesRegex(ValueError, "original tool revision"):
                tune.read_artifact(path)

    def test_artifact_requires_a_json_object(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "fit.json"
            for value in (None, [], 7):
                with self.subTest(value=value):
                    tune.atomic_write_json(path, value)
                    with self.assertRaisesRegex(ValueError, "invalid tuning artifact"):
                        tune.read_artifact(path)

    def test_checkpoint_context_does_not_coerce_boolean_fold_numbers(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "fit.json"
            tune.write_artifact(path, {"kind": "fit", "fold": False})
            with self.assertRaisesRegex(ValueError, "context mismatch"):
                tune.read_artifact(path, {"kind": "fit", "fold": 0})


def fitting_schema():
    current = base_schema([
        ("pawn.isolated", 0, 0), ("pawn.backward", 0, 0),
        ("pawn.connected_link", 0, 0),
    ])
    names = {feature["name"] for feature in current["features"]}
    constraints = tune.FIT_POLICY["constraints"]
    for anchor in constraints["anchors"]:
        for name in (anchor, tune.mirrored_psqt_name(anchor, set(constraints["mirror_files"]))):
            if name not in names:
                current["features"].append({
                    "id": len(current["features"]), "name": name, "mg": 0, "eg": 0,
                })
                names.add(name)
    return current


def write_prepared_dataset(output, current, engine, paths, policy=None):
    policy = copy.deepcopy(tune.dataset.PREPARATION_POLICY if policy is None else policy)
    policy.update(minimum_games=1, minimum_groups=1)
    output.mkdir(parents=True)
    records = []
    for index in range(32):
        board = tune.dataset.chess.Board.empty()
        board.set_piece_at(index, tune.dataset.chess.Piece.from_symbol("K"))
        board.set_piece_at(63, tune.dataset.chess.Piece.from_symbol("k"))
        item = record(
            [[0, 1], [1, 1], [2, -1], [3, 1], [4, -1], [5, 1]]
            + ([[6, 1]] if index < 3 else []),
            source=f"g{index}:game:8", result=1, phase_counts=(0, 0, 0, 0), pawns=(0, 0),
        )
        item["fen"] = board.fen()
        item["eval"] = tune.dataset.reconstruct(current, item)[0]
        records.append(item)
    data = output / tune.dataset.DATA_FILE
    data.write_text("\n".join(json.dumps(item) for item in (current, *records)) + "\n")
    report, _ = tune.dataset.validate_dataset(output, policy)
    manifest = {
        "format_version": tune.dataset.FORMAT_VERSION,
        "identity": tune.dataset.preparation_identity(engine, paths, policy),
        "collection": {"games.read": 32, "games.valid": 32, "groups.read": 32, "positions.sampled": 32},
        "deduplication": {"positions.exported": 32, "positions.retained": 32},
        "source_results": {"games.1-0": 32, "sampled.1-0": 32},
        "validation": report,
        "outputs": {data.name: tune.sha256_file(data)},
    }
    tune.atomic_write_json(output / "manifest.json", manifest)
    return manifest


def prepared_fixture(root):
    engine = root / "exporter"
    pgn = root / "games.pgn"
    engine.write_text("exporter binary")
    pgn.write_text("original games")
    output = root / "dataset"
    current = fitting_schema()
    write_prepared_dataset(output, current, engine, [pgn])
    return output, current, engine, pgn


class DatasetLoadTest(unittest.TestCase):
    def test_split_loads_compact_arrays_and_checks_exported_evaluation(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        item = record([[0, 2], [5, -1]], turn="b")
        item["eval"] = tune.dataset.reconstruct(current, item)[0]
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "development.jsonl"
            path.write_text(json.dumps(current) + "\n" + json.dumps(item) + "\n")
            split = tune.load_split(path, "development", current)
        self.assertFalse(hasattr(split, "records"))
        self.assertEqual(split.coefficients.nnz, 2)
        self.assertEqual(split.coefficients.dtype, np.int32)
        self.assertEqual(split.fixed.dtype, np.int32)
        self.assertEqual(split.groups.dtype, np.int32)
        np.testing.assert_array_equal(tune.white_exported(split), [-item["eval"]])

    def test_split_rejects_an_exported_evaluation_mismatch(self):
        current = base_schema()
        item = record([[0, 1]])
        item["eval"] = tune.dataset.reconstruct(current, item)[0] + 1
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "development.jsonl"
            path.write_text(json.dumps(current) + "\n" + json.dumps(item) + "\n")
            with self.assertRaisesRegex(ValueError, "baseline reconstruction"):
                tune.load_split(path, "development", current)

    def test_prepared_dataset_remains_usable_without_originals_and_after_relocation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, current, engine, pgn = prepared_fixture(root)
            before = tune.load_dataset(prepared)
            engine.unlink()
            pgn.unlink()
            relocated = root / "relocated"
            prepared.rename(relocated)
            after = tune.load_dataset(relocated)
            self.assertEqual(after.schema, current)
            self.assertEqual(after.manifest_sha256, before.manifest_sha256)
            self.assertEqual(after.data_sha256, before.data_sha256)
            self.assertEqual(len(after.development.targets), 32)

    def test_prepared_dataset_rejects_changed_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            with (prepared / tune.dataset.DATA_FILE).open("a") as stream:
                stream.write("changed\n")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                tune.load_dataset(prepared)

    def test_legacy_preparation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            manifest = tune.load_json(prepared / "manifest.json")
            manifest["format_version"] = 3
            tune.atomic_write_json(prepared / "manifest.json", manifest)
            with self.assertRaisesRegex(ValueError, "unsupported"):
                tune.load_dataset(prepared)


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
                mock.patch.dict(tune.dataset.PREPARATION_POLICY, minimum_games=1, minimum_groups=1),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(tune.dataset, "atomic_build", side_effect=lambda binary, paths, target:
                                  write_prepared_dataset(target, current, binary, paths)),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.prepare_command(args)
                tune.prepare_command(args)
            with self.assertRaisesRegex(ValueError, "invalid anchor"):
                tune.validate_constraints(current, tune.FIT_POLICY)

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
                mock.patch.dict(tune.dataset.PREPARATION_POLICY, minimum_games=1, minimum_groups=1),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(
                    tune.dataset, "atomic_build",
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
            self.assertFalse((output / "state.json").exists())
            self.assertFalse((output / "experiment.json").exists())

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
                mock.patch.object(tune.dataset, "atomic_build") as build,
                self.assertRaisesRegex(ValueError, "schema"),
            ):
                tune.prepare_command(types.SimpleNamespace(engine=engine, output=root / "prepared", pgn=[pgn]))
            build.assert_not_called()


@contextlib.contextmanager
def mocked_optimizer(*, improve=True, interrupt_at=None):
    policy = copy.deepcopy(tune.FIT_POLICY)
    policy["support"]["minimum_groups"] = 3
    policy["validation"]["bootstrap_samples"] = 20
    calls = 0

    def fit(split, current, parameters, fit_policy, scale, regularization):
        nonlocal calls
        calls += 1
        if calls == interrupt_at:
            raise RuntimeError("interrupted fit")
        deltas = np.asarray([
            10.0 if improve and name.startswith("pawn.isolated.") else 0.0
            for name in parameters.names
        ])
        continuous_loss = tune.mean_squared_error(
            tune.continuous_evaluation(split, current, parameters.expand(deltas)),
            split.targets, scale, split.weights,
        )
        penalty = regularization * float(parameters.multiplicity @ (deltas * deltas))
        rounded = parameters.rounded(deltas)
        exact_objective = tune.split_metric(split, current, rounded, scale)["mean_squared_error"] + penalty
        return {
            "parameters": parameters, "rounded": rounded, "deltas": deltas,
            "optimizer": types.SimpleNamespace(nit=1, nfev=1, njev=1, fun=continuous_loss + penalty),
            "selected_iteration": 1, "continuous_loss": continuous_loss,
            "penalty": penalty, "objective": continuous_loss + penalty,
            "exact_objective": exact_objective,
        }

    with (
        mock.patch.object(tune, "FIT_POLICY", policy),
        mock.patch.object(
            tune, "calibrate_scale", return_value=types.SimpleNamespace(x=0.7, nit=1, nfev=2),
        ) as calibrate,
        mock.patch.object(tune, "fit_parameters", side_effect=fit) as optimizer,
        contextlib.redirect_stdout(io.StringIO()),
    ):
        yield optimizer, calibrate


class FitTest(unittest.TestCase):
    def test_fit_reuses_all_checkpoints_and_preserves_fold_constraints_for_refit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            with mocked_optimizer() as (optimizer, calibrate):
                tune.fit_command(args)
                candidate_bytes = (output / "candidate.json").read_bytes()
                tune.fit_command(args)
            self.assertEqual(optimizer.call_count, 46)
            self.assertEqual(calibrate.call_count, 6)
            parameters = optimizer.call_args_list[0].args[2]
            self.assertTrue(all(call.args[2] is parameters for call in optimizer.call_args_list))
            self.assertIn("pawn.backward.mg", parameters.frozen)
            self.assertIn("pawn.backward.eg", parameters.frozen)
            self.assertEqual(candidate_bytes, (output / "candidate.json").read_bytes())
            self.assertTrue(tune.read_artifact(output / "candidate.json")["cross_validation_supported"])
            self.assertFalse((output / "state.json").exists())
            self.assertFalse((output / "experiment.json").exists())

    def test_interrupted_fit_resumes_after_last_complete_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "fit"
            args = types.SimpleNamespace(dataset=prepared, output=output)
            with mocked_optimizer(interrupt_at=2) as (first, _):
                with self.assertRaisesRegex(RuntimeError, "interrupted"):
                    tune.fit_command(args)
            completed = output / "cross-validation" / "fold-0" / "lambda-1e-09.json"
            completed_bytes = completed.read_bytes()
            incomplete = output / "cross-validation" / "fold-0" / "lambda-3e-09.json.partial"
            incomplete.write_text("interrupted write")
            with mocked_optimizer() as (resumed, calibrate):
                tune.fit_command(args)
            self.assertEqual(first.call_count, 2)
            self.assertEqual(resumed.call_count, 45)
            self.assertEqual(calibrate.call_count, 5)
            self.assertEqual(completed.read_bytes(), completed_bytes)
            self.assertFalse(incomplete.exists())
            fresh = root / "fresh-fit"
            with mocked_optimizer():
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=fresh))
            self.assertEqual((output / "candidate.json").read_bytes(), (fresh / "candidate.json").read_bytes())

    def test_fit_resume_accepts_relocated_dataset_without_originals(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, engine, pgn = prepared_fixture(root)
            output = root / "fit"
            with mocked_optimizer(interrupt_at=2):
                with self.assertRaises(RuntimeError):
                    tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
            engine.unlink()
            pgn.unlink()
            relocated = root / "relocated"
            prepared.rename(relocated)
            with mocked_optimizer() as (optimizer, _):
                tune.fit_command(types.SimpleNamespace(dataset=relocated, output=output))
            self.assertEqual(optimizer.call_count, 45)

    def test_unsupported_fit_retains_baseline_without_full_data_refit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, current, _, _ = prepared_fixture(root)
            output = root / "fit"
            with mocked_optimizer(improve=False) as (optimizer, calibrate):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))
            candidate = tune.read_artifact(output / "candidate.json")
            self.assertFalse(candidate["cross_validation_supported"])
            self.assertIsNone(candidate["selected_regularization"])
            self.assertEqual(candidate["changes"], [])
            np.testing.assert_array_equal(tune.weights_from_artifact(candidate, current), tune.baseline_weights(current))
            self.assertEqual(optimizer.call_count, 45)
            self.assertEqual(calibrate.call_count, 5)
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
                for target, change in (
                    ("FIT_POLICY", {**tune.FIT_POLICY, "optimizer": {**tune.FIT_POLICY["optimizer"], "maximum_iterations": 99}}),
                    ("tool_record", {"tune_sha256": "different"}),
                    ("dependency_versions", {"python": "different"}),
                ):
                    with self.subTest(target=target):
                        replacement = mock.Mock(return_value=change) if callable(getattr(tune, target)) else change
                        with mock.patch.object(tune, target, replacement), self.assertRaisesRegex(ValueError, "context mismatch"):
                            tune.fit_command(args)
                manifest = tune.load_json(prepared / "manifest.json")
                manifest["identity"]["inputs"][0]["name"] = "renamed-games.pgn"
                tune.atomic_write_json(prepared / "manifest.json", manifest)
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
                artifact = tune.read_artifact(path)
                artifact["fold"] = 1
                if corruption == "fold":
                    artifact["artifact_id"] = tune.artifact_id(artifact)
                tune.atomic_write_json(path, artifact)
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
                    if optimizer.call_count == 46:
                        with (prepared / tune.dataset.DATA_FILE).open("a") as stream:
                            stream.write("\n")
                    return result

                optimizer.side_effect = change_data
                with self.assertRaisesRegex(ValueError, "changed during fitting"):
                    tune.fit_command(types.SimpleNamespace(dataset=prepared, output=root / "fit"))

    def test_fit_rejects_legacy_registry_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepared, _, _, _ = prepared_fixture(root)
            output = root / "old-fit"
            output.mkdir()
            (output / "state.json").write_text('{"format_version": 3}')
            with self.assertRaisesRegex(ValueError, "original tool revision"):
                tune.fit_command(types.SimpleNamespace(dataset=prepared, output=output))


def verification_fixture(root, *, supported=True):
    output = root / "fit"
    current = fitting_schema()
    run = tune.write_artifact(output / "run.json", {
        "kind": "run", "dataset": {"manifest_sha256": "manifest", "data_sha256": "data"},
        "schema": current, "policy": tune.FIT_POLICY, "tool": {}, "dependencies": {},
    })
    parent = tune.baseline_weights(current).astype(np.int64)
    weights = parent.copy()
    weights[5] += 10
    candidate = tune.write_artifact(output / "candidate.json", {
        "kind": "candidate", "run_id": run["artifact_id"],
        "cross_validation_supported": supported,
        "weights": tune.weight_records(current, parent, weights),
    })
    return output, tune.schema_with_weights(current, weights), candidate


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
                    candidate["artifact_id"] = tune.artifact_id(candidate)
                    tune.atomic_write_json(output / "candidate.json", candidate)
                with self.assertRaisesRegex(ValueError, "cross-validation|context mismatch"):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=root / "engine"))

    def test_verify_rejects_noninteger_proposal_weights(self):
        for value in (10.5, True):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                output, current, candidate = verification_fixture(root)
                candidate["weights"][5]["candidate"]["mg"] = value
                candidate["artifact_id"] = tune.artifact_id(candidate)
                tune.atomic_write_json(output / "candidate.json", candidate)
                engine = root / "engine"
                engine.write_text("build")
                with (
                    mock.patch.object(tune, "read_engine_schema", return_value=current),
                    self.assertRaisesRegex(ValueError, "integers"),
                ):
                    tune.verify_command(types.SimpleNamespace(output=output, engine=engine))

    def test_verify_detects_binary_changed_during_schema_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            output, current, _ = verification_fixture(root)
            engine = root / "engine"
            engine.write_text("original build")

            def export(binary):
                binary.write_text("replacement build")
                return current

            with (
                mock.patch.object(tune, "read_engine_schema", side_effect=export),
                self.assertRaisesRegex(ValueError, "changed during verification"),
            ):
                tune.verify_command(types.SimpleNamespace(output=output, engine=engine))

    def test_verify_rejects_legacy_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "state.json").write_text('{"format_version": 3}')
            with self.assertRaisesRegex(ValueError, "original tool revision"):
                tune.verify_command(types.SimpleNamespace(output=root, engine=root / "engine"))


if __name__ == "__main__":
    unittest.main()
