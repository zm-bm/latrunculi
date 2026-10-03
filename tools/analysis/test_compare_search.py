import argparse
import contextlib
import csv
import io
import tempfile
import unittest
from pathlib import Path

from tools.analysis import compare_search


FIELDS = ("result_format", "case", "fen",
          *compare_search.CONFIG, *compare_search.SIGNATURE, "total_ns")
FENS = {
    "one": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "two": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1",
}


def write_run(path, change=None):
    with path.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        for case, fen in FENS.items():
            row = dict(result_format=compare_search.FORMAT, case=case, fen=fen,
                       limit_type="depth",
                       limit_value="5", threads="1", hash_mb="32",
                       completed_depth="5", static_score="0", score="12",
                       nodes="100", best_move="e2e4", pv="e2e4 e7e5",
                       total_ns="1000")
            if change:
                change(case, row)
            writer.writerow(row)


class CompareSearchTest(unittest.TestCase):
    def test_accepts_custom_corpus_and_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline, candidate = (Path(directory) / name for name in ("b.tsv", "c.tsv"))
            write_run(baseline)
            write_run(candidate, lambda _, row: row.update(nodes="90"))
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                compare_search.nodes(argparse.Namespace(
                    baseline=baseline, candidate=candidate, repeat=None,
                    exact_tree=False, details=True))
            self.assertIn("cases=2", output.getvalue())
            self.assertIn("geometric_mean_node_ratio=0.900000000", output.getvalue())

    def test_rejects_changed_position_settings_and_incomplete_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline, candidate = (Path(directory) / name for name in ("b.tsv", "c.tsv"))
            write_run(baseline)
            for change, message in (
                (lambda case, row: row.update(fen=FENS["two"]) if case == "one" else None,
                 "position differs"),
                (lambda case, row: row.update(hash_mb="64") if case == "one" else None,
                 "mixed search settings"),
                (lambda case, row: row.update(limit_value="6"), "search settings differ"),
                (lambda case, row: row.update(case="one") if case == "two" else None,
                 "duplicate case"),
            ):
                write_run(candidate, change)
                with self.assertRaisesRegex(SystemExit, message):
                    candidate_run = compare_search.load_run(candidate)
                    compare_search.require_comparable(compare_search.load_run(baseline),
                                                      candidate_run)
            write_run(candidate)
            lines = candidate.read_text().splitlines()
            candidate.write_text("\n".join(lines[:-1]) + "\n")
            with self.assertRaisesRegex(SystemExit, "case set differs"):
                compare_search.require_comparable(compare_search.load_run(baseline),
                                                  compare_search.load_run(candidate))

    def test_rejects_repeat_mismatch_and_accepts_exact_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            base, candidate, repeat = (Path(directory) / name for name in
                                       ("b.tsv", "c.tsv", "r.tsv"))
            write_run(base)
            write_run(candidate)
            write_run(repeat, lambda case, row: row.update(score="13") if case == "two" else None)
            args = argparse.Namespace(baseline=base, candidate=candidate, repeat=repeat,
                                      exact_tree=True, details=False)
            with self.assertRaisesRegex(SystemExit, "candidate repeat"):
                compare_search.nodes(args)
            args.repeat = None
            with contextlib.redirect_stdout(io.StringIO()):
                compare_search.nodes(args)
            args.repeat = candidate
            with self.assertRaisesRegex(SystemExit, "distinct files"):
                compare_search.nodes(args)
            args.repeat = None
            write_run(candidate, lambda _, row: row.update(static_score="1"))
            with self.assertRaisesRegex(SystemExit, "exact-tree comparison"):
                compare_search.nodes(args)

    def test_balanced_timing_with_two_or_six_pairs(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for index, order in enumerate(("BC", "CB", "BC", "CB", "BC", "CB"), 1):
                baseline = Path(directory) / f"b{index}.tsv"
                candidate = Path(directory) / f"c{index}.tsv"
                write_run(baseline)
                write_run(candidate, lambda _, row, i=index:
                          row.update(total_ns=str(900 if i % 2 else 1100)))
                paths.append((order, baseline, candidate))
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                compare_search.timing(argparse.Namespace(pair=paths[:2], exact_tree=True))
            self.assertIn("median_balanced_search_time_ratio=0.994987437", output.getvalue())
            with contextlib.redirect_stdout(io.StringIO()):
                compare_search.timing(argparse.Namespace(pair=paths, exact_tree=True))
            with self.assertRaisesRegex(SystemExit, "alternate BC,CB"):
                compare_search.timing(argparse.Namespace(pair=paths[:1], exact_tree=False))
            with self.assertRaisesRegex(SystemExit, "distinct files"):
                compare_search.timing(argparse.Namespace(pair=[paths[0], ("CB", *paths[0][1:])],
                                                           exact_tree=False))


if __name__ == "__main__":
    unittest.main()
