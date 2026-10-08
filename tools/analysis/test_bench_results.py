import csv
import tempfile
import unittest
from pathlib import Path

from tools.analysis import bench_results


FIELDS = ("result_format", "case", "fen",
          *bench_results.CONFIG, *bench_results.SIGNATURE, "total_ns")
FENS = {
    "one": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "two": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1",
}


def write_run(path, change=None):
    with path.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        for case, fen in FENS.items():
            row = dict(result_format=bench_results.FORMAT, case=case, fen=fen,
                       limit_type="depth",
                       limit_value="5", threads="1", hash_mb="32",
                       completed_depth="5", static_score="0", score="12",
                       nodes="100", best_move="e2e4" if case == "one" else "e7e5",
                       pv="e2e4 e7e5" if case == "one" else "e7e5 e2e4",
                       total_ns="1000")
            if change:
                change(case, row)
            writer.writerow(row)


class BenchResultsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def read(self, name, change=None):
        path = self.root / name
        write_run(path, change)
        return bench_results.load_run(path)

    def test_legality_checks_moves_after_the_root(self):
        run = self.read("legal.tsv")
        self.assertEqual(bench_results.require_legal_pvs(run), 4)
        with self.assertRaisesRegex(ValueError, "illegal PV move e4e6 at ply 3"):
            self.read("illegal.tsv", lambda case, row:
                      row.update(pv="e2e4 e7e6 e4e6") if case == "one" else None)

    def test_single_pair_reports_work_and_labels_time_as_diagnostic(self):
        baseline = self.read("b.tsv")
        candidate = self.read("c.tsv", lambda _, row: row.update(nodes="90", score="13"))
        for order in ("BC", "CB"):
            with self.subTest(order=order):
                summary = bench_results.summarize([(order, baseline, candidate)])
                self.assertAlmostEqual(summary["geometric_mean_node_ratio"], 0.9)
                self.assertEqual(summary["score_changes"], 2)
                self.assertEqual(summary["root_move_changes"], 0)
                self.assertEqual(summary["limit_value"], "5")
                self.assertEqual(summary["pair_1_order"], order)
                self.assertEqual(summary["timing"], "single_pair_diagnostic")
                self.assertNotIn("median_balanced_search_time_ratio", summary)
        with self.assertRaisesRegex(ValueError, "exact-tree comparison"):
            bench_results.summarize([("BC", baseline, candidate)], exact_tree=True)

    def test_numeric_errors_identify_the_field_and_requirement(self):
        for field, value, requirement in (
            ("nodes", "many", "an integer"),
            ("hash_mb", "1.5", "an integer"),
            ("score", "unknown", "an integer"),
            ("nodes", "0", "positive"),
            ("total_ns", "-1", "positive"),
        ):
            with self.subTest(field=field, value=value), self.assertRaisesRegex(
                ValueError, rf"numeric\.tsv:2: {field} must be {requirement}"
            ):
                self.read("numeric.tsv", lambda _, row: row.update({field: value}))

    def test_signed_scores_preserve_the_stored_representation(self):
        run = self.read("signed.tsv", lambda _, row: row.update(static_score="-23", score="+12"))
        self.assertEqual(run.rows["one"]["static_score"], "-23")
        self.assertEqual(run.rows["one"]["score"], "+12")

    def test_rejects_changed_positions_settings_and_incomplete_files(self):
        baseline = self.read("b.tsv")
        for change, message in (
            (lambda case, row: row.update(fen=row["fen"].replace(" 0 1", " 1 1")),
             "position differs"),
            (lambda case, row: row.update(hash_mb="64") if case == "one" else None,
             "mixed search settings"),
            (lambda _, row: row.update(limit_value="6"), "search settings differ"),
            (lambda case, row: row.update(case="one") if case == "two" else None,
             "duplicate case"),
            (lambda _, row: row.update(result_format="old_format"), "expected search_measurement"),
        ):
            with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                candidate = self.read("c.tsv", change)
                bench_results.summarize([("BC", baseline, candidate)])
        candidate = self.read("c.tsv")
        lines = candidate.path.read_text().splitlines()
        candidate.path.write_text("\n".join(lines[:-1]) + "\n")
        with self.assertRaisesRegex(ValueError, "case set differs"):
            bench_results.summarize([("BC", baseline, bench_results.load_run(candidate.path))])

    def test_repeat_mismatches_are_rejected_even_for_tree_changes(self):
        baseline = self.read("b.tsv")
        candidate = self.read("c.tsv", lambda _, row: row.update(score="13"))
        baseline_repeat = self.read("b2.tsv")
        candidate_repeat = self.read("c2.tsv", lambda _, row: row.update(score="14"))
        with self.assertRaisesRegex(ValueError, "candidate repeat 2"):
            bench_results.summarize([("BC", baseline, candidate),
                               ("CB", baseline_repeat, candidate_repeat)])

    def test_balanced_timing_preserves_statistics_and_rejects_bad_schedules(self):
        pairs = []
        for index, order in enumerate(("BC", "CB") * 3):
            baseline = self.read(f"b{index}.tsv")
            candidate = self.read(f"c{index}.tsv", lambda _, row, i=index:
                                  row.update(total_ns=str(900 if i % 2 == 0 else 1100)))
            pairs.append((order, baseline, candidate))
        for count in (2, 6):
            summary = bench_results.summarize(pairs[:count], exact_tree=True)
            self.assertAlmostEqual(summary["median_balanced_search_time_ratio"], 0.994987437)
            self.assertEqual(summary["minimum_search_time_ratio"], 0.9)
            self.assertEqual(summary["maximum_search_time_ratio"], 1.1)
        for invalid in (pairs[:3], [pairs[1], pairs[0]], [pairs[0], pairs[2]],
                        [("XX", *pairs[0][1:])], []):
            with self.assertRaisesRegex(ValueError, "alternate BC,CB"):
                bench_results.summarize(invalid)
        with self.assertRaisesRegex(ValueError, "distinct files"):
            bench_results.summarize([pairs[0], ("CB", *pairs[0][1:])])


if __name__ == "__main__":
    unittest.main()
