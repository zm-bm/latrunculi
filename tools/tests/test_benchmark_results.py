import csv
import tempfile
import unittest
from pathlib import Path

from tools import benchmark


FIELDS = ("result_format", "case", "fen",
          *benchmark.CONFIG, *benchmark.SIGNATURE, "total_ns")
FENS = {
    "one": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "two": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1",
}


def write_run(path, change=None):
    with path.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        for case, fen in FENS.items():
            row = dict(result_format=benchmark.FORMAT, case=case, fen=fen,
                       limit_type="depth",
                       limit_value="5", threads="1", hash_mb="32",
                       completed_depth="5", static_score="0", score="12",
                       nodes="100", best_move="e2e4" if case == "one" else "e7e5",
                       pv="e2e4 e7e5" if case == "one" else "e7e5 e2e4",
                       total_ns="1000")
            if change:
                change(case, row)
            writer.writerow(row)


class BenchmarkResultsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def read(self, name, change=None):
        path = self.root / name
        write_run(path, change)
        return benchmark.load_run(path)

    def test_legality_checks_moves_after_the_root(self):
        self.read("legal.tsv")
        with self.assertRaisesRegex(ValueError, "illegal PV move e4e6 at ply 3"):
            self.read("illegal.tsv", lambda case, row:
                      row.update(pv="e2e4 e7e6 e4e6") if case == "one" else None)

    def test_summary_reports_work_and_time_changes(self):
        pairs = []
        for index, order in enumerate(("BC", "CB")):
            baseline = self.read(f"b{index}.tsv")
            candidate = self.read(f"c{index}.tsv", lambda _, row:
                                  row.update(nodes="90", score="13", total_ns="1100"))
            pairs.append((order, baseline, candidate))
        summary = benchmark.comparison_summary(pairs)
        self.assertAlmostEqual(summary["geometric_mean_node_ratio"], 0.9)
        self.assertEqual(summary["score_changes"], 2)
        self.assertEqual(summary["root_move_changes"], 0)
        self.assertEqual(summary["limit_value"], "5")
        self.assertEqual(summary["timing"], "paired")
        self.assertEqual(benchmark.format_overview(summary),
                         "Candidate search time: +10.00% (median balanced ratio)\n"
                         "Candidate total nodes: -10.00%\n"
                         "Changed best moves: 0 / 2\n"
                         "Repeatability and PV legality checks: passed\n")

    def test_rejects_changed_positions_settings_and_incomplete_files(self):
        baseline = self.read("b.tsv")
        for change, message in (
            (lambda case, row: row.update(fen=row["fen"].replace(" 0 1", " 1 1")),
             "position differs"),
            (lambda _, row: row.update(limit_value="6"), "search settings differ"),
        ):
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                candidate = self.read("c.tsv", change)
                benchmark.require_comparable(baseline, candidate)
        candidate = self.read("c.tsv")
        lines = candidate.path.read_text().splitlines()
        candidate.path.write_text("\n".join(lines[:-1]) + "\n")
        with self.assertRaisesRegex(ValueError, "case set differs"):
            benchmark.require_comparable(baseline, benchmark.load_run(candidate.path))

    def test_balanced_timing_preserves_statistics(self):
        pairs = []
        for index, order in enumerate(("BC", "CB") * 3):
            baseline = self.read(f"b{index}.tsv")
            candidate = self.read(f"c{index}.tsv", lambda _, row, i=index:
                                  row.update(total_ns=str(900 if i % 2 == 0 else 1100)))
            pairs.append((order, baseline, candidate))
        for count in (2, 6):
            summary = benchmark.comparison_summary(pairs[:count])
            self.assertAlmostEqual(summary["median_balanced_search_time_ratio"], 0.994987437)
            self.assertEqual(summary["minimum_search_time_ratio"], 0.9)
            self.assertEqual(summary["maximum_search_time_ratio"], 1.1)
            self.assertEqual(benchmark.format_overview(summary),
                             "Candidate search time: -0.50% (median balanced ratio)\n"
                             "Candidate total nodes: +0.00%\n"
                             "Changed best moves: 0 / 2\n"
                             "Repeatability and PV legality checks: passed\n")


if __name__ == "__main__":
    unittest.main()
