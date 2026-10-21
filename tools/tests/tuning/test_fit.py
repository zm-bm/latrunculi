import types
import unittest
from unittest import mock

import numpy as np

from tools.tuning import model, fit as fitting
from .fixtures import base_schema, fit_policy, make_split, record


class SupportTest(unittest.TestCase):
    def test_support_counts_distinct_groups(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record([[5, 1]], source="a:x:1"),
            record([[5, -1]], source="a:y:2"),
            record([[0, 1]], source="b:x:1"),
        ]
        split = make_split(records, len(current["features"]))
        report = fitting.feature_support(split, current, 2)
        self.assertEqual(report[5]["positions"], 2)
        self.assertEqual(report[5]["groups"], 1)
        self.assertEqual(report[5]["status"], "insufficient")


class ParameterMapTest(unittest.TestCase):
    def test_tied_parameters_share_support_bounds_and_rounding(self):
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
        settings = fit_policy(
            anchors=("psqt.knight.a1",),
            mirrored=("knight",),
            support=32,
        )
        settings["fit"]["bounds"] = [
            {"pattern": "psqt.knight.b1.mg", "minimum": -6, "maximum": -3}
        ]
        parameters = fitting.build_parameter_map(
            current, model.baseline_weights(current), split, settings
        )

        self.assertEqual(parameters.names, ["psqt.knight.b1.mg", "psqt.knight.b1.eg"])
        self.assertEqual(parameters.support["psqt.knight.b1.mg"], 40)
        np.testing.assert_array_equal(parameters.multiplicity, [2, 2])
        self.assertEqual(parameters.frozen["psqt.knight.a1.mg"], "anchor")
        self.assertEqual(parameters.bounds, [(-1.0, 2.0), (-100.0, 100.0)])
        rounded = parameters.rounded([2.6, -0.5])
        self.assertEqual(rounded[5:9].tolist(), [[-10, -20], [-10, -20], [-3, -9], [-3, -9]])

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
        settings = fit_policy(support=2)
        settings["validation"]["folds"] = 2
        assignments = np.asarray([0, 0, 0, 1, 1], dtype=np.int8)

        parameters = fitting.build_parameter_map(
            current,
            model.baseline_weights(current),
            split,
            settings,
            assignments,
        )

        self.assertEqual(parameters.support["pawn.isolated.mg"], 1)
        self.assertEqual(parameters.frozen["pawn.isolated.mg"], "support")
        self.assertEqual(parameters.frozen["pawn.isolated.eg"], "support")


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
        target = model.baseline_weights(current)
        target[5, 1] = 30
        train.targets = model.expected_score(
            model.continuous_evaluation(train, current, target), 0.7
        )
        settings = fit_policy(fixed=("material.pawn.mg", "pawn.isolated.mg"))
        parameters = fitting.build_parameter_map(
            current, model.baseline_weights(current), train, settings
        )

        first = fitting.fit_parameters(train, current, parameters, settings, 0.7, 0.0)
        second = fitting.fit_parameters(train, current, parameters, settings, 0.7, 0.0)
        np.testing.assert_array_equal(first["deltas"], second["deltas"])
        self.assertAlmostEqual(first["rounded"][5, 1], 30, delta=1)

    def test_checkpoint_selection_uses_rounded_loss_and_penalty(self):
        current = base_schema([("pawn.isolated", 0, 0)])
        split = make_split([record([[5, 1]])], len(current["features"]))
        parameters = fitting.ParameterMap(
            parent=model.baseline_weights(current),
            members=[[(5, 1)]],
            names=["pawn.isolated.eg"],
            bounds=[(-100, 100)],
            support={"pawn.isolated.eg": 1},
            frozen={},
            multiplicity=np.asarray([1.0]),
        )

        def exact_loss(_, __, weights, ___):
            return {0: 0.3, 1: 0.12, 2: 0.1}[weights[5, 1]]

        for penalty, last, final, weight, iteration, objective in (
            (0.0, 0.49, 0.49, 2, 1, 0.1),
            (0.02, 0.51, 1.51, 1, 2, 0.14),
        ):
            with self.subTest(penalty=penalty):
                def minimize(*_, callback, **__):
                    callback(np.asarray([1.51]))
                    callback(np.asarray([last]))
                    return types.SimpleNamespace(
                        x=np.asarray([final]), nit=2, nfev=2, njev=2,
                        fun=0.0, success=True, message="ok",
                    )

                with (
                    mock.patch.object(fitting.optimize, "minimize", side_effect=minimize),
                    mock.patch.object(model, "exact_loss", side_effect=exact_loss),
                ):
                    result = fitting.fit_parameters(
                        split, current, parameters, fit_policy(), 0.7, penalty
                    )

                self.assertEqual(result["rounded"][5, 1], weight)
                self.assertEqual(result["selected_iteration"], iteration)
                self.assertEqual(result["optimizer"].x[0], final)
                self.assertAlmostEqual(result["exact_objective"], objective)


class ValidationTest(unittest.TestCase):
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

        best, threshold, selected = fitting.select_regularization(reports)

        self.assertEqual(best["regularization"], 1e-9)
        self.assertAlmostEqual(threshold, 0.00063)
        self.assertEqual(selected["regularization"], 3e-9)

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
        assignments = fitting.fold_assignments(split, policy)
        np.testing.assert_array_equal(
            assignments, fitting.fold_assignments(split, policy)
        )

        testing_positions = 0
        for fold in range(policy["folds"]):
            training = model.subset_split(split, assignments != fold)
            testing = model.subset_split(split, assignments == fold)
            testing_positions += len(testing.targets)
            self.assertFalse(set(training.groups) & set(testing.groups))
        self.assertEqual(testing_positions, len(split.targets))


class CalibrationTest(unittest.TestCase):
    def test_calibration_recovers_a_known_scale_with_group_weights(self):
        evaluations = np.arange(-800, 801, 100, dtype=np.float64)
        records = [record([], source=f"g{i}:x:1") for i in range(len(evaluations))]
        split = make_split(records, 0)
        split.exported = evaluations
        split.targets = model.expected_score(evaluations, 0.75)

        result = fitting.calibrate_scale(
            split,
            {"bounds": [0.0, 2.0], "absolute_tolerance": 1e-12, "maximum_iterations": 256},
        )
        self.assertAlmostEqual(result.x, 0.75, places=7)


if __name__ == "__main__":
    unittest.main()
