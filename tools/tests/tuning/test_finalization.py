import copy
import json
import tempfile
import unittest
from pathlib import Path

import chess
import numpy as np

from tools.tuning import artifacts, dataset, finalization, fit, model, tune
from tools.tests.tuning import fixtures


def example_data():
    schema = fixtures.base_schema([(f"reserved.{i}", 0, 0) for i in range(5, 529)])
    schema["version"] = 9
    names = ["material.minor_pawn_balance", *finalization.COMPLEXITY_NAMES, "endgame.scale_strength"]
    for name in names:
        schema["features"].append(dict(id=len(schema["features"]), name=name, mg=0, eg=0))
    schema["finalization"] = dict(material=529, complexity=[530, 531, 532, 533], strength=534,
                                  context="pieces-flanks-bishops-v1")
    fens = [
        "6k1/8/8/8/8/8/P6p/2B1Kb2 w - - 0 1",
        "6k1/8/8/8/8/8/P6p/2B1Kb2 b - - 0 1",
        "6k1/1n6/8/8/8/8/8/2BNK3 w - - 0 1",
        "6k1/8/8/8/8/8/P7/4K3 w - - 0 1",
        "6k1/8/8/8/8/8/P6p/4K3 b - - 0 1",
        "6k1/8/8/8/8/8/PP2N3/2B1K3 w - - 0 1",
    ]
    records = []
    for n, fen in enumerate(fens):
        board = chess.Board(fen)
        context = finalization.context_from_board(board)
        w, b = context["pieces"]
        coefficients = {i: w[i] - b[i] for i in range(5)}
        coefficients.update(finalization.expected_coefficients(schema, context))
        r = fixtures.record(sorted([i, c] for i, c in coefficients.items() if c),
                            source=f"group{n}:game:20", result=n % 3 - 1,
                            fixed=(37 * n - 83, 53 * n - 131),
                            phase_counts=[w[i] + b[i] for i in range(1, 5)],
                            pawns=[w[0], b[0]], turn='w' if board.turn else 'b')
        r.update(version=9, fen=fen, finalization=context)
        r['eval'] = dataset.reconstruct(schema, r)[0]
        records.append(r)
    return schema, records


def split_from_records(records, schema):
    split = fixtures.make_split(records, len(schema['features']))
    split.finalization_context = np.asarray([
        r['finalization']['pieces'][0] + r['finalization']['pieces'][1]
        + r['finalization']['pawn_flanks'] + r['finalization']['bishop_colors'] for r in records], dtype=np.int16)
    return split


class FinalizationTests(unittest.TestCase):
    def test_schema_context_and_dataset_loading(self):
        schema, records = example_data()
        dataset.validate_schema(schema)
        for r in records:
            dataset._validate_record(schema, r, None)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'rows.jsonl'
            path.write_text('\n'.join(json.dumps(x) for x in [schema, *records]) + '\n')
            split = model.load_split(path, 'test', schema)
            self.assertEqual(split.finalization_context.shape, (6, 14))
            self.assertEqual(model.subset_split(split, np.array([True, False] * 3)).finalization_context.shape, (3, 14))
        bad = copy.deepcopy(records[0]); bad['finalization']['pawn_flanks'][0] = 3
        with self.assertRaisesRegex(ValueError, 'context'):
            dataset._validate_record(schema, bad, None)
        bad = copy.deepcopy(records[0]); bad['coefficients'].append([534, 9])
        with self.assertRaises(ValueError):
            dataset._validate_record(schema, bad, None)

    def test_zero_terms_reproduce_legacy_schemas(self):
        schema, records = example_data()
        for version in (4, 7, 8):
            legacy = copy.deepcopy(schema); legacy['version'] = version
            legacy.pop('finalization'); legacy['features'] = legacy['features'][:529]
            dataset.validate_schema(legacy)
            old = [{**r, 'version': version, 'coefficients': [pair for pair in r['coefficients'] if pair[0] < 529]} for r in records]
            expected = [dataset.reconstruct(legacy, r)[0] for r in old]
            self.assertEqual([dataset.reconstruct(schema, r)[0] for r in records], expected)
            split = split_from_records(records, schema)
            np.testing.assert_array_equal(model.exact_evaluations(split, schema, model.baseline_weights(schema).astype(np.int64)), split.turns * expected)

    def test_integer_scalar_and_vector_paths_match(self):
        schema, records = example_data()
        split = split_from_records(records, schema)
        weights = model.baseline_weights(schema).astype(np.int64)
        weights[529] = [7, -3]
        weights[530:534, 1] = [4, 11, 27, -73]
        for strength in (0, 1, 17, 37, 63, 64):
            weights[534, 1] = strength
            changed = model.schema_with_weights(schema, weights)
            expected = np.asarray([dataset.reconstruct(changed, r)[0] for r in records]) * split.turns
            np.testing.assert_array_equal(model.exact_evaluations(split, schema, weights), expected)

    def test_nonlinear_gradients_match_finite_differences(self):
        schema, records = example_data()
        split = split_from_records(records, schema)
        weights = model.baseline_weights(schema)
        weights[529] = [3.2, 2.1]
        weights[530:534, 1] = [2.3, 7.1, 13.4, -46.2]
        weights[534, 1] = 37.4
        loss, gradient = model.continuous_loss_gradient(split, schema, weights, 1.3)
        expected_loss = model.mean_squared_error(model.continuous_evaluation(split, schema, weights), split.targets, 1.3, split.weights)
        self.assertAlmostEqual(loss, expected_loss, places=12)
        for ident, phase in [(529, 0), (529, 1), *[(i, 1) for i in range(530, 535)]]:
            h = 1e-4
            plus = weights.copy(); plus[ident, phase] += h
            minus = weights.copy(); minus[ident, phase] -= h
            finite = (model.continuous_loss_gradient(split, schema, plus, 1.3)[0] - model.continuous_loss_gradient(split, schema, minus, 1.3)[0]) / (2 * h)
            self.assertAlmostEqual(gradient[ident, phase], finite, places=8)

    def test_selection_freezes_existing_coordinates_and_records_patterns(self):
        schema, _ = example_data()
        policy = fit.selected_policy(schema, ['endgame.complexity.*.eg', 'material.minor_pawn_balance.*'])
        for f in schema['features'][:529]:
            for phase in ('mg', 'eg'):
                self.assertIn(f'{f["name"]}.{phase}', policy['constraints']['fixed'])
        self.assertEqual(len(policy['selection']['coordinates']), 6)
        with self.assertRaisesRegex(ValueError, 'matches nothing'):
            fit.selected_policy(schema, ['endgame.missing.*'])
        legacy = fixtures.base_schema()
        self.assertEqual(fit.selected_policy(legacy), fit.FIT_POLICY)

    def test_nonlinear_weights_require_zero_mg_and_declared_bounds(self):
        schema, _ = example_data()
        schema['features'][530]['mg'] = 1
        with self.assertRaisesRegex(ValueError, 'EG-only'):
            dataset.validate_schema(schema)
        schema['features'][530]['mg'] = 0
        schema['features'][534]['eg'] = 65
        with self.assertRaisesRegex(ValueError, 'bounds'):
            dataset.validate_schema(schema)

    def test_cli_selection_is_repeatable_and_changes_fit_identity(self):
        args = tune.parse_args(['fit', '--dataset', 'example', '--name', 'trial',
                                '--only', 'endgame.complexity.*.eg', '--only', 'endgame.scale_strength.eg'])
        self.assertEqual(args.only, ['endgame.complexity.*.eg', 'endgame.scale_strength.eg'])
        schema, records = example_data()
        data = model.TuningData('0' * 64, schema, split_from_records(records, schema), '1' * 64)
        a = artifacts.fit_descriptor(data, fit.selected_policy(schema, args.only))
        b = artifacts.fit_descriptor(data, fit.selected_policy(schema, ['material.minor_pawn_balance.*']))
        self.assertNotEqual(artifacts.artifact_id(a), artifacts.artifact_id(b))

    def test_actual_parameter_map_cannot_change_frozen_weights(self):
        schema, records = example_data()
        split = split_from_records(records, schema)
        policy = fit.selected_policy(schema, ['material.minor_pawn_balance.*'])
        # The compact fixture omits the real exporter's PSQT anchor families.
        policy['constraints']['anchors'] = []
        policy['constraints']['mirror_files'] = []
        policy['support']['minimum_groups'] = 1
        parent = model.baseline_weights(schema)
        parameters = fit.build_parameter_map(schema, parent, split, policy)
        self.assertEqual(set(parameters.names), {'material.minor_pawn_balance.mg', 'material.minor_pawn_balance.eg'})
        changed = parameters.expand(np.ones(len(parameters.names)))
        np.testing.assert_array_equal(changed[:529], parent[:529])
        np.testing.assert_array_equal(changed[530:], parent[530:])

    def test_unscoped_legacy_bound_validation_is_unchanged(self):
        schema, records = example_data()
        schema['version'] = 8
        schema.pop('finalization')
        split = split_from_records(records, schema)
        policy = fit.selected_policy(schema)
        policy['constraints']['anchors'] = []
        policy['constraints']['mirror_files'] = []
        policy['support']['minimum_groups'] = 1
        policy['fit']['bounds'] = [dict(pattern='material.pawn.mg', minimum=50, maximum=300)]
        with self.assertRaisesRegex(ValueError, 'bound pattern matches nothing'):
            fit.build_parameter_map(schema, model.baseline_weights(schema), split, policy)

    def test_invalid_roles_cannot_silently_linearize_finalization(self):
        schema, _ = example_data()
        for change in ('missing', 'unassigned', 'invalid_id'):
            bad = copy.deepcopy(schema)
            if change == 'missing':
                del bad['finalization']['strength']
            elif change == 'unassigned':
                bad['finalization']['complexity'] = []
            else:
                bad['finalization']['complexity'][0] = None
            with self.assertRaises(ValueError):
                dataset.validate_schema(bad)

    def test_scale_only_schema_preserves_full_trial_predictions(self):
        schema, records = example_data()
        schema['features'][534]['eg'] = 37
        projected = copy.deepcopy(schema)
        projected['features'] = projected['features'][:529] + [projected['features'][534]]
        projected['features'][529]['id'] = 529
        projected['finalization'].update(material=None, complexity=[], strength=529)
        dataset.validate_schema(projected)
        rows = []
        for record in records:
            row = copy.deepcopy(record)
            row['coefficients'] = [[529 if i == 534 else i, c]
                                   for i, c in row['coefficients'] if i < 529 or i == 534]
            expected = dataset.reconstruct(schema, record)[0]
            row['eval'] = expected
            self.assertEqual(dataset.reconstruct(projected, row)[0], expected)
            dataset._validate_record(projected, row, None)
            rows.append(row)
        split = split_from_records(rows, projected)
        np.testing.assert_array_equal(
            model.exact_evaluations(split, projected, model.baseline_weights(projected).astype(np.int64)),
            model.white_exported(split))
