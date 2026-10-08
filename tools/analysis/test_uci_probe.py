"""Protocol regressions using transcripts and a tiny fake UCI child."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

import chess
import chess.engine

from tools.analysis.uci_probe import IterationReports, UCI, position


START = chess.STARTING_FEN
MATE = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"
STALEMATE = "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"


def report(depth, slot, move, score="cp 20", nodes=100, extra=""):
    return (f"info depth {depth} multipv {slot} score {score} {extra} "
            f"nodes {nodes} time {nodes // 10} pv {move}")


FAKE_ENGINE = r'''
import json
import os
import sys
import time

fixture = json.load(open(sys.argv[1]))
log = open(sys.argv[2], "w", buffering=1)
open(sys.argv[2] + ".pid", "w").write(str(os.getpid()))
searches = 0

def emit(line):
    data = (line + "\n").encode()
    if fixture.get("chunks"):
        os.write(1, data[:3])
        os.write(1, data[3:])
    else:
        os.write(1, data)

for raw in sys.stdin:
    line = raw.strip()
    log.write(line + "\n")
    if line == "uci" and fixture.get("mode") != "handshake_timeout":
        emit("id name Fixture engine")
        emit("id author test")
        emit("option name Threads type spin default 1 min 1 max 8")
        emit("option name Hash type spin default 16 min 1 max 1024")
        emit("option name Ponder type check default false")
        emit("option name Path With Spaces type string default <empty>")
        emit("option name UCI_AnalyseMode type check default false")
        if fixture.get("multipv", True):
            emit("option name MultiPV type spin default 1 min 1 max 256")
        emit("uciok")
    elif line == "isready":
        emit("readyok")
    elif line.startswith("go "):
        if fixture.get("mode") == "eof":
            print("fixture child exited", file=sys.stderr, flush=True)
            sys.exit(0)
        if fixture.get("mode") == "timeout":
            continue
        if fixture.get("mode") == "flood":
            while True:
                emit("info nodes 100")
                time.sleep(0.001)
        transcripts = fixture["searches"]
        for response in transcripts[min(searches, len(transcripts) - 1)]:
            emit(response)
        searches += 1
    elif line == "quit":
        break
'''


class ReportTests(unittest.TestCase):
    def reports(self, lines, multipv=3, roots=None, bestmove="e2e4", board=None):
        reports = IterationReports(board or chess.Board(), multipv, roots)
        for line in lines:
            reports.add(line)
        return reports.finish(bestmove)

    def test_deeper_bound_keeps_complete_iteration_and_actual_work(self):
        result = self.reports([
            report(12, 1, "e2e4", nodes=100),
            report(12, 2, "d2d4", nodes=110),
            report(12, 3, "g1f3", nodes=120),
            report(13, 1, "e2e4", score="cp 148", nodes=200, extra="upperbound"),
        ])
        self.assertEqual(result["quality"], "complete_exact_iteration")
        self.assertEqual(result["complete_depth"], 12)
        self.assertEqual(result["lines"][0]["cp"], 20)
        self.assertTrue(result["partial_lines"][0]["upperbound"])
        self.assertEqual((result["actual_nodes"], result["time_ms"]), (200, 20))

    def test_repeated_depth_does_not_splice_old_slots(self):
        result = self.reports([
            report(12, 1, "e2e4"), report(12, 2, "d2d4"), report(12, 3, "g1f3"),
            report(12, 1, "c2c4"), report(12, 2, "b1c3"),
        ], bestmove="c2c4")
        self.assertEqual(result["quality"], "no_matching_complete_iteration")
        self.assertEqual(result["lines"], [])
        self.assertIsNone(result["complete_depth"])

    def test_same_depth_partial_cannot_replace_complete_score(self):
        result = self.reports([
            report(12, 1, "e2e4", score="cp 300"),
            report(12, 2, "d2d4"), report(12, 3, "g1f3"),
            report(12, 1, "e2e4", score="cp 310"),
        ])
        self.assertEqual(result["lines"][0]["cp"], 300)
        self.assertEqual(result["partial_lines"][0]["cp"], 310)

    def test_restored_primary_report_caps_completed_depth(self):
        result = self.reports([
            report(9, 1, "e2e4", score="cp 15"),
            report(10, 1, "e2e4", score="cp 100", nodes=200),
            report(9, 1, "e2e4", score="cp 15", nodes=250),
        ], multipv=1)
        self.assertEqual(result["complete_depth"], 9)
        self.assertEqual(result["lines"][0]["cp"], 15)
        self.assertEqual(result["actual_nodes"], 250)
        self.assertEqual(result["partial_lines"][0]["depth"], 10)

    def test_restoration_keeps_overwritten_bound_report(self):
        result = self.reports([
            report(12, 1, "e2e4", score="cp 31"),
            report(13, 1, "e2e4", score="cp 90", nodes=200, extra="upperbound"),
            report(12, 1, "e2e4", score="cp 31", nodes=250),
        ], multipv=1)
        self.assertEqual(result["complete_depth"], 12)
        self.assertEqual(result["final_lines"][0]["depth"], 12)
        self.assertEqual(result["partial_lines"][0]["depth"], 13)
        self.assertTrue(result["partial_lines"][0]["upperbound"])
        self.assertEqual(result["actual_nodes"], 250)

    def test_later_complete_group_replaces_same_depth(self):
        result = self.reports([
            report(12, 1, "e2e4"), report(12, 2, "d2d4"),
            report(12, 1, "c2c4", score="cp 33"), report(12, 2, "b1c3"),
        ], multipv=2, bestmove="c2c4")
        self.assertEqual(result["lines"][0]["cp"], 33)
        self.assertEqual(result["partial_lines"], [])

    def test_restored_same_depth_move_keeps_earlier_coherent_group(self):
        result = self.reports([
            report(10, 1, "e2e4", score="cp 31"), report(10, 2, "d2d4"),
            report(10, 1, "g1f3", score="cp 45"), report(10, 2, "b1c3"),
            report(10, 1, "e2e4", score="cp 31"),
        ], multipv=2)
        self.assertEqual(result["quality"], "complete_exact_iteration")
        self.assertEqual([row["pv"][0] for row in result["lines"]], ["e2e4", "d2d4"])
        self.assertEqual(result["lines"][0]["cp"], 31)
        self.assertEqual(result["partial_lines"], [])

    def test_requested_lines_are_capped_by_allowed_roots(self):
        result = self.reports([
            report(5, 1, "e2e4"), report(5, 2, "d2d4"),
        ], roots=["e2e4", "d2d4"])
        self.assertEqual(result["expected_lines"], 2)
        self.assertEqual(len(result["lines"]), 2)

    def test_duplicate_or_out_of_order_slots_do_not_complete(self):
        for lines in (
            [report(5, 1, "e2e4"), report(5, 2, "e2e4")],
            [report(5, 2, "d2d4"), report(5, 1, "e2e4")],
            [report(5, 1, "e2e4"), report(6, 2, "d2d4")],
        ):
            with self.subTest(lines=lines):
                self.assertEqual(self.reports(lines, multipv=2)["lines"], [])

    def test_scoreless_iteration_update_invalidates_group(self):
        for update in ("info depth 12 multipv 1 nodes 200 pv g1f3",
                       "info multipv 1 nodes 200"):
            with self.subTest(update=update):
                result = self.reports([
                    report(12, 1, "e2e4"), update, report(12, 2, "d2d4"),
                ], multipv=2)
                self.assertEqual(result["quality"], "no_matching_complete_iteration")
                self.assertEqual(result["lines"], [])

    def test_nodes_only_progress_preserves_group(self):
        result = self.reports([
            report(12, 1, "e2e4"), "info nodes 200 time 20", report(12, 2, "d2d4"),
        ], multipv=2)
        self.assertEqual(result["quality"], "complete_exact_iteration")
        self.assertEqual(result["actual_nodes"], 200)

    def test_mate_and_bound_scores_stay_distinct(self):
        result = self.reports([report(5, 1, "e2e4", score="mate -3")], multipv=1)
        self.assertEqual(result["lines"][0]["mate"], -3)
        self.assertNotIn("cp", result["lines"][0])
        bounded = self.reports([
            report(5, 1, "e2e4", extra="lowerbound"),
        ], multipv=1)
        self.assertEqual(bounded["lines"], [])
        self.assertTrue(bounded["final_lines"][0]["lowerbound"])

    def test_missing_actual_work_is_not_reported_as_zero(self):
        result = self.reports(["info depth 5 score cp 20 pv e2e4"], multipv=1)
        self.assertEqual(result["quality"], "complete_exact_iteration")
        self.assertIsNone(result["actual_nodes"])
        self.assertIsNone(result["time_ms"])

    def test_illegal_pv_or_bestmove_is_an_error(self):
        for lines, bestmove, roots in (
            ([report(5, 1, "e2e5")], "e2e4", None),
            ([report(5, 1, "e2e4 e7e6 e4e6")], "e2e4", None),
            ([report(5, 1, "e2e4 0000")], "e2e4", None),
            ([report(5, 1, "d2d4")], "d2d4", ["e2e4"]),
            ([], "0000", None),
            ([], "d2d4", ["e2e4"]),
        ):
            with self.subTest(lines=lines, bestmove=bestmove):
                with self.assertRaises(ValueError):
                    self.reports(lines, multipv=1, roots=roots, bestmove=bestmove)

    def test_terminal_roots_have_no_invented_score(self):
        for fen, terminal, bestmove in ((MATE, "checkmate", "(none)"),
                                       (STALEMATE, "stalemate", "0000")):
            with self.subTest(terminal=terminal):
                result = self.reports([], board=position(fen), bestmove=bestmove)
                self.assertEqual(result["quality"], "terminal")
                self.assertEqual(result["terminal"], terminal)
                self.assertEqual(result["bestmove"], "0000")
                self.assertEqual(result["expected_lines"], 0)
                self.assertEqual(result["lines"], [])

    def test_null_history_is_illegal(self):
        with self.assertRaisesRegex(ValueError, "illegal UCI move"):
            position(START, ["0000"])

class ChildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.script = self.directory / "fixture engine.py"
        self.script.write_text(FAKE_ENGINE)
        self.fixture = self.directory / "fixture.json"
        self.log = self.directory / "commands.log"

    def command(self, **fixture):
        fixture.setdefault("searches", [[report(5, 1, "e2e4"), "bestmove e2e4"]])
        self.fixture.write_text(json.dumps(fixture))
        return [sys.executable, "-u", str(self.script), str(self.fixture), str(self.log)]

    def commands(self):
        return self.log.read_text().splitlines()

    def assert_child_reaped(self):
        pid = int(Path(str(self.log) + ".pid").read_text())
        for _ in range(100):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return
            time.sleep(0.01)
        self.fail("fixture child is still alive")

    def test_history_reset_options_and_split_pipe_reads(self):
        board = position(START, ["e2e4", "e7e5"])
        transcript = [report(5, 1, "g1f3", nodes=321), "bestmove g1f3 ponder b8c6"]
        with UCI(self.command(searches=[transcript], chunks=True),
                 {"Hash": "64", "Ponder": "False", "Path With Spaces": "/tmp/a b"}) as engine:
            first = engine.analyse(board, 500, root_moves=["g1f3"])
            second = engine.analyse(board, 1000, root_moves=["g1f3"])
        self.assert_child_reaped()
        self.assertEqual(first["position"], {
            "start_fen": START, "moves": ["e2e4", "e7e5"],
            "fen": board.fen(en_passant="fen"),
        })
        self.assertEqual(first["engine"]["options"], {
            "Threads": 1, "Hash": 64, "Ponder": False, "UCI_AnalyseMode": False,
            "Path With Spaces": "/tmp/a b", "MultiPV": 1,
        })
        self.assertEqual(first["engine"]["id"]["name"], "Fixture engine")
        self.assertEqual(first["actual_nodes"], 321)
        self.assertEqual(second["request"]["nodes"], 1000)
        commands = self.commands()
        self.assertEqual(commands.count("ucinewgame"), 2)
        self.assertEqual(sum(line.startswith("position ") and
                             line.endswith("moves e2e4 e7e5") for line in commands), 2)
        self.assertIn("go nodes 500 searchmoves g1f3", commands)
        self.assertNotIn("setoption name UCI_AnalyseMode value true", commands)

    def test_invalid_request_does_not_start_search(self):
        with UCI(self.command()) as engine:
            board = chess.Board()
            for kwargs in ({"nodes": 0}, {"nodes": True}, {"nodes": 100, "multipv": 0},
                           {"nodes": 100, "root_moves": []},
                           {"nodes": 100, "root_moves": ["e2e4", "e2e4"]},
                           {"nodes": 100, "root_moves": ["e2e5"]}):
                with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                    engine.analyse(board, **kwargs)
        self.assertFalse(any(line.startswith("go ") for line in self.commands()))

    def test_unsupported_multipv_rejects_before_search(self):
        with UCI(self.command(multipv=False)) as engine:
            with self.assertRaisesRegex(ValueError, "MultiPV"):
                engine.analyse(chess.Board(), 100, multipv=2)
        self.assertFalse(any(line.startswith("go ") for line in self.commands()))

    def test_multipv_option_is_ambiguous_before_child_launch(self):
        for name in ("MultiPV", "multipv", "MULTIPV"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "multipv argument"):
                UCI(self.command(), {name: 2})
        self.assertFalse(self.log.exists())

    def test_timeout_and_eof_reap_only_the_child(self):
        for mode, error in (("timeout", TimeoutError), ("flood", TimeoutError),
                            ("eof", RuntimeError)):
            with self.subTest(mode=mode):
                engine = UCI(self.command(mode=mode), timeout_s=0.3)
                with self.assertRaises(error) as raised:
                    engine.analyse(chess.Board(), 100)
                self.assert_child_reaped()
                engine.close()
                if mode == "eof":
                    self.assertIn("fixture child exited", str(raised.exception))

    def test_handshake_timeout_and_option_errors_reap_child(self):
        for fixture, options, error in (
            ({"mode": "handshake_timeout"}, None, TimeoutError),
            ({}, {"Unknown": 1}, ValueError),
            ({}, {"Hash": 0}, chess.engine.EngineError),
            ({}, {"Ponder": "yes"}, ValueError),
            ({}, {"Ponder": True}, ValueError),
            ({}, {"Path With Spaces": "bad\ncommand"}, ValueError),
        ):
            with self.subTest(fixture=fixture, options=options):
                with self.assertRaises(error):
                    UCI(self.command(**fixture), options, timeout_s=0.3)
                self.assert_child_reaped()

    def test_bad_child_pv_closes_process(self):
        engine = UCI(self.command(searches=[[report(5, 1, "e2e5"), "bestmove e2e4"]]))
        with self.assertRaises(ValueError):
            engine.analyse(chess.Board(), 100)
        self.assert_child_reaped()

    def test_library_preserves_raw_bounds_and_restored_iteration(self):
        transcript = [report(5, 1, "e2e4", nodes=100),
                      report(6, 1, "e2e4", nodes=200, extra="upperbound"),
                      report(5, 1, "e2e4", nodes=250), "bestmove e2e4"]
        with UCI(self.command(searches=[transcript]), {"UCI_AnalyseMode": True}) as engine:
            result = engine.analyse(chess.Board(), 500)
        self.assertEqual(result["complete_depth"], 5)
        self.assertEqual(result["actual_nodes"], 250)
        self.assertTrue(result["partial_lines"][0]["upperbound"])
        self.assertTrue(result["engine"]["options"]["UCI_AnalyseMode"])

    def test_terminal_search_returns_structured_result(self):
        with UCI(self.command(searches=[["info nodes 0 time 0", "bestmove (none)"]])) as engine:
            result = engine.analyse(position(MATE), 100)
        self.assertEqual(result["format"], "uci_probe_v1")
        self.assertEqual(result["quality"], "terminal")
        self.assertEqual(result["actual_nodes"], 0)
        self.assertEqual(result["time_ms"], 0)

    def test_cli_emits_one_json_and_preserves_engine_argv(self):
        command = self.command() + ["--literal-engine-argument", "a b $(not-a-shell)"]
        probe = Path(__file__).with_name("uci_probe.py")
        run = subprocess.run([
            sys.executable, str(probe), "--fen", START, "--nodes", "500",
            "--option", "Hash=64", "--timeout", "1", "--", *command,
        ], text=True, capture_output=True, timeout=5)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(run.stderr, "")
        self.assertEqual(len(run.stdout.splitlines()), 1)
        result = json.loads(run.stdout)
        self.assertEqual(result["engine"]["argv"], command)
        self.assertEqual(result["request"], {"nodes": 500, "multipv": 1, "root_moves": None})

    def test_bad_cli_input_emits_no_result(self):
        probe = Path(__file__).with_name("uci_probe.py")
        for extra in (["--fen", "8/8/8/8/8/8/8/8 w - -"],
                      ["--fen", START, "--moves", "e2e5"],
                      ["--fen", START, "--moves", "0000"],
                      ["--fen", START, "--option", "mUlTiPv=3"],
                      ["--fen", START, "--option", "Hash"]):
            with self.subTest(extra=extra):
                run = subprocess.run([
                    sys.executable, str(probe), *extra, "--nodes", "500", "--", *self.command(),
                ], text=True, capture_output=True, timeout=5)
                self.assertEqual(run.returncode, 1)
                self.assertEqual(run.stdout, "")
                self.assertIn("uci_probe:", run.stderr)


if __name__ == "__main__":
    unittest.main()
