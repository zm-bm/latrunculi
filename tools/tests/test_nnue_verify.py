"""Verification gates must reject corrupted evidence and incorrect native output."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from tools.nnue.verify import NETWORK_SHA, adapted_score, check, digest


class HalfKpVerificationTests(unittest.TestCase):
    def fixture(self, root):
        (root/'corpus.jsonl').write_text(json.dumps({'start_fen':'test', 'moves':[]})+'\n')
        (root/'oracle-scores.json').write_text('[0]')
        (root/'oracle-gate.json').write_text(json.dumps({'passed':True,
            'corpus_sha256':digest(root/'corpus.jsonl'),
            'scores_sha256':digest(root/'oracle-scores.json')}))
        binary=root/'native';binary.write_bytes(b'fixture')
        return binary

    def test_corrupt_inputs_are_rejected_before_launch(self):
        for filename in ['corpus.jsonl','oracle-scores.json']:
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as name:
                root=Path(name);binary=self.fixture(root)
                (root/filename).write_text('corrupt')
                with mock.patch('tools.nnue.verify.subprocess.run') as run:
                    with self.assertRaisesRegex(ValueError,'identity mismatch'):
                        check(root,binary,'test')
                    run.assert_not_called()

    def test_native_mismatch_does_not_publish_a_pass(self):
        for change in [{'raw':1},{'score':28},{'network_sha256':'wrong'}]:
            with self.subTest(change=change), tempfile.TemporaryDirectory() as name:
                root=Path(name);binary=self.fixture(root)
                row={'raw':0,'score':27,'network_sha256':NETWORK_SHA,**change}
                response=subprocess.CompletedProcess([],0,json.dumps(row)+'\n','')
                with mock.patch('tools.nnue.verify.subprocess.run',return_value=response):
                    with self.assertRaises(ValueError):check(root,binary,'test')
                self.assertFalse((root/'test.json').exists())
                self.assertTrue((root/'test.stdout.jsonl').exists())

    def test_adapter_signed_rounding_and_clamps(self):
        for raw,expected in [(0,27),(-1,26),(-23,0),(-24,-1),(24,56),(2**31-1,30000),(-2**31,-30000)]:
            self.assertEqual(adapted_score(raw),expected)
