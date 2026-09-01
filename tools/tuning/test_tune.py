import copy
import json
import math
import pathlib
import sys
import tempfile
import unittest

import numpy as np
from scipy import sparse


sys.path.insert(0, str(pathlib.Path(__file__).parent))
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


def make_split(records, feature_count):
    rows = []
    columns = []
    values = []
    for row, record in enumerate(records):
        for feature_id, coefficient in record["coefficients"]:
            rows.append(row)
            columns.append(feature_id)
            values.append(coefficient)
    return tune.Split(
        records=records,
        coefficients=sparse.csr_matrix(
            (values, (rows, columns)), shape=(len(records), feature_count), dtype=np.float64
        ),
        fixed=np.asarray([record["fixed"] for record in records], dtype=np.float64),
        phase_counts=np.asarray(
            [record["phase_counts"] for record in records], dtype=np.float64
        ),
        pawn_counts=np.asarray([record["pawn_counts"] for record in records], dtype=np.int8),
        turns=np.asarray(
            [1 if record["turn"] == "w" else -1 for record in records], dtype=np.float64
        ),
        targets=np.asarray([(record["result"] + 1) / 2 for record in records]),
        exported=np.asarray([record["eval"] for record in records], dtype=np.float64),
    )


def record(coefficients, fixed=(0, 0), phase_counts=(2, 2, 2, 1), pawns=(4, 4), turn="w"):
    return {
        "type": "position",
        "version": 1,
        "source": "test",
        "result": 0,
        "fen": "",
        "turn": turn,
        "phase_counts": list(phase_counts),
        "pawn_counts": list(pawns),
        "fixed": list(fixed),
        "coefficients": list(coefficients),
        "eval": 0,
    }


def config(anchor_names=(), mirrored=()):
    return {
        "support": {"minimum_positions": 32},
        "constraints": {
            "fixed": ["material.pawn.mg"],
            "anchors": list(anchor_names),
            "mirror_files": list(mirrored),
        },
    }


class ObjectiveTest(unittest.TestCase):
    def test_white_perspective_flips_the_complete_production_evaluation(self):
        records = [record([], turn="w"), record([], turn="b")]
        records[0]["eval"] = 30
        records[1]["eval"] = 30
        split = make_split(records, 0)

        np.testing.assert_array_equal(tune.white_exported(split), [30, -30])
        self.assertAlmostEqual(
            float(tune.expected_score(30, 0.7)),
            1 - float(tune.expected_score(-30, 0.7)),
        )

    def test_calibration_recovers_a_known_scale(self):
        evaluations = np.arange(-800, 801, 100, dtype=np.float64)
        split = make_split([], 0)
        split.exported = evaluations
        split.turns = np.ones(len(evaluations))
        split.targets = tune.expected_score(evaluations, 0.75)

        result = tune.calibrate_scale(
            split,
            {"bounds": [0.0, 2.0], "absolute_tolerance": 1e-12, "maximum_iterations": 256},
        )

        self.assertAlmostEqual(result.x, 0.75, places=7)


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
        weights = tune.baseline_weights(current).astype(np.int64)

        values = tune.exact_evaluations(split, current, weights)

        self.assertEqual(values.tolist(), [-145.0])

    def test_continuous_gradient_matches_finite_differences(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record([[0, 2], [1, 1], [5, -2]], fixed=(13, -7), pawns=(3, 6)),
            record(
                [[2, -1], [3, 1], [4, -1], [5, 3]],
                fixed=(-11, 9),
                phase_counts=(1, 1, 1, 1),
                pawns=(7, 2),
                turn="b",
            ),
        ]
        records[0]["result"] = 1
        records[1]["result"] = -1
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
                    tune.continuous_evaluation(split, current, upper), split.targets, 0.7
                )
                lower_loss = tune.mean_squared_error(
                    tune.continuous_evaluation(split, current, lower), split.targets, 0.7
                )
                numeric[feature_id, phase] = (upper_loss - lower_loss) / (2 * step)

        np.testing.assert_allclose(analytic, numeric, rtol=2e-5, atol=1e-10)

    def test_scaling_uses_candidate_eg_sign_and_stronger_side_pawns(self):
        current = base_schema()
        records = [
            record([], fixed=(0, -4), phase_counts=(0, 0, 0, 0), pawns=(0, 4)),
            record([], fixed=(0, 0), phase_counts=(0, 0, 0, 0), pawns=(0, 4)),
            record([], fixed=(0, 4), phase_counts=(0, 0, 0, 0), pawns=(0, 4)),
        ]
        split = make_split(records, len(current["features"]))

        values = tune.continuous_evaluation(split, current, tune.baseline_weights(current))

        np.testing.assert_array_equal(values, [16, 20, 23])

    def test_phase_clamps_promoted_material_to_midgame(self):
        current = base_schema()
        position = record([], fixed=(80, -40), phase_counts=(10, 10, 10, 10))
        split = make_split([position], len(current["features"]))

        values = tune.continuous_evaluation(split, current, tune.baseline_weights(current))

        np.testing.assert_array_equal(values, [100])


class SupportTest(unittest.TestCase):
    def test_support_reports_and_freezes_sparse_features(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [record([[0, 2], [5, -1]]), record([[0, -1]])]
        split = make_split(records, len(current["features"]))

        report = tune.feature_support(split, current, 2)

        self.assertEqual(
            report[0],
            {
                "id": 0,
                "name": "material.pawn",
                "positions": 2,
                "absolute_coefficient": 3,
                "minimum_coefficient": -1,
                "maximum_coefficient": 2,
                "status": "active",
            },
        )
        self.assertEqual(report[1]["status"], "unsupported")
        self.assertEqual(report[5]["status"], "insufficient")


class ParameterMapTest(unittest.TestCase):
    def test_ties_anchors_support_bounds_and_rounding(self):
        current = base_schema(
            [
                ("psqt.knight.a1", -10, -20),
                ("psqt.knight.h1", -10, -20),
                ("psqt.knight.b1", -5, -8),
                ("psqt.knight.g1", -5, -8),
            ]
        )
        parent = tune.baseline_weights(current)
        coefficients = sparse.csr_matrix(
            (
                [1] * 40,
                (
                    list(range(40)),
                    [7] * 20 + [8] * 20,
                ),
            ),
            shape=(40, len(current["features"])),
        )
        stage = {
            "name": "knight",
            "features": ["psqt.knight.*"],
            "phases": ["mg", "eg"],
            "delta_bounds": [-10, 10],
            "bounds": [
                {
                    "pattern": "psqt.knight.b1.mg",
                    "minimum": -6,
                    "maximum": -3,
                }
            ],
            "regularization": 0.0,
        }

        parameters = tune.build_parameter_map(
            current,
            parent,
            coefficients,
            config(["psqt.knight.a1"], ["knight"]),
            stage,
        )

        self.assertEqual(parameters.names, ["psqt.knight.b1.mg", "psqt.knight.b1.eg"])
        self.assertEqual(parameters.bounds, [(-1.0, 2.0), (-10.0, 10.0)])
        self.assertEqual(parameters.frozen["psqt.knight.a1.mg"], "anchor")
        self.assertEqual(parameters.support["psqt.knight.b1.mg"], 40)
        rounded = parameters.rounded([2.6, -0.5])
        self.assertEqual(
            rounded[5:9].tolist(),
            [[-10, -20], [-10, -20], [-3, -9], [-3, -9]],
        )

    def test_selection_cannot_split_a_mirror_tie(self):
        current = base_schema(
            [("psqt.knight.a1", -10, -20), ("psqt.knight.h1", -10, -20)]
        )
        stage = {
            "name": "knight",
            "features": ["psqt.knight.a1"],
            "phases": ["mg"],
            "delta_bounds": [-10, 10],
            "bounds": [],
            "regularization": 0.0,
        }

        with self.assertRaisesRegex(ValueError, "splits mirror tie"):
            tune.build_parameter_map(
                current,
                tune.baseline_weights(current),
                sparse.csr_matrix((1, len(current["features"]))),
                config(["psqt.knight.a1"], ["knight"]),
                stage,
            )

    def test_tied_support_uses_the_combined_coefficient(self):
        current = base_schema(
            [
                ("psqt.knight.a1", -10, -20),
                ("psqt.knight.h1", -10, -20),
                ("psqt.knight.b1", -5, -8),
                ("psqt.knight.g1", -5, -8),
            ]
        )
        coefficients = sparse.csr_matrix(
            (
                [1, -1] * 40,
                (
                    [row for row in range(40) for _ in range(2)],
                    [7, 8] * 40,
                ),
            ),
            shape=(40, len(current["features"])),
        )
        stage = {
            "name": "knight",
            "features": ["psqt.knight.*"],
            "phases": ["mg", "eg"],
            "delta_bounds": [-10, 10],
            "bounds": [],
            "regularization": 0.0,
        }

        with self.assertRaisesRegex(ValueError, "all selected parameters are frozen"):
            tune.build_parameter_map(
                current,
                tune.baseline_weights(current),
                coefficients,
                config(["psqt.knight.a1"], ["knight"]),
                stage,
            )

    def test_bounds_must_include_the_parent(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        settings = config()
        settings["support"]["minimum_positions"] = 1
        stage = {
            "name": "pawns",
            "features": ["pawn.isolated"],
            "phases": ["mg"],
            "delta_bounds": [-20, 20],
            "bounds": [
                {"pattern": "pawn.isolated.mg", "minimum": 0, "maximum": 10}
            ],
            "regularization": 0.0,
        }

        with self.assertRaisesRegex(ValueError, "exclude parent"):
            tune.build_parameter_map(
                current,
                tune.baseline_weights(current),
                sparse.csr_matrix(([1], ([0], [5])), shape=(1, len(current["features"]))),
                settings,
                stage,
            )

    def test_selected_table_requires_an_anchor(self):
        current = base_schema([("mobility.knight.0", -40, -48)])
        stage = {
            "name": "mobility",
            "features": ["mobility.knight.*"],
            "phases": ["mg"],
            "delta_bounds": [-10, 10],
            "bounds": [],
            "regularization": 0.0,
        }

        with self.assertRaisesRegex(ValueError, "no anchor"):
            tune.build_parameter_map(
                current,
                tune.baseline_weights(current),
                sparse.csr_matrix((1, len(current["features"]))),
                config(),
                stage,
            )


class OptimizerTest(unittest.TestCase):
    def test_selected_fit_is_deterministic_and_leaves_other_weights_fixed(self):
        current = base_schema([("pawn.isolated", -5, -15)])
        records = [
            record([[5, coefficient]], phase_counts=(0, 0, 0, 0))
            for coefficient in range(-4, 5)
        ]
        train = make_split(records, len(current["features"]))
        validation = make_split(records, len(current["features"]))
        target_weights = tune.baseline_weights(current)
        target_weights[5, 1] = 30
        for split in (train, validation):
            split.targets = tune.expected_score(
                tune.continuous_evaluation(split, current, target_weights), 0.7
            )

        data = tune.TuningData(
            manifest={},
            manifest_sha256="manifest",
            schema=current,
            splits={"train": train, "validation": validation},
        )
        settings = config()
        settings["support"]["minimum_positions"] = 1
        settings["optimizer"] = {
            "method": "L-BFGS-B",
            "maximum_iterations": 200,
            "gradient_tolerance": 1e-12,
            "function_tolerance": 1e-15,
            "maximum_line_search_steps": 50,
        }
        stage = {
            "name": "pawn-structure",
            "features": ["pawn.isolated"],
            "phases": ["eg"],
            "delta_bounds": [-100, 100],
            "bounds": [],
            "regularization": 0.0,
        }
        first = tune.fit_parameters(
            data, tune.baseline_weights(current), settings, stage, 0.7
        )
        second = tune.fit_parameters(
            data, tune.baseline_weights(current), settings, stage, 0.7
        )

        np.testing.assert_array_equal(first["deltas"], second["deltas"])
        np.testing.assert_array_equal(first["rounded"][:5], tune.baseline_weights(current)[:5])
        self.assertEqual(first["rounded"][5, 0], -5)
        self.assertAlmostEqual(first["rounded"][5, 1], 30, delta=1)


class ArtifactTest(unittest.TestCase):
    def test_candidate_id_and_output_are_deterministic(self):
        artifact = {"artifact_version": 1, "metrics": {"train": 1.0}}
        with tempfile.TemporaryDirectory() as directory:
            first = pathlib.Path(directory) / "first.json"
            second = pathlib.Path(directory) / "second.json"

            tune.write_artifact(first, dict(artifact))
            tune.write_artifact(second, dict(artifact))

            self.assertEqual(first.read_bytes(), second.read_bytes())
            written = json.loads(first.read_text())
            self.assertEqual(written["candidate_id"], tune.artifact_id(written))


class DatasetLoadTest(unittest.TestCase):
    def test_loads_endgame_validation_without_opening_heldout(self):
        current = base_schema()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            outputs = {}
            for filename in tune.SPLIT_FILES.values():
                path = root / filename
                path.write_text(json.dumps(current) + "\n")
                outputs[filename] = tune.sha256_file(path)
            (root / "heldout.jsonl").write_text("not json\n")
            outputs["heldout.jsonl"] = "not-read"
            manifest = {
                "config": {"schema_version": 1},
                "inputs": [],
                "outputs": outputs,
                "engine": {"sha256": "engine"},
            }
            (root / "manifest.json").write_text(json.dumps(manifest))
            settings = {
                "dataset": {
                    "manifest_sha256": tune.sha256_file(root / "manifest.json"),
                    "schema_version": 1,
                    "feature_count": len(current["features"]),
                    "inputs": [],
                    "corpus_policy": "test",
                }
            }

            data = tune.load_dataset(root, settings)

            self.assertEqual(set(data.splits), set(tune.SPLIT_FILES))


class ParentTest(unittest.TestCase):
    def test_parent_must_preserve_the_fixed_configuration_and_objective(self):
        current = base_schema()
        settings = {
            "dataset": {"corpus_policy": "test"},
            "objective": {
                "perspective": "white",
                "result_mapping": "-1=0,0=0.5,1=1",
                "sigmoid": "1/(1+10^(-k*eval/400))",
                "loss": "mean_squared_error",
                "weighting": "position",
            },
            "support": {"minimum_positions": 32},
        }
        data = tune.TuningData(
            manifest={
                "inputs": [],
                "outputs": {filename: filename for filename in tune.SPLIT_FILES.values()},
            },
            manifest_sha256="manifest",
            schema=current,
            splits={},
        )
        parent = {
            "artifact_version": 1,
            "kind": "calibration",
            "dataset": tune.dataset_record(data, settings),
            "configuration": settings,
            "objective": {**settings["objective"], "scale": 0.7},
        }
        tune.validate_parent_artifact(parent, data, settings)

        changed = copy.deepcopy(parent)
        changed["configuration"]["support"]["minimum_positions"] = 1
        with self.assertRaisesRegex(ValueError, "different configuration"):
            tune.validate_parent_artifact(changed, data, settings)

        changed = copy.deepcopy(parent)
        changed["objective"]["sigmoid"] = "different"
        with self.assertRaisesRegex(ValueError, "different objective"):
            tune.validate_parent_artifact(changed, data, settings)


if __name__ == "__main__":
    unittest.main()
