import importlib.util
import pathlib
import unittest

import chess


MODULE_PATH = pathlib.Path(__file__).with_name("dataset.py")
SPEC = importlib.util.spec_from_file_location("latrunculi_dataset", MODULE_PATH)
dataset = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dataset)


CONFIG = {
    "seed": 20260827,
    "splits": {"train": 80, "validation": 10, "heldout": 10},
}


class DatasetTest(unittest.TestCase):
    def test_canonical_fen_ignores_move_counters(self):
        first = chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1")
        second = chess.Board("4k3/8/8/8/8/8/4P3/4K3 w - - 37 81")
        self.assertEqual(dataset.canonical_fen(first), dataset.canonical_fen(second))

    def test_split_is_deterministic(self):
        self.assertEqual(
            dataset.split_for("game-pair-1", CONFIG),
            dataset.split_for("game-pair-1", CONFIG),
        )
        self.assertIn(dataset.split_for("game-pair-1", CONFIG), dataset.SPLITS)

    def test_game_group_uses_member_round_and_game_fallback(self):
        paired = dataset.game_group_key("games.tar", "part-1.pgn", 1, "7", chess.STARTING_FEN)
        reverse = dataset.game_group_key("games.tar", "part-1.pgn", 2, "7", chess.STARTING_FEN)
        other_member = dataset.game_group_key(
            "games.tar", "part-2.pgn", 1, "7", chess.STARTING_FEN
        )
        unknown_first = dataset.game_group_key(
            "games.tar", "part-1.pgn", 1, "?", chess.STARTING_FEN
        )
        unknown_second = dataset.game_group_key(
            "games.tar", "part-1.pgn", 2, "?", chess.STARTING_FEN
        )

        self.assertEqual(paired, reverse)
        self.assertNotEqual(paired, other_member)
        self.assertNotEqual(unknown_first, unknown_second)

    def test_quiet_policy_rejects_captures_and_promotions(self):
        quiet = chess.Board()
        capture = chess.Board("4k3/8/8/8/8/3p4/4P3/4K3 w - - 0 1")
        promotion = chess.Board("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")

        self.assertFalse(dataset.has_legal_capture_or_promotion(quiet))
        self.assertTrue(dataset.has_legal_capture_or_promotion(capture))
        self.assertTrue(dataset.has_legal_capture_or_promotion(promotion))


if __name__ == "__main__":
    unittest.main()
