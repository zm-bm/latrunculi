import json
import pathlib
import tempfile
import unittest
from unittest import mock

import chess
import chess.pgn

import dataset


def position(source, result, fen):
    return {
        "type": "position",
        "version": 1,
        "source": source,
        "result": result,
        "fen": fen,
    }


class IdentityTest(unittest.TestCase):
    def test_canonical_fen_ignores_move_counters(self):
        first = chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1")
        second = chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 37 81")
        self.assertEqual(dataset.canonical_fen(first), dataset.canonical_fen(second))

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

    def test_quantile_sampling_is_bounded_and_spread(self):
        self.assertEqual(dataset.quantile_indices(100, 6), [8, 25, 41, 58, 75, 91])
        self.assertEqual(dataset.quantile_indices(3, 6), [0, 1, 2])
        self.assertEqual(dataset.quantile_indices(0, 6), [])


class CollectionTest(unittest.TestCase):
    def test_collects_at_most_six_positions_per_game(self):
        pgn = """[Event \"first\"]
[Result \"1-0\"]
[SetUp \"1\"]
[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 6. Re1 b5 1-0

[Event \"reverse\"]
[Result \"0-1\"]
[SetUp \"1\"]
[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]

1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Nf3 Be7 5. Bg5 O-O 6. e3 h6 0-1
"""
        config = {
            "minimum_game_ply": 2,
            "maximum_positions_per_game": 6,
            "minimum_games": 2,
            "minimum_groups": 1,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "games.pgn"
            output = root / "positions.tsv"
            source.write_text(pgn)

            counts, _ = dataset.collect_positions([(source, "input-hash")], config, output)

            lines = output.read_text().splitlines()
            self.assertEqual(counts["games.valid"], 2)
            self.assertEqual(counts["groups.read"], 1)
            self.assertEqual(len(lines), 12)
            self.assertEqual(len({line.split(":", 1)[0] for line in lines}), 1)

    def test_game_without_fen_is_retained_as_an_independent_group(self):
        pgn = """[Event "ordinary"]
[Result "1/2-1/2"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 1/2-1/2
"""
        config = {
            "minimum_game_ply": 2,
            "maximum_positions_per_game": 6,
            "minimum_games": 1,
            "minimum_groups": 1,
        }
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "games.pgn"
            output = root / "positions.tsv"
            source.write_text(pgn)

            counts, _ = dataset.collect_positions([(source, "input-hash")], config, output)

            self.assertEqual(counts["games.valid"], 1)
            self.assertEqual(counts["groups.read"], 1)
            self.assertEqual(counts["groups.singleton"], 1)
            self.assertEqual(len(output.read_text().splitlines()), 6)


class DeduplicationTest(unittest.TestCase):
    def test_deduplicates_after_settling_and_drops_conflicting_results(self):
        schema = {"type": "schema", "version": 1, "features": []}
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
    def test_output_hashes_are_checked_before_structural_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            path = root / dataset.DATA_FILE
            path.write_text("schema\n")
            experiment = {"version": 1}
            manifest = {
                "format_version": dataset.FORMAT_VERSION,
                "experiment_sha256": dataset.sha256_json(experiment),
                "outputs": {path.name: dataset.sha256_file(path)},
                "validation": {"valid": True},
            }
            (root / "manifest.json").write_text(json.dumps(manifest))
            with mock.patch.object(
                dataset,
                "validate_dataset",
                return_value=({"valid": True}, {"type": "schema"}),
            ) as validate:
                dataset.validate_output(root)
                validate.assert_called_once_with(root)

                manifest["outputs"]["extra.jsonl"] = dataset.sha256_file(path)
                (root / "manifest.json").write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, "invalid dataset outputs"):
                    dataset.validate_output(root)

                manifest["outputs"].pop("extra.jsonl")
                (root / "manifest.json").write_text(json.dumps(manifest))
                path.write_text("changed\n")
                with self.assertRaisesRegex(ValueError, "output hash mismatch"):
                    dataset.validate_output(root)


class BuildTest(unittest.TestCase):
    def test_build_is_deterministic_and_discards_temporary_files(self):
        schema = {"type": "schema", "version": 1, "features": []}
        experiment = {
            "dataset": {
                "schema_version": 1,
                "feature_count": 0,
                "minimum_groups": 0,
            }
        }

        def collect(_, __, path):
            path.write_text("")
            return {"positions.sampled": 0}, {}

        def export(_, __, path):
            path.write_text(dataset.canonical_json(schema) + "\n")

        def write(_, output):
            (output / dataset.DATA_FILE).write_text(
                dataset.canonical_json(schema) + "\n"
            )
            return {"positions.exported": 0}, schema

        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine = root / "engine"
            pgn = root / "games.pgn"
            engine.write_text("engine")
            pgn.write_text("games")
            with (
                mock.patch.object(dataset, "collect_positions", side_effect=collect),
                mock.patch.object(dataset, "export_settled_features", side_effect=export),
                mock.patch.object(dataset, "write_development", side_effect=write),
            ):
                first = dataset.build_dataset(engine, [pgn], root / "first", experiment)
                second = dataset.build_dataset(engine, [pgn], root / "second", experiment)

            self.assertEqual(first, second)
            self.assertEqual(
                (root / "first" / "manifest.json").read_bytes(),
                (root / "second" / "manifest.json").read_bytes(),
            )
            self.assertFalse(any(path.is_dir() for path in (root / "first").iterdir()))

if __name__ == "__main__":
    unittest.main()
