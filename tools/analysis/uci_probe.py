"""Probe one cold UCI search while preserving its starting FEN and move history."""

import argparse
import asyncio
import json
import math
import sys
import tempfile

import chess
import chess.engine


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


def parse_info(line):
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


class IterationReports:
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
        row = parse_info(line)
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


class TranscriptProtocol(chess.engine.UciProtocol):
    """Observe raw reports before the library aggregates or parses their scores."""

    def __init__(self):
        super().__init__()
        self.lines = []

    def line_received(self, line):
        if line.startswith(("info ", "bestmove ")):
            self.lines.append(line)

    async def initialize(self):
        try:
            await super().initialize()
        except BaseException:
            # SimpleEngine cannot expose its process when initialization fails.
            # Reap it before its startup event loop closes.
            self.transport.close()
            await asyncio.shield(self.returncode)
            raise


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
        self._engine = None
        self._stderr = tempfile.TemporaryFile()
        try:
            self._engine = chess.engine.SimpleEngine.popen(
                TranscriptProtocol, self.command, timeout=timeout_s, stderr=self._stderr)
            self.options = self._engine.options
            self.identity = self._engine.id
            self.configured_options = self._configure_options(options or {})
        except BaseException:
            self.close(force=True)
            raise

    def _configure_options(self, overrides):
        """Apply supported overrides and return the effective engine options."""
        settings = {"Threads": 1, "Hash": 32}
        # python-chess otherwise enables analysis mode itself.
        if "UCI_AnalyseMode" in self.options:
            settings["UCI_AnalyseMode"] = self.options["UCI_AnalyseMode"].default
        settings.update(overrides)
        configured = {}
        managed = {"ponder": False, "uci_chess960": False, "uci_variant": "chess"}
        for name, value in settings.items():
            if name not in self.options:
                raise ValueError(f"engine does not support option {name!r}")
            if any(char in name + str(value) for char in "\r\n\0"):
                raise ValueError("option names and values must fit one UCI command")
            option = self.options[name]
            if option.type == "check":
                value = str(value).lower()
                if value not in ("true", "false"):
                    raise ValueError(f"option {name!r} must be true or false")
            parsed = option.parse(value)
            if name.lower() in managed and parsed != managed[name.lower()]:
                raise ValueError(f"option {name!r} conflicts with standard non-pondering analysis")
            configured[option.name] = parsed
        self._engine.configure({name: value for name, value in configured.items()
                                if name.lower() not in managed})
        for name, value in managed.items():
            if name in self.options:
                configured[self.options[name].name] = value
        return configured

    def analyse(self, board, nodes, multipv=1, root_moves=None):
        if self._engine is None:
            raise RuntimeError("UCI engine is closed")
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
        protocol = self._engine.protocol
        protocol.lines = []
        try:
            if "MultiPV" in self.options:
                self.options["MultiPV"].parse(multipv)
                self.configured_options["MultiPV"] = multipv
            # A fresh game token resets engine state for every probe. Bound the
            # caller's wait: SimpleEngine's node-only searches have no deadline.
            future = asyncio.run_coroutine_threadsafe(protocol.analyse(
                board, chess.engine.Limit(nodes=nodes), multipv=multipv, game=object(),
                info=chess.engine.INFO_NONE,
                root_moves=None if root_moves is None else map(chess.Move.from_uci, root_moves),
            ), protocol.loop)
            future.result(timeout=self.timeout_s)
            reports = IterationReports(board, multipv, root_moves)
            bestmove = ""
            for line in protocol.lines:
                if line.startswith("info "):
                    reports.add(line)
                else:
                    bestmove = line.split()[1]
            result = reports.finish(bestmove)
            result.update(
                format="uci_probe_v1",
                engine={"argv": self.command, "id": dict(self.identity),
                        "options": dict(self.configured_options)},
                position={"start_fen": start_fen, "moves": moves,
                          "fen": board.fen(en_passant="fen")},
                request={"nodes": nodes, "multipv": multipv, "root_moves": root_moves},
            )
            return result
        except chess.engine.EngineTerminatedError as error:
            self._stderr.seek(0, 2)
            self._stderr.seek(max(0, self._stderr.tell() - 4096))
            detail = self._stderr.read().decode(errors="replace")
            self.close(force=True)
            raise RuntimeError(f"UCI engine exited: {error}; {detail}") from error
        except TimeoutError as error:
            self.close(force=True)
            raise TimeoutError(f"UCI engine exceeded {self.timeout_s:g}s response limit") from error
        except BaseException:
            self.close(force=True)
            raise

    def close(self, *, force=False):
        engine, self._engine = self._engine, None
        try:
            if engine is not None:
                try:
                    if not force and not engine.returncode.done():
                        future = asyncio.run_coroutine_threadsafe(engine.protocol.quit(),
                                                                 engine.protocol.loop)
                        future.result(timeout=2)
                except (TimeoutError, chess.engine.EngineError):
                    pass
                finally:
                    # Do not cancel a pending analysis: termination completes its
                    # result, avoiding cancelled-future errors in python-chess.
                    engine.close()
                    engine.returncode.result(timeout=2)
        finally:
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
    except (ValueError, OSError, RuntimeError, TimeoutError, chess.engine.EngineError) as error:
        print(f"uci_probe: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
