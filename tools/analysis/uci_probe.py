"""Probe one cold UCI search while preserving its starting FEN and move history."""

import argparse
import json
import math
import os
import select
import subprocess
import sys
import tempfile
import time

import chess


def _push_legal(board, move):
    parsed = board.parse_uci(move)
    if parsed not in board.legal_moves:
        raise ValueError(f"illegal UCI move: {move}")
    board.push(parsed)


def position(fen, moves=()):
    if len(fen.split()) != 6:
        raise ValueError("the starting FEN must contain all six fields")
    board = chess.Board(fen)
    if not board.is_valid():
        raise ValueError("invalid starting position")
    for move in moves:
        _push_legal(board, move)
    return board


def info(line):
    tokens = line.split()
    result = {"multipv": 1, "pv": [], "lowerbound": False, "upperbound": False}
    index = 1
    while index < len(tokens):
        key = tokens[index]
        if key == "string":
            break
        if key == "pv":
            result["pv"] = tokens[index + 1:]
            break
        if key in ("lowerbound", "upperbound"):
            result[key] = True
        elif key == "score" and index + 2 < len(tokens):
            kind = tokens[index + 1]
            if kind in ("cp", "mate"):
                result[kind] = int(tokens[index + 2])
            index += 2
        elif key in ("depth", "multipv", "nodes", "time") and index + 1 < len(tokens):
            value = int(tokens[index + 1])
            if value < (1 if key == "multipv" else 0):
                raise ValueError(f"invalid UCI {key}: {value}")
            result["time_ms" if key == "time" else key] = value
            index += 1
        index += 1
    return result


class Reports:
    """Complete report groups; never splice slots from repeated depth reports."""

    def __init__(self, board, multipv, root_moves):
        self.board = board
        self.allowed = set(root_moves) if root_moves is not None else {
            move.uci() for move in board.legal_moves
        }
        self.expected = min(multipv, len(self.allowed))
        self.final = {}
        self.scored_rows = []
        self.complete = []
        self.group = []
        self.nodes = None
        self.time_ms = None

    def add(self, line):
        row = info(line)
        if "nodes" in row:
            self.nodes = row["nodes"] if self.nodes is None else max(self.nodes, row["nodes"])
        if "time_ms" in row:
            self.time_ms = row["time_ms"] if self.time_ms is None else max(self.time_ms, row["time_ms"])
        if "cp" not in row and "mate" not in row:
            fields = line.partition(" string ")[0].split()
            if "depth" in row or row["pv"] or "multipv" in fields:
                self.group = []
            return
        if row["pv"]:
            if row["pv"][0] not in self.allowed:
                raise ValueError("engine PV violates the legal root moves")
            board = self.board.copy(stack=False)
            for move in row["pv"]:
                _push_legal(board, move)
        slot = row["multipv"]
        self.scored_rows.append(row)
        self.final[slot] = row
        if slot == 1:
            self.group = []
        if ("depth" not in row or not row["pv"] or row["lowerbound"] or row["upperbound"]
                or slot != len(self.group) + 1
                or (self.group and row["depth"] != self.group[0]["depth"])):
            self.group = []
            return
        self.group.append(row)
        if len(self.group) == self.expected:
            if len({item["pv"][0] for item in self.group}) == self.expected:
                self.complete.append(list(self.group))
            self.group = []

    def finish(self, bestmove):
        terminal = not self.board.legal_moves.count()
        if terminal:
            if bestmove not in ("0000", "(none)"):
                raise ValueError("engine returned a move for a terminal position")
            bestmove = "0000"
        elif bestmove not in self.allowed:
            raise ValueError("engine bestmove violates the legal root moves")
        chosen = []
        final_depth = self.final.get(1, {}).get("depth")
        completed = sorted(reversed(self.complete), key=lambda rows: rows[0]["depth"], reverse=True)
        for candidate in completed:
            # Latrunculi's final report restores the last accepted iteration.
            if final_depth is not None and candidate[0]["depth"] > final_depth:
                continue
            if candidate[0]["pv"][:1] == [bestmove]:
                chosen = candidate
                break
        final = [self.final[key] for key in sorted(self.final)]
        selected = {row["multipv"]: row for row in chosen}
        # Keep overwritten aborted reports, without relabeling old complete groups.
        completed_rows = {
            id(row) for group in self.complete for row in group
            if final_depth is None or group[0]["depth"] <= final_depth
        }
        return {
            "bestmove": bestmove,
            "quality": "terminal" if terminal else (
                "complete_exact_iteration" if chosen else "no_matching_complete_iteration"),
            "terminal": ("checkmate" if self.board.is_check() else "stalemate") if terminal else None,
            "complete_depth": chosen[0]["depth"] if chosen else None,
            "expected_lines": self.expected,
            "lines": chosen,
            "final_lines": final,
            "partial_lines": [row for row in self.scored_rows
                              if id(row) not in completed_rows
                              and row != selected.get(row["multipv"])],
            "actual_nodes": self.nodes,
            "time_ms": self.time_ms,
        }


class UCI:
    def __init__(self, command, options=None, timeout_s=90):
        if isinstance(command, (str, bytes)) or not command or not all(
                isinstance(arg, str) and arg and "\0" not in arg for arg in command):
            raise ValueError("engine command must be a nonempty argv sequence")
        if not math.isfinite(timeout_s) or timeout_s <= 0:
            raise ValueError("timeout must be positive and finite")
        if any(name.lower() == "multipv" for name in (options or {})):
            raise ValueError("set MultiPV through the analyse multipv argument")
        self.command = list(command)
        self.timeout_s = timeout_s
        self.options = {}
        self.configured_options = {}
        self.identity = {}
        self._buffer = b""
        self._process = None
        self._stderr = tempfile.TemporaryFile()
        try:
            self._process = subprocess.Popen(
                self.command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=self._stderr, bufsize=0,
            )
            os.set_blocking(self._process.stdout.fileno(), False)
            self._send("uci")
            deadline = time.monotonic() + self.timeout_s
            while (line := self._line(deadline)) != "uciok":
                if line.startswith("option name "):
                    name, _, definition = line[12:].partition(" type ")
                    self.options[name] = definition
                elif line.startswith("id "):
                    _, key, value = line.split(" ", 2)
                    self.identity[key] = value
            for name, value in {"Threads": 1, "Hash": 32, **(options or {})}.items():
                self._set_option(name, value)
            self._ready()
        except BaseException:
            self.close()
            raise

    def _send(self, command):
        self._process.stdin.write((command + "\n").encode())
        self._process.stdin.flush()

    def _set_option(self, name, value):
        if name not in self.options:
            raise ValueError(f"engine does not support option {name!r}")
        value = str(value).lower() if isinstance(value, bool) else str(value)
        if any(char in name + value for char in "\r\n\0"):
            raise ValueError("option names and values must fit one UCI command")
        definition = self.options[name].split()
        if definition[0] == "spin":
            number = int(value)
            lower = int(definition[definition.index("min") + 1])
            upper = int(definition[definition.index("max") + 1])
            if not lower <= number <= upper:
                raise ValueError(f"option {name!r} must be between {lower} and {upper}")
            value = number
        elif definition[0] == "check":
            if value not in ("true", "false"):
                raise ValueError(f"option {name!r} must be true or false")
        self._send(f"setoption name {name} value {value}")
        self.configured_options[name] = value

    def _line(self, deadline):
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"UCI engine exceeded {self.timeout_s:g}s response limit")
            if b"\n" in self._buffer:
                line, self._buffer = self._buffer.split(b"\n", 1)
                return line.decode("utf-8", errors="replace").strip()
            if not select.select([self._process.stdout], [], [], remaining)[0]:
                continue
            chunk = os.read(self._process.stdout.fileno(), 65536)
            if not chunk:
                self._stderr.seek(0, os.SEEK_END)
                self._stderr.seek(max(0, self._stderr.tell() - 4096))
                detail = self._stderr.read().decode(errors="replace")
                raise RuntimeError(f"UCI engine closed stdout: {detail}")
            self._buffer += chunk

    def _ready(self):
        self._send("isready")
        deadline = time.monotonic() + self.timeout_s
        while self._line(deadline) != "readyok":
            pass

    def analyse(self, board, nodes, multipv=1, root_moves=None):
        if type(nodes) is not int or nodes <= 0 or type(multipv) is not int or multipv <= 0:
            raise ValueError("nodes and multipv must be positive integers")
        if board.chess960:
            raise ValueError("the probe supports standard chess")
        start_fen = board.root().fen(en_passant="fen")
        moves = [move.uci() for move in board.move_stack]
        replayed = position(start_fen, moves)
        if replayed.fen(en_passant="fen") != board.fen(en_passant="fen"):
            raise ValueError("board does not match its starting position and history")
        if multipv > 1 and "MultiPV" not in self.options:
            raise ValueError("engine does not support MultiPV")
        if root_moves is not None:
            root_moves = list(root_moves)
            legal = {move.uci() for move in board.legal_moves}
            if not root_moves or any(move not in legal for move in root_moves):
                raise ValueError("root_moves must contain legal UCI moves")
            if len(root_moves) != len(set(root_moves)):
                raise ValueError("root_moves must be distinct")
        reports = Reports(board, multipv, root_moves)
        try:
            self._send("ucinewgame")
            if "MultiPV" in self.options:
                self._set_option("MultiPV", multipv)
            self._ready()
            command = "position fen " + start_fen
            if moves:
                command += " moves " + " ".join(moves)
            self._send(command)
            go = f"go nodes {nodes}"
            if root_moves is not None:
                go += " searchmoves " + " ".join(root_moves)
            started = time.monotonic()
            self._send(go)
            deadline = started + self.timeout_s
            while True:
                line = self._line(deadline)
                if line.startswith("bestmove "):
                    result = reports.finish(line.split()[1])
                    result.update(
                        format="uci_probe_v1",
                        engine={"argv": self.command, "id": dict(self.identity),
                                "options": dict(self.configured_options)},
                        position={"start_fen": start_fen, "moves": moves,
                                  "fen": board.fen(en_passant="fen")},
                        request={"nodes": nodes, "multipv": multipv, "root_moves": root_moves},
                    )
                    return result
                if line.startswith("info "):
                    reports.add(line)
        except BaseException:
            self.close()
            raise

    def close(self):
        if self._process is not None:
            if self._process.poll() is None:
                try:
                    self._send("stop")
                    self._send("quit")
                    self._process.wait(timeout=2)
                except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                    self._process.terminate()
                    try:
                        self._process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        self._process.kill()
                        self._process.wait(timeout=2)
            self._process.stdin.close()
            self._process.stdout.close()
        self._stderr.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    separator = argv.index("--") if "--" in argv else len(argv)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fen", required=True, help="six-field starting FEN")
    parser.add_argument("--moves", nargs="*", default=[], help="UCI moves from the starting FEN")
    parser.add_argument("--nodes", required=True, type=int)
    parser.add_argument("--multipv", type=int, default=1)
    parser.add_argument("--root-moves", nargs="+")
    parser.add_argument("--option", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args(argv[:separator])
    command = argv[separator + 1:]
    if not command:
        parser.error("provide the engine argv after --")
    try:
        options = {}
        for option in args.option:
            name, equal, value = option.partition("=")
            if not equal or not name:
                raise ValueError("options must use NAME=VALUE")
            if name.lower() == "multipv":
                raise ValueError("set MultiPV through --multipv")
            options[name] = value
        board = position(args.fen, args.moves)
        with UCI(command, options, args.timeout) as engine:
            result = engine.analyse(board, args.nodes, args.multipv, args.root_moves)
        print(json.dumps(result, sort_keys=True))
        return 0
    except (ValueError, OSError, RuntimeError, TimeoutError) as error:
        print(f"uci_probe: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
