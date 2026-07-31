# Search Audit and Refactor Roadmap

## Summary

The search architecture is fundamentally sound and does not need redesign. This
roadmap preserves search policy while focusing on:

- reducing white-box test complexity;
- improving names, comments, and ownership boundaries without changing tuning.

`docs/search-stability.md`, search tuning, evaluation changes, pruning-policy
changes, and new features are explicitly out of scope.

## Current Architecture

### Control flow

```text
Engine::handle(GoCommand)
  -> ThreadPool::start_search()
     -> configure every Thread
     -> wake helpers, then main
        -> SearchWorker::search()
           -> reset per-search state
           -> main worker ages TT and releases helpers
           -> build_root_lines()
           -> iterative deepening
              -> root PVS
                 -> alphabeta()
                    -> quiescence()
           -> publish RootLine snapshot
           -> main worker stops helpers
           -> select_best_root_line()
           -> final info and bestmove
```

### Ownership and lifetime

- `Engine` owns the UCI game-history `Board`, `Writer`, options, and
  `ThreadPool`.
- `ThreadPool` owns persistent `Thread` objects; each `Thread` owns one
  `SearchWorker` and native thread.
- Each worker independently owns:
  - a copied `Board`;
  - `MoveOrdering`;
  - root lines and current result;
  - limits and timing state;
  - node and stop atomics;
  - instrumentation;
  - a mutex-protected published root snapshot.
- The global TT is the only shared search data structure.
- TT storage is stable during search. Resize and clear are idle-only UCI
  operations.
- The main worker increments the non-atomic TT generation before releasing
  helpers, so workers only read a stable generation.
- Quiet and continuation histories are per-worker, per-game state.
- Killers, counters, root data, limits, nodes, stop state, and instrumentation
  are per-search state.
- Board traversal, search ply, local PVs, move pickers, and failed-quiet
  tracking are node-local.

### Important conventions

- `SearchWorker::ply` is search-root-relative and remains separate from Board
  history.
- Mate scores are root-relative in search and converted to position-relative
  form in the TT.
- `Board::is_draw(ply)` distinguishes game-history repetition from cycles
  reached after the search root.
- Root search intentionally does not treat an already drawn root as terminal;
  it must still return a legal move.
- The TT remains power-of-two sized so multiplicative high-bit indexing can use
  a shift.

## Baseline

Original baseline retained for final comparison:

- Commit: `62dfa65117158140b2618c858ca2f13ad8a90c56`
- Compiler: GCC 13.3.0
- Flags: `-O3 -DNDEBUG -std=c++23`
- CPU: Intel i7-11800H
- Affinity: CPU 15
- Threads: 1
- Hash: 32 MiB
- Preserved binary:
  `/tmp/latrunculi-search-audit-baseline.A5H3n9/latrunculi`
- SHA-256:
  `b8aab9175b055c77e8c35718b3c689310076d4d87c8a358700112b1475469388`

Correctness baseline:

- `debug-dev`: passed in 8.46 seconds
- `release-dev`: passed in 0.69 seconds
- `release-stats-dev`: passed in 0.70 seconds

The UCI runner uses an exact 196,608-node limit, five warm-up rounds, and twenty
measured rounds.

| Case | Median nodes/s | Stable best move |
|---|---:|---|
| Cold start position | 1,505,385 | `d2d4` |
| Cold tactical | 1,532,401 | `e2a6` |
| Cold middlegame | 1,624,451 | `c3d5` |
| Cold endgame | 2,609,940 | `b4f4` |
| Cold in-check | 2,665,574 | `e1d2` |
| Cold history-bearing | 1,493,751 | `d2d4` |
| Warm opening ply 2 | 1,505,999 | `b1c3` |
| Warm opening ply 4 | 1,446,552 | `b1c3` |
| Warm opening ply 6 | 1,534,714 | `b5c6` |
| Warm opening ply 8 | 1,474,853 | `e1g1` |

Aggregate medians:

- Cold: 1,779,467 nodes/s
- Retained-history sequence: 1,475,906 nodes/s
- Combined: 1,639,851 nodes/s

The original baseline can emit stale final node/time values, so comparisons
against it must still calculate throughput from the known node boundary and
wall time.

## Remaining Findings

### High-value cleanup

- Reduce and split `search.test.cpp`; replace its approximately 350-line fixture
  surface with a small shared access shim.

### Optional cleanup

- Use a scoped `NodeType` with `Pv` and `NonPv` enumerators.
- Rename `SearchedMoves` to `FailedQuiets` or similarly describe its actual
  history-malus role.
- Correct stale search step numbering and shorten comments that merely narrate
  syntax.
- Remove redundant includes while touching each chunk.

### Explicitly rejected

- Redesigning `SearchWorker`, `ThreadPool`, lazy SMP, or global TT ownership.
- Splitting the 601-line production `search.cpp`; its root, alpha-beta, and
  quiescence implementations form one coherent algorithm unit.
- Arbitrary-sized TT modulo or multiply-high indexing.
- Reopening accepted history lifetime or aging policy.
- Removing `CaptureHistory` scaffolding.
- Changing reductions, pruning guards, aspiration widths, history formulas,
  replacement weights, or move-ordering bands.
- Removing useful instrumentation solely to reduce line count.
- Moving hot picker or history code out of headers without performance
  evidence.

## Chunk Order and Dependencies

```text
TT [complete: 3950f0b]
 └─> Search value/result types [complete]
      └─> Instrumentation [complete: c2946ff]
           └─> Histories and ordering [complete]
                └─> Move picker [complete]
                     └─> Worker lifecycle/reporting [complete]
                          └─> Core search and test restructuring
                               └─> Final integration
```

Each remaining chunk should be separately reviewable. Split correctness fixes
from cleanup when doing so materially improves reviewability.

## Chunk 1: Transposition Table — Complete

Completed in `3950f0b` (`refactor(search): correct TT sizing and simplify its
interface`).

- TT capacity now rounds down to the largest fitting power of two, and resize
  publishes new storage and metadata only after allocation succeeds.
- `store()` is the single search-facing storage API; `store_search()` and the
  unused `prefetch_addr()` were removed.
- TT coverage was reduced from 24 to 19 focused tests while adding
  non-power-of-two capacity regression cases.
- Entry packing, indexing, replacement, concurrency, generation, mate
  conversion, search results, and node accounting remained unchanged.
- Debug, release, and stats-enabled tests passed. The repeated final benchmark
  against the original baseline was neutral: cold -0.16%, retained-history
  -0.17%, and combined +0.06%.

## Chunk 2: Search Value and Result Types — Complete

Completed as a test-only consolidation.

- `SearchLimits`, `PrincipalVariation`, and `RootLine` required no production
  changes; their behavior, representation, timing, and ordering remain intact.
- Their focused coverage now lives in `search_types.test.cpp`; the separate
  limits and PV test files were removed.
- Five fixture-free RootLine selection tests moved out of `search.test.cpp` and
  became two direct behavioral tests, reducing that file by 72 lines.
- Focused coverage was reduced from 15 to 12 tests while retaining limit
  normalization, active-range PV equality, deterministic RootLine selection,
  and fallback behavior.
- Debug, release, and stats-enabled tests passed in 8.60, 0.69, and 0.69 seconds.
  No performance gate was required because production code was unchanged.

## Chunk 3: Search Instrumentation — Complete

Completed in `c2946ff` (`refactor(search): separate hot instrumentation from
reporting`).

- Hot event recording remains inline, while enabled reset, aggregation, and
  formatting now live in `search_instrumentation.cpp`. The header was reduced
  from 520 to 226 lines.
- The disabled specialization is an empty no-op type. Ordinary builds contain
  no enabled instrumentation symbols, branches, calls, or reporting code.
- Total, early, and late cutoffs are derived from the cutoff buckets;
  aspiration re-searches are derived from fail-low plus fail-high. All other
  counters retain their original meanings and denominators.
- Instrumentation-specific formatters were removed. Final reporting formats a
  string through the existing debug writer, preserving the diagnostic text and
  trailing newline.
- Enabled instrumentation shrank from 28,696 to 25,616 bytes, reducing a stats
  worker from 48,472 to 45,392 bytes. The disabled worker remained 19,784
  bytes.
- Focused coverage now verifies the empty disabled mode, every retained event
  family, bounds, complete aggregation, derived report values, stable output,
  and multi-worker reporting.
- Debug, release, and stats-enabled tests passed. Disabled alpha-beta and
  quiescence machine code remained byte-identical, and all benchmark search
  signatures were unchanged.
- The first performance gate measured cold -0.10%, retained-history +0.96%, and
  combined +1.34%. A repeat cleared the only per-case outlier and measured cold
  +1.11%, retained-history +0.20%, and combined +0.09%.

## Chunk 4: Histories and Move-Ordering State — Complete

- `history.hpp` and `move_ordering.hpp` required no production changes; their
  ownership, layout, formulas, hot paths, and accepted lifecycle remain intact.
- `CaptureHistory` remains isolated scaffolding for the later capture-ordering
  review.
- The two production-aligned test files remain separate. Their coverage was
  reduced from 29 tests and 359 lines to 8 tests and 169 lines.
- The retained tests cover signed updates, saturation, aging, clearing, history
  key isolation, continuation indexing, killer rotation, counter replacement,
  and per-search preparation.
- Worker-, search-, and UCI-level tests continue to protect persistence, missing
  contexts, cutoff updates, and `ucinewgame` clearing.
- Debug, release, and stats-enabled tests passed in 8.58, 0.69, and 0.69 seconds.
  No performance gate was required because production code was unchanged.

## Chunk 5: Move Picker — Complete

- Picker-only modes, stages, policies, candidates, and ranges are now private to
  `Picker`; stage names follow the normal scoped-enum style.
- `MoveOrdering::make_context()` always builds the full main-search context.
  Qsearch constructs its current-side-only context internally.
- Scoring bands, SEE classification, hint precedence, generation timing, stage
  order, and emitted move order remain unchanged.
- Focused coverage was reduced from 38 tests and 725 lines to 14 tests and 407
  lines while retaining move-set, ordering, hint, continuation, evasion,
  qsearch, special-move, and skip behavior.
- `Picker` remained 2,136 bytes and `SearchWorker` remained 19,784 bytes.
  `Picker::next()`, noisy scoring, and main-search construction retained
  byte-identical machine code.
- All 18 exact move-order oracle scenarios and all search signatures remained
  unchanged. Debug, release, and stats-enabled tests passed.
- The performance gate measured cold -0.14%, retained-history +0.34%, and
  combined -0.08%, with no per-case regression above 2%.

## Chunk 6: Worker Lifecycle, Threading, and Reporting — Complete

Completed in the current working tree.

- `ThreadPool` now configures every worker before waking helpers behind the
  closed gate and waking the main worker last. A fast main search can no longer
  stop an unconfigured helper that later starts after `bestmove`.
- An immediate stop retains an incomplete published root snapshot but reports
  the first legal root candidate at depth zero. Checkmate and stalemate still
  report `bestmove 0000`.
- Final reporting bypasses ordinary progress suppression and emits fresh node
  and time information immediately before `bestmove`, after helpers stop.
- The test-only `worker_running` atomic was removed, worker snapshot collection
  became private, and TT generation is no longer polled concurrently.
- Direct private-`Thread` tests and redundant lifecycle/snapshot cases were
  removed. Ordinary builds now run 17 focused `ThreadPoolTest` cases; the stats
  build adds its instrumentation case.
- `SearchWorker` remained 19,784 bytes and `Thread` remained 19,888 bytes. Six
  fixed-node search signatures matched the preserved baseline exactly.
- Debug, release, and stats-enabled tests passed in 8.62, 0.64, and 0.63
  seconds. Fifty immediate stops, both terminal roots, and thirty immediate
  32-worker restarts passed; final stats nodes matched the aggregated total.
- GCC's TSan runtime could not start on this host because of an `unexpected
  memory mapping` failure. The same focused 20-test suite passed under Clang
  18's TSan runtime with no race reports.

## Chunk 7: Core Search and Test Restructuring

Files:

- `src/search/search.cpp`
- `src/search/search_worker.hpp`
- current `tests/search/search.test.cpp`
- new focused search test files
- a reduced shared test-access helper
- CMake registration

Production cleanup:

- Keep `search.cpp` as one translation unit.
- Make `NodeType` scoped with `Pv` and `NonPv`.
- Rename `SearchedMoves` to describe retained failed quiets.
- Correct the missing Step 7 and audit step comments for precision.
- Group root, alpha-beta, and quiescence helpers clearly without changing
  execution order.
- Do not change any search condition, margin, reduction, window, depth, or
  history update.

Target tests:

- `quiescence.test.cpp`
  - depth-zero dispatch;
  - draw and max-ply exits;
  - stand pat;
  - in-check evasions and checkmate;
  - PV construction;
  - TT cutoffs, quiet TT rejection, and stored bounds;
  - stats integration;
  - PV-node TT behavior.

- `search.test.cpp`
  - fail-soft alpha-beta;
  - mate-distance and max-ply handling;
  - TT cutoffs and stored bounds;
  - null-move pruning;
  - razoring and futility;
  - quiet cutoff and malus updates;
  - PV behavior;
  - LMR.

- `root_search.test.cpp`
  - legal root-line construction;
  - root draw policy;
  - mate, stalemate, and mate-in-one;
  - root PVS;
  - aspiration widening;
  - iterative-deepening result/PV publication;
  - stopped and partially completed searches;
  - progress suppression and final reporting.

Introduce a small `SearchTestAccess` helper exposing only:

- worker configuration/reset;
- alpha-beta, quiescence, and root entry points;
- Board, ordering, ply, root result/lines, and enabled counters needed by
  retained tests.

Do not preserve the current one-wrapper-per-field fixture API.

Exact consolidation priorities:

- Combine fail-low/fail-high cases.
- Combine mate-distance clamp cases.
- Combine max-ply draw/static cases.
- Combine qsearch exact/upper storage cases.
- Replace the separate checkmate/stalemate root tests and snapshot duplicates
  with one terminal-root table.
- Combine aspiration fail-high/fail-low cases.
- Combine completed root ordering, PV, and snapshot tests.
- Move all RootLine selection tests to `search_types.test.cpp`.
- Replace the quiet cutoff/counter/continuation cluster with:
  - one positive previous-move-context test;
  - one contextless/null-context test;
  - one non-quiet cutoff test.
- Merge quiet and continuation malus assertions into the same representative
  tests.
- Combine TT/killer malus exclusions.
- Combine minimum-depth and minimum-failed-move malus guards.
- Combine minimum-depth and minimum-move-count LMR guards.
- Retain null-move descendant re-enablement, pruning guard tables, root PVS
  re-search, PV TT policy, and stopped-search preservation as focused fragile
  regressions.
- Delete assertions that merely mirror move-picker or Board legality internals
  already covered in their owning tests.

Risk: hot-path and behavior-sensitive even if production edits are intended to
be code-generation-neutral.

Verification:

- Debug, release, and stats-enabled tests.
- Compare generated code around alpha-beta and quiescence.
- Full cold and retained-history performance gate.
- Stable cold best moves, node accounting, and completed depths.
- Suggested commit:
  `refactor(search): simplify search organization and focused coverage`

## Chunk 8: Final Integration

Only create this chunk if earlier work leaves genuine cross-file cleanup.

Checks:

- No stale test names, helpers, includes, or CMake entries.
- No use of `docs/search-stability.md`.
- `clang-format-20` on all touched C++ files.
- `git diff --check`.
- Full `debug-dev`, `release-dev`, and `release-stats-dev` tests.
- Verify UCI `go`, `stop`, `ucinewgame`, `Clear Hash`, thread resizing, and
  terminal positions.
- Compare final candidate against:
  - the immediately preceding accepted chunk;
  - the original preserved baseline.
- Require aggregate throughput within 1%.
- Investigate consistent per-case regressions above 2%.
- Separate cold and retained-history results.
- Confirm the intentional immediate-stop and final-info output changes.
- Do not create an empty cleanup commit.

## Checklist

- [x] Preserve the original binary, compiler, flags, affinity, hash, and UCI
      runner.
- [x] Move instrumentation formatting out of the header.
- [x] Remove only derivable instrumentation counters.
- [x] Verify disabled instrumentation compiles away.
- [x] Simplify history and ordering tests without reopening lifecycle policy.
- [x] Internalize move-picker implementation types.
- [x] Reduce duplicated move-picker tests and preserve exact order.
- [x] Fix immediate-stop legal fallback.
- [x] Force final UCI search information.
- [x] Remove concurrent TT-age polling.
- [ ] Split and reduce `search.test.cpp`.
- [ ] Narrow search test access.
- [ ] Apply only organizational changes to `search.cpp`.
- [ ] Run every chunk's required correctness and performance gate.
- [ ] Compare the final engine directly against commit `62dfa65`.

## Assumptions

- The current search design and selective-search policy remain authoritative.
- Power-of-two TT sizing remains intentional.
- The recently accepted history lifecycle remains unchanged.
- A final legal fallback move does not make an incomplete root snapshot
  completed.
- Stats-enabled builds remain optional diagnostics.
- There are no unresolved implementation-time decisions.
- `docs/search-stability.md` remains uninspected and out of scope.
