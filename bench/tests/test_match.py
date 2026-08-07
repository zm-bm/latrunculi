from __future__ import annotations

import unittest
from pathlib import Path

from bench.benchlib.match import (
    build_match_command,
    is_abnormal_termination,
    parse_finished_games,
    strength_conclusion,
    summarize_results,
)


def finished_game(game: int, candidate_color: str, candidate_score: float) -> str:
    candidate_white = candidate_color == "white"
    white, black = ("candidate", "baseline") if candidate_white else ("baseline", "candidate")
    if candidate_score == 0.5:
        result, termination = "1/2-1/2", "Draw by repetition"
    else:
        candidate_won = candidate_score == 1.0
        white_won = candidate_won == candidate_white
        result, termination = ("1-0", "White mates") if white_won else ("0-1", "Black mates")
    return f"Finished game {game} ({white} vs {black}): {result} {{{termination}}}"


class MatchBenchmarkTest(unittest.TestCase):
    def test_parses_pairs_and_summarizes_pentanomial_results(self) -> None:
        pair_scores = ((0.0, 0.0), (0.0, 0.5), (0.5, 0.5), (0.5, 1.0), (1.0, 1.0))
        lines = []
        for pair, scores in enumerate(pair_scores):
            lines.append(finished_game(pair * 2 + 1, "white", scores[0]))
            lines.append(finished_game(pair * 2 + 2, "black", scores[1]))

        rows = parse_finished_games(reversed(lines), len(pair_scores))
        statistics = summarize_results(rows)

        self.assertEqual([int(row["game"]) for row in rows], list(range(1, 11)))
        self.assertEqual(
            (statistics["wins"], statistics["draws"], statistics["losses"]), (3, 4, 3)
        )
        self.assertEqual(statistics["pentanomial"], [1, 1, 1, 1, 1])
        self.assertEqual(
            strength_conclusion("standard", "Elo difference: 5.0 +/- 4.0,"),
            "candidate improvement supported",
        )
        self.assertEqual(
            strength_conclusion("standard", "Elo difference: 5.0 +/- 6.0,"),
            "inconclusive",
        )

    def test_rejects_incomplete_malformed_and_unpaired_results(self) -> None:
        valid = [finished_game(1, "white", 1.0), finished_game(2, "black", 0.0)]
        cases = {
            "missing": valid[:1],
            "malformed": [valid[0].replace("1-0", "*"), valid[1]],
            "not swapped": [valid[0], finished_game(2, "white", 0.0)],
        }
        for name, lines in cases.items():
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                parse_finished_games(lines, 1)

        self.assertFalse(is_abnormal_termination("Black mates"))
        self.assertTrue(is_abnormal_termination("White disconnects"))
        self.assertTrue(is_abnormal_termination("Black loses on time"))

    def test_builds_fixed_smoke_and_standard_commands(self) -> None:
        common = {
            "cutechess": Path("/tools/cutechess-cli"),
            "candidate": Path("/engines/candidate"),
            "baseline": Path("/engines/baseline"),
            "openings": Path("/books/openings.pgn"),
            "pgn": Path("/results/games.pgn"),
            "pairs": 3,
            "concurrency": 2,
        }
        smoke = build_match_command(profile="smoke", **common)
        standard = build_match_command(profile="standard", **common)

        self.assertIn("tc=inf", smoke)
        self.assertIn("depth=1", smoke)
        self.assertNotIn("-draw", smoke)
        self.assertIn("tc=10+0.1", standard)
        self.assertIn("timemargin=250", standard)
        self.assertEqual(
            standard[standard.index("-draw") + 1 : standard.index("-resign")],
            ["movenumber=34", "movecount=8", "score=20"],
        )
        self.assertEqual(
            standard[standard.index("-resign") + 1 : standard.index("-maxmoves")],
            ["movecount=3", "score=600", "twosided=true"],
        )
        self.assertEqual(standard[standard.index("-maxmoves") + 1], "200")
        for command in (smoke, standard):
            self.assertLess(command.index("name=candidate"), command.index("name=baseline"))
            self.assertIn("option.Hash=32", command)
            self.assertIn("option.Threads=1", command)
            self.assertIn("file=/books/openings.pgn", command)
            self.assertIn("order=sequential", command)
            self.assertIn("start=1", command)
            self.assertIn("-repeat", command)
            self.assertIn("policy=round", command)
            self.assertEqual(command[command.index("-games") + 1], "2")
            self.assertEqual(command[command.index("-rounds") + 1], "3")
            self.assertEqual(command[command.index("-concurrency") + 1], "2")
            self.assertEqual(command[command.index("-srand") + 1], "1")


if __name__ == "__main__":
    unittest.main()
