# Performance Roadmap

## Summary

The reference audit found no open correctness blockers. This roadmap is the
single durable backlog for measured engine-improvement work. Keep changes small
enough to revert, compare fixed-depth and fixed-time evidence when search shape
changes, and avoid importing reference-engine constants without local
measurement.

Current focus: P3 search/pruning. P2 move ordering is closed as good enough and
is now the baseline that P3 should measure against.

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

## Completed Baselines

| Task | Status | Key Result | Boundary |
|---|---|---|---|
| P0: Search-shape instrumentation | kept | Stats for beta cutoff index, PVS re-searches, aspiration fails, TT hit/cutoff rates, qnode ratio, and staged picker fields are available behind stats builds. | Normal builds should stay free of stats output/cost. |
| P1: Qsearch TT integration | kept | Non-PV qsearch-root TT probe/store reduced fixed-depth nodes/time and was fixed-time neutral. | Deeper qTT use, qTT move ordering, or TT static-eval payloads need separate evidence. |
| P2: Good enough move ordering | closed | Single owning staged `MovePicker` is accepted as the baseline. Closure stats show no obvious pathology. | Deeper ordering heuristics move to P4 or later. |
| P2.5: Benchmark infrastructure | kept | `bench/bench.py` covers board-core, perft, search-smoke, direct-UCI, compare, and local cutechess match runs. | Use this before board, movegen, search, or eval performance work. |

Reference note: Stockfish and Ethereal both probe/store TT in qsearch. CPW does
not, and remains useful only as the simple correctness model.

## Current P2 Baseline

P2 is closed as good enough at `77c333a7a5385e734eb61bac5bf06947823b1596`. No
move-ordering tuning was added during the closure pass.

Accepted architecture:

- one owning staged `MovePicker` is used by root, PV, non-PV, and qsearch paths;
- the picker owns `ScoredMove` storage and no longer prebuilds a full
  `MoveList`;
- public movegen exposes named non-evasion, noisy, quiet, evasion, and
  pseudo-legal contracts;
- qsearch has one public picker mode and internally uses noisy or evasion
  generation based on check state;
- ordering uses PV/hash hints, promotions/good tacticals, killers, history
  quiets, and bad noisy moves;
- capture scoring keeps the SEE gate and adds a victim-value bonus;
- quiet ordering remains deliberately simple.

Latest staged-picker benchmark:

| Metric | Before `cb14a9a` | After `77c333a` | Change |
|---|---:|---:|---:|
| Direct-UCI mid avg NPS | `4,213,406` | `4,547,404` | `+7.97%` |
| Direct-UCI mid avg nodes | `6,323,166` | `6,825,982` | `+7.99%` |
| Direct-UCI mid avg depth | `15.44` | `15.78` | `+0.34` |
| Board-core generation | old full/capture names | new staged names | `+17-27%` |
| Board-core legal filter | baseline | staged baseline | neutral |

P2 closure evidence:

| Evidence | Result |
|---|---|
| `cmake --preset release-dev` | passed |
| `cmake --build --preset release-dev` | passed |
| `ctest --preset release-dev` | passed, `1/1` test target |
| `git diff --check` | passed |
| Stats run | `scratch/bench-runs/20260627-125245-p2-good-enough-stats` |
| Fixed-depth run | `scratch/bench-runs/20260627-130220-p2-good-enough-fixeddepth` |
| Direct-UCI mid run | `scratch/bench-runs/20260627-130229-p2-good-enough-mid` |

Stats-build closure metrics:

| Metric | Average |
|---|---:|
| Staged noisy generated | `88.9%` |
| Staged quiet generated | `12.4%` |
| Staged cutoff before quiet | `86.2%` |
| Staged cutoff during quiets | `2.6%` |
| Staged cutoff during bad noisy | `0.0%` |
| Skip-quiet requests | `2.6%` |
| Beta cutoff average index | `1.2` |
| QNode ratio | `66.8%` |
| Main TT hit | `24.9%` |
| QSearch TT hit | `23.7%` |

Read: quiet generation is not happening at nearly every staged node, most staged
cutoffs happen before quiet generation, and beta cutoffs remain early. There is
no evidence that P2 needs another tuning pass before P3.

Fixed-depth control:

| Position | Depth | Nodes | Time ms | NPS | Score | Bestmove |
|---|---:|---:|---:|---:|---:|---|
| `startpos` | `12` | `698998` | `496` | `1409270` | `cp 39` | `d2d4` |
| `arasan20-01` | `12` | `526207` | `281` | `1872622` | `cp -119` | `e4a8` |
| `arasan20-08` | `12` | `1387961` | `836` | `1660240` | `cp -271` | `h6h5` |

Direct-UCI mid sanity:

| Metric | Average |
|---|---:|
| NPS | `4594039` |
| Nodes | `6895605` |
| Depth | `15.78` |

Historical P2 lesson: earlier staged attempts regressed fixed-depth efficiency
when they mixed staged policy with prebuilt/full-list artifacts. Do not
reintroduce a production prebuilt-vs-staged split unless fresh evidence clearly
beats the current single-picker baseline.

## Board-Core Baselines

Board-core work should start from fresh board-core rows plus profiler data, not
from broad make/unmake or legality-filter rewrites based on suspicion.

Kept `MoveList::add` optimization:

| Mode | Result |
|---|---:|
| `generate_all` now `generate_non_evasions` | `-8.03%` avg ns/op |
| `generate_captures` now `generate_noisy` | `-6.60%` avg ns/op |
| `legal_filter_all` | `-5.30%` avg ns/op |
| `make_unmake_legal` | `-0.30%` avg ns/op |
| `make_unmake_quiet` | `-0.22%` avg ns/op |
| `make_unmake_capture` | `-0.04%` avg ns/op |
| `null_make_unmake` | `-0.43%` avg ns/op |

Attribution snapshot:

| Mode | Avg ns/op | Read |
|---|---:|---|
| `generate_all` now `generate_non_evasions` | `74.863` | Full pseudo-legal generation remains larger than one legality check. |
| `generate_captures` now `generate_noisy` | `41.818` | Capture/noisy generation is cheap relative to full generation. |
| `legal_filter_all` | `117.426` | Full generation plus legality checks. |
| `legal_filter_prebuilt_all` | `2.625` | Isolated generated-move legality is small per move. |
| `make_unmake_legal` | `48.887` | Current make/unmake baseline. |
| `make_unmake_capture` | `52.249` | Captures are modestly higher. |
| `make_unmake_en_passant` | `60.174` | Slower but rare and correctness-sensitive. |
| `null_make_unmake` | `30.391` | Null move remains cheap. |

Read: isolated legality checks were not the standalone bottleneck. En passant
and capture-promotion remain correctness-sensitive special paths, not immediate
optimization targets without profiling.

## Active Backlog

## P3: Search Foundations

Hypothesis: before tuning aspiration, LMR, null move, razoring, or futility, the
search stack should be rebuilt and validated from a bare fail-soft alpha-beta
core upward.

Scope: execute `roadmap/p3-search-foundations.md`. Start with S0 bare
alpha-beta, then add qsearch, TT, aspiration, PVS, null move, razoring/futility,
and LMR one layer at a time. Keep the current Board, MovePicker, eval,
UCI/threading, and benchmark infrastructure.

Evidence: focused search tests, stats-build counters where relevant,
fixed-depth controls, direct-UCI runs, and a final S9 baseline recorded here.

Keep criteria: each restored search layer has clear correctness evidence and
neutral or explainable fixed-depth/fixed-time behavior.

Revert criteria: tactical failures, broken fail-soft or bound semantics,
unstable root publication, or a feature whose effect cannot be explained by
measured evidence.

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
