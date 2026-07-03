# Performance Roadmap

## Summary

The reference audit found no open correctness blockers. This roadmap is the
single durable backlog for measured engine-improvement work. Keep changes small
enough to revert, compare fixed-depth and fixed-time evidence when search shape
changes, and avoid importing reference-engine constants without local
measurement.

Current focus: P4 move-ordering heuristics. P3 search foundations are closed as
the current search baseline for future tuning.

## Evidence Gates

Baseline gate for implementation work:

```bash
git status --short
git rev-parse HEAD
cmake --preset release-dev
cmake --build --preset release-dev
ctest --preset release-dev
git diff --check
```

Performance gates when search shape, move ordering, pruning, TT behavior, or
eval cost changes:

```bash
python3 bench/bench.py run board-core --label board-core-smoke --profile smoke --repeats 3
python3 bench/bench.py run perft --label perft-smoke --profile smoke --repeats 2
python3 bench/bench.py run search-smoke --label search-smoke --profile smoke
python3 bench/bench.py run direct-uci --label direct-uci-mid --profile mid
python3 bench/bench.py compare <baseline-run-dir> <candidate-run-dir>
```

Use `bench/bench.py` as the benchmark front door. C++ timed code remains
canonical in `bench/benchmark.cpp`; Python builds, runs, captures metadata, and
compares run directories under:

```text
scratch/bench-runs/<timestamp>-<label>/
  manifest.json
  results.tsv
  summary.md
  raw/
```

Direct-UCI selections:

- `suite`: `startpos` plus five curated Arasan positions.
- `arasan20-full`: all 200 rows from `bench/arasan20.epd`, excluding
  `startpos`.

Roadmap policy: keep the current durable baseline and the active backlog here.
Intermediate experiment runs, rejected variants, and step-by-step implementation
history belong in commit history or scratch run directories, not in this file.

## Completed Baselines

| Task | Status | Key Result | Boundary |
|---|---|---|---|
| P0: Search-shape instrumentation | kept | Search stats are available behind stats builds. | Normal builds should stay free of stats output/cost. |
| P1: Qsearch TT integration | kept | Non-PV qsearch-root TT probe/store reduced fixed-depth nodes/time and was fixed-time neutral. | Deeper qTT use, qTT move ordering, or TT static-eval payloads need separate evidence. |
| P2: Good enough move ordering | closed | Single owning staged `MovePicker` is accepted as the baseline. Closure stats show no obvious pathology. | Deeper ordering heuristics move to P4 or later. |
| P2.5: Benchmark infrastructure | kept | `bench/bench.py` covers board-core, perft, search-smoke, direct-UCI, compare, and local cutechess match runs. | Use this before board, movegen, search, or eval performance work. |
| P3: Search foundations | kept | Fail-soft alpha-beta, qsearch, root iterative deepening, TT, aspiration, PVS, null move, razoring/futility, and LMR are restored with focused tests and depth-10 evidence. | Treat this as the search baseline; future pruning or tuning changes need fresh fixed-depth and fixed-time evidence. |

## Current P3 Search Baseline

P3 rebuilt the search stack from a bare fail-soft alpha-beta core, then restored
qsearch, root-line iterative deepening, main and qsearch TT integration,
aspiration windows, PVS, null-move pruning, razoring/futility, and LMR. The
kept stack is intentionally conservative around root publication, TT bound
semantics, stopped searches, and tactical pruning guards.

Final S10 validation passed: release and stats focused
`SearchTest.*:MovePickerTest.*SkipQuiet*:SearchInstrumentation.*` suites
passed `77/77`, full `ctest --preset release-dev` passed, benchmark parser
compile passed, and `git diff --check` passed.

Final fixed-depth depth-10 baseline:
`scratch/bench-runs/20260703-112102-p3-final-fixeddepth-d10`. The six-position
suite searched `1.002M` nodes in `558 ms`.

Direct-UCI sanity:
`scratch/bench-runs/20260703-112105-p3-final-smoke` and
`scratch/bench-runs/20260703-112110-p3-final-mid`. The mid run averaged
`4.889M` NPS, `7.338M` nodes, and depth `15.97`, compared with the P2 mid
baseline's `4.594M` NPS, `6.896M` nodes, and depth `15.78`.

Stats spot-check:
`scratch/bench-runs/20260703-112213-p3-final-stats-arasan20-01-d10` reported
`32409` LMR tries at `0.1%` re-searches, `5825` null tries at `71.5%` cutoffs,
`1138` razor tries at `95.5%` cutoffs, `3301` futility skips, main TT
`64.8%/37.7%` hit/cutoff, qsearch TT `23.4%/87.8%` hit/cutoff, and `63.4%`
qnodes. Stats smoke:
`scratch/bench-runs/20260703-112208-p3-final-stats`.

## Board-Core Notes

Board-core work should start from fresh board-core rows plus profiler data, not
from broad make/unmake or legality-filter rewrites based on suspicion. The last
kept board-core pass showed `MoveList::add` was worth simplifying, while
isolated legality checks were not the standalone bottleneck. En passant and
capture-promotion remain correctness-sensitive paths; only touch them with
focused profiling and perft coverage.

## Active Backlog

## P4: Move-Ordering Heuristics

Hypothesis: capture history, countermove history, continuation history, negative
history/malus updates, or good/bad quiet thresholds may improve cutoff quality
beyond the current TT/PV, tactical, killer, and simple history model.

Current baseline to resume from:

- single owning staged `MovePicker`;
- signed `MoveScore` storage;
- SEE-gated capture victim-bonus scoring;
- killers before history quiets;
- simple side/from/to quiet history;
- qsearch noisy/evasion generation through the same picker interface;
- no capture history, countermove history, continuation history, negative
  history malus, or good/bad quiet threshold yet.

Scope: evaluate one heuristic at a time. Preserve the single picker interface
unless a focused experiment proves a different shape is better. Avoid copying
reference-engine constants; use Stockfish/Ethereal/Minic as shape references.

Evidence: move-ordering tests, history/killer tests, beta-cutoff-by-index data,
staged cutoff-source stats, fixed-depth search results, direct-UCI runs, and
eventually local cutechess sanity matches.

Keep criteria: earlier cutoffs, fewer searched moves before beta, and neutral or
better fixed-depth/fixed-time results.

Revert criteria: larger state with no cutoff improvement, worse killer/history
priority behavior, copied tuning constants, or a regression that only looks good
under fixed-time but loses fixed-depth efficiency.

## P5: UCI And Time Management

Hypothesis: richer time management and broader UCI support may improve practical
play and GUI compatibility, but they are not the main speed path.

Scope: consider optimum/max-time budgeting, `go infinite`, `searchmoves`,
`mate`, `Move Overhead`, real ponder, and a dedicated search manager only when
specific lifecycle or compatibility evidence exists.

Evidence: protocol/engine/thread tests, repeated lifecycle tests for threading
changes, direct UCI smoke tests, and compatibility cases from real GUI use.

Keep criteria: simpler or more reliable lifecycle behavior, better time use, or
needed GUI compatibility without destabilizing async search.

Revert criteria: more orchestration complexity without a concrete user-facing
benefit or new race-prone command interactions.

## P6: Evaluation Infrastructure

Hypothesis: eval caching or a broader eval rewrite might help, but only if eval
cost or strength data justifies it.

Scope: profile before adding pawn hash or eval cache. Defer NNUE, broad eval
rewrite, and full trace/tuning infrastructure until there is a strength-testing
pipeline and an active tuning workflow.

Evidence: profiler data showing eval cost, eval/search tests, fixed-depth node
impact, benchmark evidence, and, for broad eval changes, a repeatable strength
test process.

Keep criteria: measurable eval-cost reduction or strength-process-backed
improvement without search instability.

Revert criteria: cache complexity without profiling support, imported eval
constants without tuning evidence, or broad rewrites that obscure current
classical eval behavior.
