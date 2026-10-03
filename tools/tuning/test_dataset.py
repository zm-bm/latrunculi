import copy
import json
import pathlib
import tempfile
import unittest
from unittest import mock

import chess
import chess.pgn

import dataset


def make_schema(extra=()):
    features = [
        ("material.pawn", 100, 166),
        ("material.knight", 600, 680),
        ("material.bishop", 650, 740),
        ("material.rook", 1000, 1100),
        ("material.queen", 2000, 2150),
        *extra,
    ]
    return {
        "type": "schema", "version": 2,
        "perspective": {"coefficients": "white", "fixed": "white", "eval": "side_to_move"},
        "result": "1=white_win,0=draw,-1=black_win",
        "phase_counts": ["knight", "bishop", "rook", "queen"],
        "pawn_counts": ["white", "black"],
        "phase_limit": 128, "phase_material_min": 0, "phase_material_max": 13000,
        "scale_limit": 64, "scale_base": 48, "scale_per_pawn": 4, "tempo": 20,
        "features": [
            {"id": index, "name": name, "mg": mg, "eg": eg}
            for index, (name, mg, eg) in enumerate(features)
        ],
    }


def position(source, result, fen, schema=None):
    schema = make_schema() if schema is None else schema
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
    schema = make_schema()
    with output_path.open("w") as stream:
        stream.write(dataset.canonical_json(schema) + "\n")
        for line in input_path.read_text().splitlines():
            source, result, fen = line.split("\t")
            stream.write(dataset.canonical_json(position(source, int(result), fen, schema)) + "\n")


def prepare_fixture(root, name="prepared"):
    engine = root / "engine"
    pgn = root / "games.pgn"
    engine.write_text("fixture engine")
    pgn.write_text(TINY_PGN)
    output = root / name
    with mock.patch.object(dataset, "export_settled_features", side_effect=fixture_export):
        dataset.atomic_build(engine, [pgn], output, TINY_POLICY)
    return engine, pgn, output


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
        schema = make_schema()
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
        self.assertEqual(schema, make_schema())

    def test_output_hashes_are_checked_before_structural_validation(self):
        path = self.output / dataset.DATA_FILE
        path.write_text("changed\n")
        with mock.patch.object(dataset, "validate_dataset") as validate:
            with self.assertRaisesRegex(ValueError, "output hash mismatch"):
                dataset.validate_output(self.output)
            validate.assert_not_called()

    def test_extra_output_and_legacy_manifest_are_rejected(self):
        path = self.output / "manifest.json"
        original = json.loads(path.read_text())
        for change, message in (
            (lambda manifest: manifest["outputs"].update({"extra.jsonl": "0" * 64}), "invalid dataset outputs"),
            (lambda manifest: manifest.update(format_version=3), "unsupported dataset manifest"),
            (lambda manifest: manifest.update(experiment_sha256="0" * 64), "manifest fields"),
        ):
            with self.subTest(message=message):
                manifest = copy.deepcopy(original)
                change(manifest)
                path.write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, message):
                    dataset.validate_output(self.output)

    def test_reauthenticated_corruption_is_audited(self):
        original = (self.output / dataset.DATA_FILE).read_text()
        changes = (
            (lambda rows: rows[0].update(version=3), "unsupported dataset schema"),
            (lambda rows: rows[0]["features"][1].update(id=0), "feature IDs"),
            (lambda rows: rows[0].update(scale_limit=0), "scaling invariants"),
            (lambda rows: rows[1].update(eval=rows[1]["eval"] + 1), "evaluation"),
            (lambda rows: rows[1].update(coefficients=[[0, 1], [0, 1]]), "coefficient IDs"),
            (lambda rows: rows[1].update(coefficients=[[5, 1]]), "feature coefficients"),
            (lambda rows: rows[1]["pawn_counts"].__setitem__(0, 0), "material counts"),
            (lambda rows: rows[1].update(turn="x"), "side to move"),
            (lambda rows: rows.append(copy.deepcopy(rows[1])), "duplicate source"),
            (lambda rows: rows.append({**rows[1], "source": "new:game:8"}), "duplicate position"),
            (lambda rows: rows[1].update(source="group:game:1"), "minimum ply"),
        )
        for change, message in changes:
            with self.subTest(message=message):
                (self.output / dataset.DATA_FILE).write_text(original)
                self.rewrite(change)
                with self.assertRaisesRegex(ValueError, message):
                    dataset.validate_output(self.output)

    def test_manifest_statistics_and_limits_are_audited(self):
        path = self.output / "manifest.json"
        original = json.loads(path.read_text())
        changes = (
            (lambda manifest: manifest["validation"].update(positions=0), "validation report"),
            (lambda manifest: manifest["identity"]["policy"].update(minimum_groups=2), "minimums"),
            (lambda manifest: manifest["identity"]["policy"].update(maximum_positions_per_game=1), "one game"),
            (lambda manifest: manifest["collection"].update({"positions.sampled": 13}), "collection counts"),
            (lambda manifest: manifest["deduplication"].update({"positions.exported": 13}), "deduplication counts"),
            (lambda manifest: manifest["source_results"].update({"games.1-0": 2}), "source results"),
            (lambda manifest: manifest["identity"]["engine"].update(name="/absolute/engine"), "provenance"),
            (lambda manifest: manifest["identity"]["tool"].update(sha256="z" * 64), "provenance"),
        )
        for change, message in changes:
            with self.subTest(message=message):
                manifest = copy.deepcopy(original)
                change(manifest)
                path.write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, message):
                    dataset.validate_output(self.output)


class SchemaTest(unittest.TestCase):
    def test_feature_count_is_derived_and_compatible_constants_are_allowed(self):
        schema = make_schema([("pawn.connected_link", 3, 7)])
        schema.update(scale_limit=80, scale_base=40, scale_per_pawn=5, tempo=25)
        self.assertEqual(dataset.validate_schema(schema), schema)

    def test_invalid_schema_ids_names_material_and_invariants(self):
        changes = (
            lambda schema: schema.update(type="position"),
            lambda schema: schema.update(version=1),
            lambda schema: schema["features"][0].update(id=False),
            lambda schema: schema["features"][1].update(id=0),
            lambda schema: schema["features"][0].update(name=" "),
            lambda schema: schema["features"][1].update(name="material.pawn"),
            lambda schema: schema["features"][0].update(name="missing.pawn"),
            lambda schema: schema["features"][0].update(mg=True),
            lambda schema: schema["perspective"].update(eval="white"),
            lambda schema: schema.update(phase_limit=64),
            lambda schema: schema.update(phase_material_max=13001),
            lambda schema: schema.update(scale_limit=0),
            lambda schema: schema.update(scale_base=-1),
        )
        for change in changes:
            with self.subTest(change=change):
                schema = make_schema()
                change(schema)
                with self.assertRaises(ValueError):
                    dataset.validate_schema(schema)

    def test_known_signed_reconstruction_remains_exact(self):
        schema = make_schema()
        white = position("group:game:8", 0, "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1")
        black = position("group:game:8", 0, "4k3/4p3/8/8/8/8/8/4K3 b - - 0 1")
        self.assertEqual(dataset.reconstruct(schema, white), (154, 0))
        self.assertEqual(dataset.reconstruct(schema, black), (154, 0))


class BuildTest(unittest.TestCase):
    def test_build_is_deterministic_and_discards_temporary_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine, pgn, first_path = prepare_fixture(root, "first")
            first = json.loads((first_path / "manifest.json").read_text())
            with mock.patch.object(dataset, "export_settled_features", side_effect=fixture_export):
                second = dataset.build_dataset(engine, [pgn], root / "second", TINY_POLICY)

            self.assertEqual(first, second)
            self.assertEqual(
                (root / "first" / "manifest.json").read_bytes(),
                (root / "second" / "manifest.json").read_bytes(),
            )
            self.assertFalse(any(path.is_dir() for path in (root / "first").iterdir()))

    def test_identity_is_portable_and_preparation_inputs_are_pinned(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine, pgn, _ = prepare_fixture(root)
            expected = dataset.preparation_identity(engine, [pgn], TINY_POLICY)
            self.assertEqual(expected["tool"]["sha256"], dataset.sha256_file(pathlib.Path(dataset.__file__)))
            self.assertNotIn(str(root), dataset.canonical_json(expected))
            copied = root / "elsewhere"
            copied.mkdir()
            other_engine, other_pgn = copied / engine.name, copied / pgn.name
            other_engine.write_bytes(engine.read_bytes())
            other_pgn.write_bytes(pgn.read_bytes())
            self.assertEqual(expected, dataset.preparation_identity(other_engine, [other_pgn], TINY_POLICY))
            other_pgn.write_text(other_pgn.read_text() + "\n")
            self.assertNotEqual(expected, dataset.preparation_identity(other_engine, [other_pgn], TINY_POLICY))
            with self.assertRaisesRegex(ValueError, "duplicate PGN"):
                dataset.preparation_identity(engine, [pgn, pgn], TINY_POLICY)
            with self.assertRaisesRegex(ValueError, "policy"):
                dataset.preparation_identity(engine, [pgn], {**TINY_POLICY, "feature_count": 5})

    def test_mutation_during_preparation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            engine, pgn, _ = prepare_fixture(root)

            def changed_export(*arguments):
                fixture_export(*arguments)
                pgn.write_text(pgn.read_text() + "\n")

            with mock.patch.object(dataset, "export_settled_features", side_effect=changed_export):
                with self.assertRaisesRegex(ValueError, "inputs changed"):
                    dataset.atomic_build(engine, [pgn], root / "changed", TINY_POLICY)
            self.assertFalse((root / "changed").exists())
            self.assertFalse((root / "changed.partial").exists())

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
