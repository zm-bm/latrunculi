import unittest

import numpy as np

from tools.tuning import dataset, model
from .fixtures import base_schema, make_split, record


class ObjectiveTest(unittest.TestCase):
    def test_group_weights_give_every_group_equal_total_weight(self):
        groups = np.asarray(["a", "a", "a", "b"], dtype=object)
        weights = model.group_weights(groups)
        np.testing.assert_allclose(weights, [1 / 3, 1 / 3, 1 / 3, 1])
        self.assertAlmostEqual(weights[:3].sum(), weights[3:].sum())

    def test_white_perspective_flips_the_complete_production_evaluation(self):
        records = [record([], source="a:x:1", turn="w"), record([], source="b:x:1", turn="b")]
        records[0]["eval"] = 30
        records[1]["eval"] = 30
        split = make_split(records, 0)

        np.testing.assert_array_equal(model.white_exported(split), [30, -30])
        self.assertAlmostEqual(
            float(model.expected_score(30, 0.7)),
            1 - float(model.expected_score(-30, 0.7)),
        )


class ModelTest(unittest.TestCase):
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
        values = model.exact_evaluations(split, current, model.baseline_weights(current).astype(int))
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
        parent = model.baseline_weights(current).astype(int)
        candidate = parent.copy()
        candidate[1, 0] += 37
        for index, weights in enumerate((parent, candidate)):
            actual, phases = model.exact_evaluations_and_phases(split, current, weights)
            candidate_schema = model.schema_with_weights(current, weights)
            expected = []
            expected_phases = []
            for item in records:
                value, phase = dataset.reconstruct(candidate_schema, item)
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
        weights = model.baseline_weights(current)
        _, analytic = model.continuous_loss_gradient(split, current, weights, 0.7)
        numeric = np.empty_like(weights)
        step = 1e-4
        for feature_id in range(len(weights)):
            for phase in range(2):
                upper = weights.copy()
                lower = weights.copy()
                upper[feature_id, phase] += step
                lower[feature_id, phase] -= step
                upper_loss = model.mean_squared_error(
                    model.continuous_evaluation(split, current, upper),
                    split.targets,
                    0.7,
                    split.weights,
                )
                lower_loss = model.mean_squared_error(
                    model.continuous_evaluation(split, current, lower),
                    split.targets,
                    0.7,
                    split.weights,
                )
                numeric[feature_id, phase] = (upper_loss - lower_loss) / (2 * step)
        np.testing.assert_allclose(analytic, numeric, rtol=2e-5, atol=1e-10)


if __name__ == "__main__":
    unittest.main()
