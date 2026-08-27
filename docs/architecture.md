# Architecture

## System Overview

```text
UCI input
   |
   v
uci::Engine ---- owns ----> Board
   |
   v
search::ThreadPool ----> search::Thread ----> search::Worker
   |                                              |
   |                                              +-- Board copy
   |                                              +-- movegen
   |                                              +-- eval
   |                                              +-- ordering state
   +-- shared search::tt

search results ----> search::Reporter ----> uci::Writer ----> UCI output
```

Source directories define logical subsystem boundaries, and most
subsystem-owned APIs use matching namespaces. Fundamental chess types and
`Board` remain global types shared across those boundaries.

## Subsystems

| Directory | Responsibility |
| --- | --- |
| `src/core` | Fundamental chess types, pieces, squares, moves, bitboards, attack tables, and move geometry |
| `src/board` | Mutable position representation, reversible history, chess rules, FEN, notation, and static exchange evaluation |
| `src/movegen` | Pseudo-legal move generation, move lists, and production perft |
| `src/eval` | Handcrafted-evaluation parameters, mechanics, incremental base terms, feature extraction, and diagnostics |
| `src/search` | Search algorithm, limits, root results, move ordering, transposition table, workers, and thread lifecycle |
| `src/uci` | Protocol commands, parsing, options, engine coordination, and output formatting |

## Board and Position State

`Board` is the mutable position used by move generation, evaluation, search,
perft, and UCI commands. It maintains redundant representations for efficient
queries:

- per-color piece and occupancy bitboards;
- a square mailbox and piece counts;
- cached king squares and side to move; and
- incrementally maintained `eval::BaseTerms` for material and piece-square
  evaluation.

Reversible state lives in an owned stack of `PlyState` values. Each state holds
the Zobrist key, castling rights, en-passant information, halfmove clock,
previous move, tactical caches, and undo data. `Board::make()` and
`Board::unmake()` update the durable representation, active ply state, Zobrist
key, and evaluation base terms together. Null moves use the same reversible
history without changing piece placement.

`Board` is also the final authority for move legality. It owns attack queries,
castling validation, draw detection, check detection, and static exchange
evaluation. FEN loading rebuilds the complete representation and incremental
caches from the supplied position.

Each search worker owns an independent `Board` copy. Search make/unmake
operations therefore mutate only worker-local position history.

## Move Generation

The `movegen` subsystem emits pseudo-legal candidates in a stable order.
Generation is specialized by side to move and by normal, noisy, quiet, or
evasion mode. Callers use `Board` for final king-safety filtering. Production
perft exercises the same generator and make/unmake path used by search.

## Evaluation

The public evaluation boundary consists of `eval::evaluate()` and
`eval::extract_features()`. Both use the same internal, single-use evaluator;
normal search does not pay for feature construction. Evaluation combines the
Board-owned material and piece-square base terms with pawn, piece, mobility,
threat, king-safety, phase, scaling, and tempo terms.

Evaluation parameters and `eval::TaperedScore` are owned by `src/eval`.
`Board` deliberately depends on `eval::BaseTerms` because those cached values
are handcrafted-evaluation state rather than intrinsic chess-position data.
Feature extraction separates tunable linear terms from the fixed evaluation
residual while retaining the weighted term breakdown used by diagnostics. The
`latrunculi features` mode exports versioned tuning records.

## Search

`search::ThreadPool` owns one or more native `search::Thread` instances. Each
thread owns a `search::Worker`; thread zero is the main worker and the remaining
workers are helpers. `search::Limits` carries the resolved search request into
each worker.

Workers search root moves through iterative deepening, aspiration windows, PVS,
and quiescence. Alpha-beta pruning and reduction techniques use static
evaluation, move ordering, and transposition-table bounds to limit work.

Every worker owns its board, root lines, node counter, and
`search::ordering::State`. Ordering state contains killer and countermove
refutations plus quiet and continuation histories used by the staged move
picker.

The clustered `search::TranspositionTable` is shared globally as `search::tt`.
Its entries use atomic publication so probes return detached, validated record
snapshots while workers search concurrently.

The main worker controls final result selection and publication. Helpers expose
synchronized root snapshots, and the main worker combines those snapshots
before reporting the final line and move. Infinite and ponder searches reuse
the same lifecycle and wait on atomic state until `stop` or `ponderhit` permits
publication.

Search reports through the non-owning `search::Reporter` interface. This keeps
the search subsystem independent of UCI formatting and streams.

## UCI Boundary

The executable initializes attack tables and enters `uci::Engine::loop()`.
Each input line is parsed into the `uci::Command` variant, then dispatched by
the engine on the command thread.

`uci::Engine` owns:

- the current root `Board` and game history;
- parsed UCI `Options`;
- the `uci::Writer`; and
- the `search::ThreadPool`.

The engine validates state changes and search requests before waking workers.
Option updates apply subsystem effects such as TT or thread-pool resizing
before committing the candidate configuration. Command-side state mutations
remain serialized around asynchronous search.

`uci::Writer` implements `search::Reporter`. It translates structured search
results into synchronized `info` and `bestmove` output while also owning the
engine's diagnostic stream. Search code therefore has no dependency on UCI
commands or output syntax.

The same command loop provides local board, evaluation, move, and perft
inspection commands outside the UCI protocol.

## Build and Validation

CMake compiles production sources into the `latrunculi_lib` object library.
The `latrunculi` executable supplies the UCI entry point. Test-enabled presets
also build:

- `tests`, the deterministic GoogleTest executable; and
- `latrunculi-stress`, the reproducible randomized stress executable.

The `release-dev` and `release-stats` presets additionally build
`latrunculi-measure`, the optional component-measurement executable.

Tests mirror the production subsystem layout under `tests/`, with narrow
support fixtures for internal observations. Component measurements live under
`measurements/` and exercise production perft, evaluation, and search paths.
Tests cover correctness; component measurements cover deterministic work and
local performance.
