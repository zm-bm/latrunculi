import json
import pathlib
import tempfile
import unittest
from unittest import mock

import chess
import chess.pgn

from tools.tuning import dataset
from .fixtures import base_schema


def position(source, result, fen, schema=None):
    schema = base_schema() if schema is None else schema
    board = chess.Board(fen)
    coefficients = []
    for index, piece in enumerate((chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN)):
        coefficient = len(board.pieces(piece, chess.WHITE)) - len(board.pieces(piece, chess.BLACK))
        if coefficient:
            coefficients.append([index, coefficient])
    record = {
        "type": "position",
        "version": 2,
        "source": source,
        "result": result,
        "fen": fen,
        "turn": "w" if board.turn else "b",
        "phase_counts": [sum(len(board.pieces(piece, color)) for color in chess.COLORS)
                         for piece in (chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN)],
        "pawn_counts": [len(board.pieces(chess.PAWN, color)) for color in (chess.WHITE, chess.BLACK)],
        "fixed": [0, 0], "coefficients": coefficients,
    }
    record["eval"] = dataset.reconstruct(schema, record)[0]
    return record


TINY_PGN = '''[Event "first"]
[Result "1-0"]
[SetUp "1"]
[FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 6. Re1 b5 1-0

[Event "reverse"]
[Result "0-1"]
[SetUp "1"]
[FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"]

1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Nf3 Be7 5. Bg5 O-O 6. e3 h6 0-1
'''
TINY_POLICY = {**dataset.PREPARATION_POLICY, "minimum_game_ply": 2, "minimum_games": 2, "minimum_groups": 1}


def fixture_export(_, input_path, output_path):
    schema = base_schema()
    with output_path.open("w") as stream:
        stream.write(dataset.canonical_json(schema) + "\n")
        for line in input_path.read_text().splitlines():
            source, result, fen = line.split("\t")
            stream.write(dataset.canonical_json(position(source, int(result), fen, schema)) + "\n")


def prepare_fixture(root):
    engine = root / "engine"
    pgn = root / "games.pgn"
    engine.write_text("fixture engine")
    pgn.write_text(TINY_PGN)
    output = root / "prepared"
    with mock.patch.object(dataset, "export_settled_features", side_effect=fixture_export):
        dataset.atomic_build(engine, [pgn], output, TINY_POLICY)
    return engine, pgn, output


class IdentityTest(unittest.TestCase):
    def test_explicit_fen_groups_across_archives_and_fallback_is_per_game(self):
        paired = chess.pgn.Game()
        paired.setup(chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"))
        reverse = chess.pgn.Game()
        reverse.setup(chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 12 20"))

        self.assertEqual(
            dataset.game_group_key("first", "a.pgn", 1, paired),
            dataset.game_group_key("second", "b.pgn", 7, reverse),
        )

        ordinary = chess.pgn.Game()
        self.assertNotEqual(
            dataset.game_group_key("first", "a.pgn", 1, ordinary),
            dataset.game_group_key("first", "a.pgn", 2, ordinary),
        )


class DeduplicationTest(unittest.TestCase):
    def test_deduplicates_after_settling_and_drops_conflicting_results(self):
        schema = base_schema()
        records = [
            position("b-group:game:1", 1, "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"),
            position("a-group:game:2", 1, "4k3/8/8/8/8/8/4P3/4K3 w - - 8 9"),
            position("c-group:game:3", 1, "4k3/8/8/8/8/8/3P4/4K3 w - - 0 1"),
            position("d-group:game:4", -1, "4k3/8/8/8/8/8/3P4/4K3 w - - 0 1"),
            position("e-group:game:5", 0, "4k3/8/8/8/8/8/2P5/4K3 w - - 0 1"),
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            settled = root / "settled.jsonl"
            settled.write_text(
                "\n".join(dataset.canonical_json(value) for value in [schema, *records]) + "\n"
            )

            counts, returned_schema = dataset.write_development(settled, root)

            retained = [
                json.loads(line)["source"]
                for line in (root / dataset.DATA_FILE).read_text().splitlines()[1:]
            ]
            self.assertEqual(returned_schema, schema)
            self.assertEqual(set(retained), {"a-group:game:2", "e-group:game:5"})
            self.assertEqual(counts["positions.duplicate"], 2)
            self.assertEqual(counts["positions.conflicting"], 1)
            self.assertEqual(counts["positions.conflicting_occurrences"], 2)


class ValidationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = pathlib.Path(self.temporary.name)
        self.engine, self.pgn, self.output = prepare_fixture(self.root)

    def rewrite(self, change):
        path = self.output / dataset.DATA_FILE
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        change(rows)
        path.write_text("".join(dataset.canonical_json(row) + "\n" for row in rows))
        manifest_path = self.output / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["outputs"][dataset.DATA_FILE] = dataset.sha256_file(path)
        manifest_path.write_text(json.dumps(manifest))

    def test_real_schema_two_prepared_dataset_validates_without_originals(self):
        self.engine.unlink()
        self.pgn.unlink()
        report, schema = dataset.validate_output(self.output)
        self.assertEqual(report["schema_version"], 2)
        self.assertEqual(report["feature_count"], 5)
        self.assertEqual(report["positions"], 12)
        self.assertEqual(report["groups"], 1)
        self.assertEqual(schema, base_schema())

    def test_changed_dataset_is_rejected(self):
        path = self.output / dataset.DATA_FILE
        path.write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "output hash mismatch"):
            dataset.validate_output(self.output)

    def test_previous_manifest_format_is_rejected(self):
        path = self.output / "manifest.json"
        manifest = json.loads(path.read_text())
        manifest["format_version"] = 4
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "unsupported dataset manifest"):
            dataset.validate_output(self.output)

    def test_schema_and_evaluation_are_validated_even_with_matching_hashes(self):
        original = (self.output / dataset.DATA_FILE).read_text()
        for change, message in (
            (lambda rows: rows[0].update(version=3), "unsupported dataset schema"),
            (lambda rows: rows[1].update(eval=rows[1]["eval"] + 1), "evaluation"),
        ):
            with self.subTest(message=message):
                (self.output / dataset.DATA_FILE).write_text(original)
                self.rewrite(change)
                with self.assertRaisesRegex(ValueError, message):
                    dataset.validate_output(self.output)


class BuildTest(unittest.TestCase):
    def test_atomic_interrupt_cleans_partial_and_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine, pgn, output = prepare_fixture(root)
            original = (output / "manifest.json").read_bytes()
            with self.assertRaisesRegex(ValueError, "already exists"):
                dataset.atomic_build(engine, [pgn], output, TINY_POLICY)
            self.assertEqual((output / "manifest.json").read_bytes(), original)
            interrupted = root / "interrupted"
            with mock.patch.object(dataset, "export_settled_features", side_effect=KeyboardInterrupt):
                with self.assertRaises(KeyboardInterrupt):
                    dataset.atomic_build(engine, [pgn], interrupted, TINY_POLICY)
            self.assertFalse(interrupted.exists())
            self.assertFalse((root / "interrupted.partial").exists())


if __name__ == "__main__":
    unittest.main()
