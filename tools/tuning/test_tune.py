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
        "version": 1,
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
        "version": 1,
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


def complete_experiment(name="run-test"):
    path = pathlib.Path(__file__).with_name("experiment.example.json")
    config = tune.load_experiment(path)
    config["name"] = name
    config["baseline"] = {"revision": "1" * 40, "benchmark": 42}
    return config


def experiment_input(name="run-test"):
    config = json.loads(pathlib.Path(__file__).with_name("experiment.example.json").read_text())
    config["name"] = name
    config["baseline"] = {"revision": "1" * 40, "benchmark": 42}
    return config


class ConfigurationTest(unittest.TestCase):
    def test_example_resolves_the_fixed_joint_protocol(self):
        config = tune.load_experiment(
            pathlib.Path(__file__).with_name("experiment.example.json")
        )
        self.assertEqual(config["version"], 3)
        self.assertEqual(config["dataset"]["maximum_positions_per_game"], 6)
        self.assertNotIn("splits", config["dataset"])
        self.assertEqual(
            config["fit"]["regularization"],
            [1e-9, 3e-9, 1e-8, 3e-8, 1e-7, 3e-7, 1e-6, 3e-6, 1e-5],
        )
        self.assertEqual(config["validation"]["folds"], 5)
        self.assertEqual(config["strength"]["normalized_elo_bounds"], [0, 3])
        self.assertNotIn("features", config["fit"])
        self.assertNotIn("phases", config["fit"])

    def test_experiment_input_rejects_protocol_overrides(self):
        config = experiment_input()
        config["fit"] = {"regularization": [0]}
        with self.assertRaisesRegex(ValueError, "invalid experiment configuration"):
            tune.resolve_experiment(config)

    def test_experiment_input_rejects_boolean_numbers(self):
        config = experiment_input()
        config["baseline"]["benchmark"] = True
        with self.assertRaisesRegex(ValueError, "benchmark"):
            tune.resolve_experiment(config)

    def test_version_two_experiments_are_not_accepted(self):
        config = experiment_input()
        config["version"] = 2
        with self.assertRaisesRegex(ValueError, "unsupported experiment version"):
            tune.resolve_experiment(config)


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
        data = tune.TuningData("manifest", current, development)
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
            tune.write_artifact(first, dict(artifact))
            tune.write_artifact(second, dict(artifact))
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(
                tune.read_artifact(first)["artifact_id"],
                tune.artifact_id(json.loads(first.read_text())),
            )

    def test_completed_steps_are_immutable_and_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            artifact = root / "artifact.json"
            artifact.write_text("first")
            state = root / "state.json"
            tune.atomic_write_json(state, {"steps": {}})
            tune.record_step(state, "test", artifact)
            tune.record_step(state, "test", artifact)
            artifact.write_text("changed")
            with self.assertRaisesRegex(ValueError, "completed step"):
                tune.validate_steps(root, tune.load_json(state))

    def test_result_ledger_rejects_a_reused_experiment_name(self):
        with tempfile.TemporaryDirectory() as directory:
            ledger = pathlib.Path(directory) / "results.jsonl"
            first = {"experiment": "same-name", "experiment_sha256": "a" * 64}
            second = {"experiment": "same-name", "experiment_sha256": "b" * 64}
            with mock.patch.object(tune, "RESULTS_PATH", ledger):
                tune.record_result(first)
                with self.assertRaisesRegex(ValueError, "conflicts"):
                    tune.record_result(second)


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

    def test_dataset_loads_the_single_development_file(self):
        current = base_schema()
        experiment_config = {
            "dataset": {"schema_version": 1, "feature_count": len(current["features"])},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            path = root / tune.dataset.DATA_FILE
            item = record([], source="development:game:1")
            item["eval"] = tune.dataset.reconstruct(current, item)[0]
            path.write_text(json.dumps(current) + "\n" + json.dumps(item) + "\n")
            manifest = {
                "format_version": tune.dataset.FORMAT_VERSION,
                "experiment_sha256": tune.dataset.sha256_json(experiment_config),
                "outputs": {path.name: tune.sha256_file(path)},
                "engine": {"sha256": "engine"},
            }
            (root / "manifest.json").write_text(json.dumps(manifest))
            data = tune.load_dataset(root, experiment_config)
            self.assertEqual(len(data.development.targets), 1)


def prepare_output(root, current, cross_validation_supported=True):
    config = complete_experiment("lifecycle-test")
    config["dataset"]["feature_count"] = len(current["features"])
    tune.atomic_write_json(root / "experiment.json", config)
    tune.atomic_write_json(
        root / "state.json",
        {
            "format_version": tune.STATE_FORMAT_VERSION,
            "experiment": config["name"],
            "experiment_sha256": tune.dataset.sha256_json(config),
            "tool": tune.tool_record(),
            "dependencies": tune.dependency_versions(),
            "steps": {},
        },
    )
    weights = tune.baseline_weights(current).astype(int)
    tune.write_artifact(
        root / "candidate.json",
        {
            "kind": "candidate",
            "cross_validation_supported": cross_validation_supported,
            "selected_regularization": 1e-9 if cross_validation_supported else None,
            "weights": tune.weight_records(current, weights, weights),
        },
    )
    tune.record_step(root / "state.json", "candidate", root / "candidate.json")
    candidate = tune.read_artifact(root / "candidate.json")
    return config, candidate


class LifecycleTest(unittest.TestCase):
    def test_version_two_state_is_not_accepted(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepare_output(root, current)
            state = tune.load_json(root / "state.json")
            state["format_version"] = 2
            tune.atomic_write_json(root / "state.json", state)
            with self.assertRaisesRegex(ValueError, "unsupported experiment state"):
                tune.read_state(root)

    def test_compiled_candidate_verification_compares_the_complete_schema(self):
        current = base_schema()
        weights = tune.baseline_weights(current).astype(int)
        candidate = {
            "artifact_id": "candidate",
            "weights": tune.weight_records(current, weights, weights),
        }
        with tempfile.TemporaryDirectory() as directory:
            engine = pathlib.Path(directory) / "engine"
            engine.write_text("engine")
            with mock.patch.object(tune, "read_engine_schema", return_value=current):
                verification = tune.candidate_verification(candidate, current, engine)
            self.assertEqual(verification["candidate_id"], "candidate")

            changed = copy.deepcopy(current)
            changed["tempo"] += 1
            with (
                mock.patch.object(tune, "read_engine_schema", return_value=changed),
                self.assertRaisesRegex(ValueError, "does not match"),
            ):
                tune.candidate_verification(candidate, current, engine)

    def test_verify_records_the_compiled_candidate(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            _, candidate = prepare_output(root, current, True)
            dataset_dir = root / "dataset"
            dataset_dir.mkdir()
            development = dataset_dir / tune.dataset.DATA_FILE
            development.write_text(json.dumps(current) + "\n")
            tune.atomic_write_json(
                dataset_dir / "manifest.json",
                {
                    "outputs": {tune.dataset.DATA_FILE: tune.sha256_file(development)}
                },
            )
            engine = root / "engine"
            engine.write_text("engine")
            with (
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.verify_command(types.SimpleNamespace(output=root, engine=engine))

            state = tune.load_json(root / "state.json")
            self.assertIn("verification", state["steps"])
            self.assertEqual(
                tune.load_json(root / "verification.json")["candidate_id"],
                candidate["artifact_id"],
            )

    def test_validate_checks_the_experiment_dataset(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            config, _ = prepare_output(root, current, False)
            dataset_dir = root / "dataset"
            dataset_dir.mkdir()
            tune.atomic_write_json(
                dataset_dir / "manifest.json",
                {"experiment_sha256": tune.dataset.sha256_json(config)},
            )
            report = {"duplicates": 0}
            stream = io.StringIO()
            with (
                mock.patch.object(tune.dataset, "validate_output", return_value=(report, current)),
                contextlib.redirect_stdout(stream),
            ):
                tune.validate_command(types.SimpleNamespace(output=root))
            self.assertEqual(json.loads(stream.getvalue()), report)

    def test_strength_result_requires_compiled_verification(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepare_output(root, current, True)
            args = types.SimpleNamespace(
                output=root,
                result="upper",
                reason="upper boundary",
                openbench_test="42",
            )
            with (
                mock.patch.object(tune, "RESULTS_PATH", root / "results.jsonl"),
                self.assertRaisesRegex(ValueError, "not been verified"),
            ):
                tune.close_command(args)

    def test_close_derives_decision_and_records_one_durable_result(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            _, candidate = prepare_output(root, current, True)
            tune.atomic_write_json(
                root / "verification.json",
                {"candidate_id": candidate["artifact_id"]},
            )
            tune.record_step(root / "state.json", "verification", root / "verification.json")
            args = types.SimpleNamespace(
                output=root,
                result="upper",
                reason="upper boundary",
                openbench_test="42",
            )
            ledger = root / "results.jsonl"
            with (
                mock.patch.object(tune, "RESULTS_PATH", ledger),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.close_command(args)
                tune.close_command(args)

            records = [json.loads(line) for line in ledger.read_text().splitlines()]
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0]["decision"], "accepted")
            self.assertEqual(records[0]["result"], "upper")

    def test_lower_and_inconclusive_results_reject_the_candidate(self):
        current = base_schema()
        for result in ("lower", "inconclusive"):
            with self.subTest(result=result), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                _, candidate = prepare_output(root, current, True)
                tune.atomic_write_json(
                    root / "verification.json",
                    {"candidate_id": candidate["artifact_id"]},
                )
                tune.record_step(
                    root / "state.json", "verification", root / "verification.json"
                )
                args = types.SimpleNamespace(
                    output=root,
                    result=result,
                    reason="strength test did not pass",
                    openbench_test="42",
                )
                with (
                    mock.patch.object(tune, "RESULTS_PATH", root / "results.jsonl"),
                    contextlib.redirect_stdout(io.StringIO()),
                ):
                    tune.close_command(args)
                self.assertEqual(
                    tune.load_json(root / "decision.json")["decision"], "rejected"
                )

    def test_offline_rejection_has_no_openbench_test(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepare_output(root, current, False)
            invalid = types.SimpleNamespace(
                output=root,
                result="offline",
                reason="failed validation",
                openbench_test="42",
            )
            with (
                mock.patch.object(tune, "RESULTS_PATH", root / "results.jsonl"),
                self.assertRaisesRegex(ValueError, "no OpenBench"),
            ):
                tune.close_command(invalid)

            valid = types.SimpleNamespace(
                output=root,
                result="offline",
                reason="failed validation",
                openbench_test=None,
            )
            with (
                mock.patch.object(tune, "RESULTS_PATH", root / "results.jsonl"),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.close_command(valid)
            self.assertEqual(
                tune.load_json(root / "decision.json")["decision"], "rejected"
            )

    def test_verify_rejects_an_unsupported_candidate(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            prepare_output(root, current, False)
            with self.assertRaisesRegex(ValueError, "cross-validation"):
                tune.verify_command(
                    types.SimpleNamespace(output=root, engine=root / "engine")
                )


class RunnerTest(unittest.TestCase):
    def test_run_resumes_completed_atomic_steps(self):
        config = complete_experiment()
        source_config = experiment_input()
        current = base_schema()
        split = make_split(
            [record([], source=f"g{index}:x:1") for index in range(5)],
            len(current["features"]),
        )
        data = tune.TuningData("manifest", current, split)
        assignments = np.arange(5, dtype=np.int8)

        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            experiment_path = root / "experiment.json"
            engine = root / "engine"
            pgn = root / "games.pgn"
            output = root / config["name"]
            experiment_path.write_text(json.dumps(source_config))
            engine.write_text("engine")
            pgn.write_text("games")
            args = types.SimpleNamespace(
                experiment=experiment_path,
                engine=engine,
                output=output,
                pgn=[pgn],
            )

            def build_dataset(_, __, path, ___):
                path.mkdir()
                (path / "manifest.json").write_text(
                    json.dumps(
                        {
                            "inputs": [
                                {"name": pgn.name, "sha256": tune.sha256_file(pgn)}
                            ],
                            "engine": {"sha256": tune.sha256_file(engine)},
                        }
                    )
                )

            calibration = {
                "kind": "calibration",
                "objective": {"scale": 0.7},
            }
            fit = {
                "kind": "fit",
                "constraints": {
                    "variables": [],
                    "variable_support": {},
                    "bounds": [],
                },
                "weights": [],
            }
            cross_validation = {
                "kind": "cross_validation",
                "supported": True,
                "selected_regularization": 1e-9,
            }
            candidate = {
                "kind": "candidate",
                "cross_validation_supported": True,
                "selected_regularization": 1e-9,
                "review": {},
                "weights": [],
            }

            with (
                mock.patch.object(tune, "read_benchmark", return_value=42),
                mock.patch.object(tune, "read_engine_schema", return_value=current),
                mock.patch.object(tune, "validate_schema"),
                mock.patch.object(tune.dataset, "atomic_build", side_effect=build_dataset) as build,
                mock.patch.object(tune, "load_dataset", return_value=data),
                mock.patch.object(tune, "validate_constraints"),
                mock.patch.object(
                    tune, "fold_assignments", return_value=assignments
                ),
                mock.patch.object(
                    tune, "build_parameter_map", return_value=object()
                ) as build_parameters,
                mock.patch.object(tune, "feature_support", return_value=[]),
                mock.patch.object(
                    tune,
                    "calibrate_scale",
                    return_value=types.SimpleNamespace(x=0.7, nit=1, nfev=2),
                ) as calibrate,
                mock.patch.object(tune, "calibration_artifact", return_value=calibration),
                mock.patch.object(tune, "fit_parameters", return_value={}) as fit_parameters,
                mock.patch.object(tune, "fit_artifact", return_value=fit),
                mock.patch.object(
                    tune,
                    "cross_validation_artifact",
                    return_value=cross_validation,
                ) as cross_validate,
                mock.patch.object(
                    tune, "candidate_artifact", return_value=candidate
                ) as select,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                tune.run_command(args)
                tune.run_command(args)

            self.assertEqual(build.call_count, 1)
            self.assertEqual(build_parameters.call_count, 2)
            self.assertEqual(calibrate.call_count, 6)
            self.assertEqual(fit_parameters.call_count, 46)
            self.assertEqual(cross_validate.call_count, 1)
            self.assertEqual(select.call_count, 1)
            self.assertNotIn("dataset", json.loads(experiment_path.read_text()))
            self.assertIn("dataset", tune.load_json(output / "experiment.json"))
            status = tune.status_record(output)
            self.assertTrue(status["cross_validation_supported"])
            self.assertIn("run verify", status["next_action"])
            stream = io.StringIO()
            with contextlib.redirect_stdout(stream):
                tune.status_command(types.SimpleNamespace(output=output, json=True))
            self.assertEqual(json.loads(stream.getvalue())["experiment"], config["name"])

    def test_state_rejects_changed_input_hash(self):
        config = complete_experiment()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine = root / "engine"
            pgn = root / "games.pgn"
            engine.write_text("engine")
            pgn.write_text("first")
            state = tune.initial_state(config, engine, [pgn])
            pgn.write_text("second")
            with self.assertRaisesRegex(ValueError, "inputs changed"):
                tune.validate_state(state, config, engine, [pgn])


if __name__ == "__main__":
    unittest.main()
