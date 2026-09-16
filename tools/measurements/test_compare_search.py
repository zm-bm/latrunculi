import argparse
import contextlib
import csv
import io
import tempfile
import unittest
from pathlib import Path

try:
    from . import compare_search
except ImportError:
    import compare_search


FIELDS = (
    "result_format",
    "case",
    *compare_search.PROFILE,
    "score",
    "nodes",
    "total_ns",
    "best_move",
    "pv",
)
CASES = sorted(compare_search.current_suite()[0])


def write_run(path: Path, change=None) -> None:
    with path.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        for index, case in enumerate(CASES, start=1):
            row = {
                "result_format": compare_search.FORMAT,
                "case": case,
                **compare_search.PROFILE,
                "score": "10",
                "nodes": str(1000 + index),
                "total_ns": str(1_000_000 + index),
                "best_move": "e2e4",
                "pv": "e2e4 e7e5",
            }
            if change is not None:
                change(index, row)
            writer.writerow(row)


def timing_panel(directory: str, candidate_change=None):
    pairs = []
    for pair, order in enumerate(("BC", "CB", "BC", "CB", "BC", "CB"), start=1):
        baseline = Path(directory) / f"baseline-{pair}.tsv"
        candidate = Path(directory) / f"candidate-{pair}.tsv"
        write_run(baseline)
        write_run(
            candidate,
            lambda index, row, pair=pair: candidate_change(pair, index, row)
            if candidate_change is not None
            else row.update(total_ns=str(int(row["total_ns"]) * 9 // 10)),
        )
        pairs.append((order, baseline, candidate))
    return pairs


class ProfileValidationTest(unittest.TestCase):
    def test_rejects_wrong_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "depth-5.tsv"
            write_run(
                path,
                lambda _index, row: row.update(limit_value="5", completed_depth="5"),
            )
            with self.assertRaisesRegex(SystemExit, "invalid strength profile"):
                compare_search.load_run(path, set(CASES))

    def test_rejects_incomplete_duplicate_and_malformed_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            incomplete = Path(directory) / "incomplete.tsv"
            write_run(incomplete)
            lines = incomplete.read_text(encoding="utf-8").splitlines()
            incomplete.write_text("\n".join(lines[:-1]) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(SystemExit, "not the current 200-case"):
                compare_search.load_run(incomplete, set(CASES))

            duplicate = Path(directory) / "duplicate.tsv"
            write_run(
                duplicate,
                lambda index, row: row.update(case=CASES[0]) if index == 200 else None,
            )
            with self.assertRaisesRegex(SystemExit, "duplicate case"):
                compare_search.load_run(duplicate, set(CASES))

            malformed = Path(directory) / "malformed.tsv"
            write_run(
                malformed,
                lambda index, row: row.update(nodes="bad") if index == 1 else None,
            )
            with self.assertRaisesRegex(SystemExit, "malformed"):
                compare_search.load_run(malformed, set(CASES))


class ComparisonTest(unittest.TestCase):
    def test_node_summary_reports_core_and_optional_details(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline = Path(directory) / "baseline.tsv"
            candidate = Path(directory) / "candidate.tsv"
            repeat = Path(directory) / "candidate-repeat.tsv"
            write_run(baseline)

            def change(index, row):
                row.update(nodes=str(int(row["nodes"]) * 9 // 10))
                if index == 1:
                    row.update(score="20", best_move="d2d4", pv="d2d4 d7d5")

            write_run(
                candidate,
                change,
            )
            write_run(repeat, change)
            args = argparse.Namespace(
                baseline=baseline,
                candidate=candidate,
                repeat=repeat,
                exact_tree=False,
                details=True,
            )
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                compare_search.print_node_summary(args)
            result = output.getvalue()
            self.assertIn("geometric_mean_node_ratio=", result)
            self.assertIn("total_node_ratio=", result)
            self.assertIn("improved=200", result)
            self.assertIn("root_move_changes=1", result)
            self.assertIn("score_changes=1", result)
            self.assertIn("largest_node_regressions=", result)

    def test_rejects_repeat_and_exact_tree_mismatches(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline = Path(directory) / "baseline.tsv"
            candidate = Path(directory) / "candidate.tsv"
            repeat = Path(directory) / "repeat.tsv"
            write_run(baseline)
            write_run(candidate)
            write_run(
                repeat,
                lambda index, row: row.update(nodes="9999") if index == 1 else None,
            )
            repeat_args = argparse.Namespace(
                baseline=baseline,
                candidate=candidate,
                repeat=repeat,
                exact_tree=False,
                details=False,
            )
            with self.assertRaisesRegex(SystemExit, "invalid candidate repeat"):
                compare_search.print_node_summary(repeat_args)

            exact_args = argparse.Namespace(**vars(repeat_args))
            exact_args.repeat = None
            exact_args.exact_tree = True
            exact_args.candidate = repeat
            with self.assertRaisesRegex(SystemExit, "invalid exact-tree comparison"):
                compare_search.print_node_summary(exact_args)

    def test_timing_panel_reports_balanced_medians(self):
        with tempfile.TemporaryDirectory() as directory:
            args = argparse.Namespace(pair=timing_panel(directory), exact_tree=True)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                compare_search.print_timing_summary(args)
            result = output.getvalue()
            self.assertIn("pairs=6", result)
            self.assertIn("candidate_wins=6", result)
            self.assertIn("bc_median_search_time_ratio=", result)
            self.assertIn("cb_median_search_time_ratio=", result)

    def test_tree_changing_timing_allows_baseline_candidate_signature_differences(self):
        with tempfile.TemporaryDirectory() as directory:
            def change(_pair, index, row):
                row.update(total_ns=str(int(row["total_ns"]) * 9 // 10))
                if index == 1:
                    row.update(score="20", best_move="d2d4", pv="d2d4 d7d5")

            args = argparse.Namespace(
                pair=timing_panel(directory, change),
                exact_tree=False,
            )
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                compare_search.print_timing_summary(args)
            self.assertIn("pairs=6", output.getvalue())

    def test_timing_panel_rejects_candidate_repeat_nondeterminism(self):
        with tempfile.TemporaryDirectory() as directory:
            def change(pair, index, row):
                row.update(total_ns=str(int(row["total_ns"]) * 9 // 10))
                if pair == 6 and index == 1:
                    row.update(nodes="9999")

            args = argparse.Namespace(
                pair=timing_panel(directory, change),
                exact_tree=False,
            )
            with self.assertRaisesRegex(SystemExit, "invalid candidate timing repeat"):
                compare_search.print_timing_summary(args)

    def test_timing_panel_rejects_reused_paths_and_wrong_order(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline = Path(directory) / "baseline.tsv"
            candidate = Path(directory) / "candidate.tsv"
            write_run(baseline)
            write_run(candidate)
            reused = argparse.Namespace(
                pair=[
                    (order, baseline, candidate)
                    for order in ("BC", "CB", "BC", "CB", "BC", "CB")
                ],
                exact_tree=False,
            )
            with self.assertRaisesRegex(SystemExit, "twelve distinct paths"):
                compare_search.print_timing_summary(reused)

            pairs = timing_panel(directory)
            pairs[1] = ("BC", pairs[1][1], pairs[1][2])
            wrong_order = argparse.Namespace(pair=pairs, exact_tree=False)
            with self.assertRaisesRegex(SystemExit, "declared timing order"):
                compare_search.print_timing_summary(wrong_order)


if __name__ == "__main__":
    unittest.main()
