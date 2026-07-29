# Engine Roadmap

This document is a subsystem-oriented roadmap for the engine. Each section
describes the current implementation and important boundaries; mature sections
may also record practical improvements to consider next.

## Engine / UCI

The engine boundary is intentionally small: `Engine` coordinates command
handling, board/history state, UCI options, and asynchronous search lifecycle.
`uci::Reader` reads one line at a time from its configured input stream and
delegates parsing to `parse_command()`; `uci::Writer` owns UCI and debug-output
formatting and flushing; `uci::Options` owns option parsing and validation while
`Engine` applies option side effects. `ThreadPool` starts workers from the
resolved root board and `SearchLimits`; each `SearchWorker` owns an independent
Board copy retaining the reconstructed game history and subsequent search
traversal.

Current UCI support covers the core loop: `uci`, `debug`, `isready`,
`setoption`, `ucinewgame`, `position`, `go`, `stop`, and `quit`. `go` applies
`depth`, `movetime`, `nodes`, `wtime`/`btime`, `winc`/`binc`, and `movestogo`.
The parser also records `ponder`, `infinite`, `mate`, `searchmoves`, and unknown
`go` tokens, but `Engine` does not yet apply them. `ponderhit`, `register`, and
unknown commands are accepted as silent compatibility no-ops. `ucinewgame`
clears the transposition table; `position` rebuilds the board and game history.

Advertised options are `Hash`, `Clear Hash`, `Threads`, and `Debug`. Search
output includes `info depth`, `score cp`/`score mate`, nodes, time, nps, and a
PV only while the complete line remains legal from the root. Positions without
a legal move produce `bestmove 0000`.

The same command loop also accepts local debug-console extensions: `help`,
`board`/`d`, `eval`, `move`, `moves`, and `perft`, plus `exit` as a local quit
alias. These are local inspection tools, not protocol features.

### Potential Improvements

Future UCI work should focus on common GUI compatibility rather than broad
protocol surface area. The likely next protocol gaps are real ponder support,
applying parsed `go searchmoves`, `go infinite`, and `go mate` limits, MultiPV,
lowerbound/upperbound score reporting, richer progress fields such as
`currmove`, `currmovenumber`, `hashfull`, `tbhits`, and `cpuload`, and Chess960
support if the board/search layer grows that capability.

Likely future options include `SyzygyPath`, `SyzygyProbeDepth`,
`Syzygy50MoveRule`, `UCI_Chess960`, `MultiPV`, and optional strength controls
such as `UCI_LimitStrength` and `UCI_Elo`. Treat these as compatibility targets,
not commitments to add unsupported engine features prematurely.

## Board

`Board` is the mutable position type used by move generation, evaluation,
search, perft, and UCI tooling. It owns the durable board representation and a
dynamically growable stack of per-ply states. Board copies are independent and
reserve enough additional state capacity for a complete search.

The durable representation combines per-color piece and occupancy bitboards, a
square mailbox, piece counts, king locations, side to move, game and
incremental material and piece-square scores.
The active `PlyState` holds the Zobrist key, castling rights, raw and legal
en-passant targets, halfmove clock, cached checkers and slider blockers, and
undo data. `make()`/`unmake()` maintain these synchronized views incrementally;
null transitions preserve piece placement while updating side to move,
reversible state, and tactical caches. Repetition detection scans the keys in
the owned state stack. Board calculations reuse attack and move-geometry
primitives from `core`.

`board.hpp` is the module map and retains hot query and representation-mutation
definitions inline. Implementation files separate representation and copying,
FEN I/O, make/unmake, Board rules, static exchange evaluation, and notation.
FEN parsing, castling rights, per-ply state, and immutable Zobrist tables remain
narrow support components.

## Move Ordering

Move ordering is currently a solid staged baseline. The engine uses one
`move_picker::Picker` for main search and qsearch; mode-specific factories
configure the staged picker, while `MoveOrdering` owns the per-worker heuristic
state used by the picker and search updates.

### Current State

The main-search move order is:

1. TT move, after pseudo-legal validation.
2. Good noisy moves and promotions.
3. Two killer moves.
4. One countermove hint.
5. Generated quiet moves ordered by history.
6. Bad noisy moves.

When in check, the picker generates evasions instead of the normal staged main
search order. Qsearch uses the same picker interface, but only searches TT/noisy
moves outside check and evasions while in check; it does not use main-search
quiet hints.

Capture ordering is conservative and exact:

- promotions are scored above ordinary captures;
- ordinary captures are classified with `Board::see()`;
- SEE-safe captures are ordered by victim value plus exact SEE score;
- SEE-losing captures remain reachable after quiets in main search;
- qsearch omits SEE-losing noisy moves outside check.

Quiet ordering uses a compact set of refutation and history tables:

- `KillerMoves` stores two quiet beta-cutoff refutations per ply;
- `CounterMoves` stores one quiet reply to the previous move context;
- `QuietHistory` scores quiet moves by side/from/to;
- `ContinuationHistory` adds one previous-move context for generated quiet
  moves;
- `MoveOrdering::Context` caches node-local color and previous-move keys so
  lookup work is not repeated per move.

Quiet-history updates use signed gravity. On a quiet beta cutoff, search rewards
the cutoff quiet, updates killers and countermoves, and applies a conservative
malus to a bounded list of previously searched ordinary quiets. The malus path
is depth-gated, excludes TT and killer quiets, requires at least two failed
quiets, and uses half-strength penalties.

`CaptureHistory` exists as tested table scaffolding, but it is not part of the
active search or picker path. Generated quiets currently share one
history-ordered stage rather than a good/bad quiet split.

### Potential Improvements

Keep the current staged order as the baseline. It is simple, predictable, and
already has the important refutation layers. Future work should prefer measured
search-policy use of the existing history data before adding new picker stages.

Recommended next directions:

- Use quiet and continuation history to tune late-move reductions. The current
  LMR formula uses depth and move count plus node type, move kind, check state,
  checking-move status, and killer status, but not history scores. History
  scores could reduce less for strong quiets and reduce more for poor quiets.
- Add cautious history-based quiet pruning near the leaves. This should be a
  search-policy step, not a move-generation change, and it should preserve the
  first-legal-move safety assumptions used by current pruning.
- Revisit capture history only as a complete capture-ordering experiment:
  capture-history scoring, attempted-capture malus, and lazy threshold SEE need
  to be evaluated together. Exact SEE should remain the active baseline until
  that combined shape produces stable fixed-depth evidence.
- Consider adding a second previous-ply follow-up history only after the
  one-ply continuation table remains useful across broader tests. Avoid a
  family of history tables before there is evidence that the current context is
  saturated.
- Add move-kind-specific stats before another ordering split. Useful counters
  would separate TT, good noisy, killer, counter, generated quiet, and bad noisy
  cutoff sources instead of relying only on global cutoff index buckets.

Avoid for now:

- a standalone good/bad quiet picker split;
- capture-history reads in qsearch;
- copied score constants from other engines;
- pruning or reduction changes bundled with unrelated move-ordering changes.
