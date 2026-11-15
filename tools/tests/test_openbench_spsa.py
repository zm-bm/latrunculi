import contextlib
import copy
import io
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from tools import openbench, openbench_spsa as spsa


def recipe():
    value = json.loads((spsa.RECIPES/'nnue-pruning.json').read_text())
    value['parameters'] = [
        dict(name='FutilityMargin1', min=125, max=375, c_end=12.5, r_end=.002),
        dict(name='NullMoveSurplusPerPly', min=100, max=300, c_end=10, r_end=.002),
        dict(name='LmrQuietDivisor', min=1.25, max=3.75, c_end=.125, r_end=.002),
    ]
    return value


def metadata():
    return {'format': 'latrunculi.search-parameters.v1', 'parameters': [
        dict(name='FutilityMargin1', type='int', default=250, min=0, max=30000),
        dict(name='NullMoveSurplusPerPly', type='int', default=200, min=1, max=30000),
        dict(name='LmrQuietDivisor', type='float', default=2.5, min=.5, max=10),
    ]}


def option_line(p):
    if p['type'] == 'float':
        return f"option name {p['name']} type string default {p['default']}"
    return f"option name {p['name']} type spin default {p['default']} min {p['min']} max {p['max']}"


class RecipeTests(unittest.TestCase):
    def test_malformed_recipe_schedule_is_rejected(self):
        value = recipe()
        value['schedule'] = []
        with self.assertRaises(ValueError):
            spsa.parameter_rows(metadata(), value)

    def test_preparation_keeps_the_recipe_snapshot_used_before_bench(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            binary = folder/'engine'; binary.write_text('binary'); binary.chmod(0o700)
            path = folder/'recipe.json'; original = json.dumps(recipe()).encode(); path.write_bytes(original)
            args = openbench.parse_prepare_args(['spsa','--candidate','HEAD','--candidate-engine',str(binary),
                '--games','2000','--recipe',str(path),'--description','test','--output',str(folder/'prepared')])
            uci = '\n'.join(option_line(p)
                            for p in metadata()['parameters'])
            def bench(*args, **kwargs):
                path.write_text('{}')
                return 5185395
            with patch.object(openbench,'revision',return_value='1'*40), \
                    patch.object(spsa.subprocess,'run',side_effect=[Mock(stdout=json.dumps(metadata())),Mock(stdout=uci)]), \
                    patch.object(spsa.engine,'bench_nodes',side_effect=bench), contextlib.redirect_stdout(io.StringIO()):
                result = openbench.prepare(args)
            self.assertEqual((result.parent/'recipe.json').read_bytes(),original)
            candidate = json.loads((result.parent/'candidate.json').read_text())
            self.assertEqual(candidate['recipe_sha256'],hashlib.sha256(original).hexdigest())

    def test_defaults_come_from_engine_and_campaign_policy_from_recipe(self):
        rows = spsa.parameter_rows(metadata(), recipe())
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[2], ('LmrQuietDivisor', 'float', 2.5, 1.25, 3.75, .125, .002))
        self.assertEqual(rows[0], ('FutilityMargin1', 'int', 250, 125, 375, 12.5, .002))
        changed = metadata()
        changed['parameters'][0]['default'] = 260
        self.assertEqual(spsa.parameter_rows(changed, recipe())[0][2], 260)

    def test_unknown_duplicate_and_unsafe_parameters_are_rejected(self):
        bads = []
        value = recipe(); value['parameters'][0]['name'] = 'Unknown'; bads.append(value)
        value = recipe(); value['parameters'].append(value['parameters'][0]); bads.append(value)
        value = recipe(); value['parameters'][0]['min'] = -1; bads.append(value)
        value = recipe(); value['parameters'][0]['max'] = 240; bads.append(value)
        value = recipe(); value['parameters'][0]['c_end'] = 0; bads.append(value)
        for value in bads:
            with self.subTest(value=value), self.assertRaises((ValueError, openbench.cli.OpenBenchError)):
                spsa.parameter_rows(metadata(), value)
        value = metadata(); value['parameters'].append(value['parameters'][0])
        with self.assertRaises(ValueError): spsa.parameter_rows(value, recipe())

    def test_fractional_metadata_and_bounds_must_be_finite_and_typed(self):
        for invalid in (float('nan'), float('inf'), True):
            value = metadata(); value['parameters'][2]['default'] = invalid
            with self.subTest(metadata=invalid), self.assertRaises(ValueError):
                spsa.parameter_rows(value, recipe())
            value = recipe(); value['parameters'][2]['max'] = invalid
            with self.subTest(bound=invalid), self.assertRaises(ValueError):
                spsa.parameter_rows(metadata(), value)
        value = recipe(); value['parameters'][0]['min'] = 125.5
        with self.assertRaises(ValueError):
            spsa.parameter_rows(metadata(), value)

    def prepare(self, temporary, *, uci=True, games='2000'):
        binary = temporary/'engine'; binary.write_text('binary'); binary.chmod(0o700)
        args = openbench.parse_prepare_args(['spsa','--candidate','HEAD','--candidate-engine',str(binary),
            '--games',games,'--description','SPSA test','--output',str(temporary/'prepared')])
        args.recipe = temporary/'recipe.json'
        args.recipe.write_text(json.dumps(recipe()))
        discovery = '\n'.join(option_line(p)
                              for p in metadata()['parameters']) if uci else 'uciok'
        with patch.object(openbench,'revision',return_value='1'*40), \
                patch.object(spsa.subprocess,'run',side_effect=[Mock(stdout=json.dumps(metadata())), Mock(stdout=discovery)]), \
                patch.object(spsa.engine,'bench_nodes',return_value=5185395) as bench, \
                contextlib.redirect_stdout(io.StringIO()):
            path = openbench.prepare(args)
        return path, bench

    def test_offline_preparation_records_exact_budget_recipe_and_binary_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path, bench = self.prepare(Path(directory))
            value = json.loads(path.read_text())
            self.assertEqual(value['dev_engine'],'Latrunculi-Tuning')
            self.assertEqual(int(value['spsa_iterations'])*2*int(value['spsa_pairs_per']),2000)
            self.assertEqual(len(spsa.client.parameters(value['spsa_inputs'])),3)
            self.assertEqual(json.loads((path.parent/'recipe.json').read_text()),recipe())
            self.assertEqual(len(json.loads((path.parent/'candidate.json').read_text())['sha256']),64)
            bench.assert_called_once()

    def test_disagreeing_uci_or_inexact_budget_fails_before_bench(self):
        for kwargs in [dict(uci=False),dict(games='2002')]:
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(ValueError): self.prepare(Path(directory),**kwargs)
                self.assertFalse((Path(directory)/'prepared').exists())

    def test_existing_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory)/'prepared').mkdir()
            with self.assertRaises(ValueError): self.prepare(Path(directory))


if __name__ == '__main__': unittest.main()
