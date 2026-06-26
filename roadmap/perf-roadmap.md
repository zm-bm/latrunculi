# Performance Roadmap

## Summary

The six-pass reference audit found no open correctness blockers. This roadmap
turns the deferred audit findings into measured improvement tasks. The goal is
better search shape, speed, and maintainability without importing reference
engine constants or doing broad rewrites without evidence.

Treat each task as an experiment:

- State the hypothesis before changing code.
- Keep implementation scope small enough to revert cleanly.
- Compare fixed-depth and fixed-time evidence when search behavior changes.
- Keep the change only when correctness stays green and the measured result is
  neutral or better for the intended metric.

## Current P2 Status

Staged move generation remains a plausible idea, but it is deferred until the
generator surface is stage-safe and measured outside search. The Move/MoveList
prerequisite is complete; continue with `roadmap/movegen-stage-refactor.md`.

Prior P2 attempts:

| Attempt | Fixed-Time | Fixed-Depth | Lesson |
|---|---:|---:|---|
| Adapter-style lazy picker | NPS `+0.94%`, depth `+0.25` | NPS `-5.27%`, nodes `+15.59%`, time `+15.27%` | Front-running a full-list picker pays staged overhead plus old generation cost. |
| Narrowed adapter front-end | NPS `+66.13%`, depth `+0.75` | NPS `-6.95%`, nodes `+10.82%`, time `+16.63%` | Fixed-time depth gains are not enough when fixed-depth efficiency regresses. |
| True staged main-search picker | NPS `-7.14%`, depth `-0.08 avg` | NPS `-4.89%`, nodes `+8.59%`, time `+28.01%` | Staging avoided quiet generation, but overhead or ordering drift dominated. |

Latest diagnostic result: current generator modes are not yet stage-safe enough
for another picker rewrite. The next P2 patch should add generator contracts,
union/duplicate tests, and board-core rows for split-batch cost before changing
search behavior. Use `roadmap/movegen-stage-refactor.md` for the staged
generation plan.

Diagnostic findings to preserve:

- `generate<QUIETS>()` is an internal enum mode, not a public contract.
- `CAPTURES` currently behaves like noisy generation because promotions are
  included.
- Promotion generation needs clear quiet/capture partitioning before staged
  batches feed search.
- Split-generation rows are missing for noisy, quiet, evasions, and
  noisy-plus-quiet union cost.
- In the diagnostic baseline, `generate_all` measured `70.320 ns/op` and
  `legal_filter_all` measured `123.568 ns/op`, so legality and board-state work
  remain large enough that skipped quiet generation must be measured.

Diagnostic run directories:

- Board-core:
  `scratch/bench-runs/20260625-212323-p2-diagnostics-board-before`
- Perft: `scratch/bench-runs/20260625-212353-p2-diagnostics-perft-before`
- Direct-UCI smoke:
  `scratch/bench-runs/20260625-212359-p2-diagnostics-search-smoke`

## Evidence Gates

Baseline gates for any implementation task:

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
```

Use focused tests first, then full CTest. Use reference engines as shape checks,
not as tuning sources.

## Benchmark Runner Contract

Use `bench/bench.py` as the single Python benchmark front door. C++ timed code
remains canonical in `bench/benchmark.cpp`; Python only builds, runs, captures
metadata, writes summaries, and compares run directories.

Standard run directory:

```text
scratch/bench-runs/<timestamp>-<label>/
  manifest.json
  results.tsv
  summary.md
  raw/
```

Primary commands:

```bash
python3 bench/bench.py run board-core --label board-core-smoke --profile smoke --repeats 3
python3 bench/bench.py run perft --label perft-smoke --profile smoke --repeats 2
python3 bench/bench.py run search-smoke --label search-smoke --profile smoke
python3 bench/bench.py run direct-uci --label direct-uci-mid --profile mid
python3 bench/bench.py run match --label match-smoke --opponent /path/to/opponent
python3 bench/bench.py compare <baseline-run-dir> <candidate-run-dir>
```

Direct-UCI position selections:

- `suite`: curated moderate set, `startpos` plus five Arasan positions.
- `arasan20-full`: all 200 rows from `bench/arasan20.epd`, excluding
  `startpos`.

Match lane:

- `run match` uses `cutechess-cli` for local sanity matches only.
- Default local engine is `build/<preset>/bin/latrunculi`; pass `--opponent`
  explicitly.
- Use this lane before broader Cute Chess or OpenBench testing, but do not treat
  short local matches as SPRT evidence.

## Completed Work

P0 and P1 are complete. Use these rows as controls for later experiments, not as
tuned targets. Future changes should compare against a fresh same-command
control when the result is within a few percent.

| Task | Implemented Shape | Result | Future Boundary |
|---|---|---|---|
| P0: Search-Shape Instrumentation | Stats for beta cutoffs by index, PVS re-searches, aspiration fails, qnode ratio, and separate main/qsearch TT fields behind stats builds. | Kept. Normal builds have no active stats-counter cost; stats output is stable enough for iteration. | Treat the measured P0 delta as the local control band for P1-P4. |
| P1: Qsearch TT Integration | Non-PV qsearch-root TT probe/store only, depth `0`, existing mate-score adjustment, existing bound semantics, `NULL_MOVE` storage. | Kept. Fixed-depth searched fewer nodes/time; fixed-time was roughly neutral. | Deeper qTT use, qTT move ordering, or TT static-eval payloads need separate evidence. |
| P2.5: Board-Core Benchmarking | Suite-style C++ `benchmark` entrypoint plus unified `bench/bench.py` front door for board-core, perft, search-smoke, direct-UCI, and local cutechess match runs. | Kept as infrastructure. Board/perft repeats aggregate median/mean/min/max under the plain `cpp-benchmark` result format; direct-UCI positions derive from `bench/arasan20.epd`; all runs write `manifest.json`, `results.tsv`, `summary.md`, and `raw/` under `scratch/bench-runs/`. | Use this lane before revisiting staged generation or make/unmake optimizations. |

Reference note: Stockfish and Ethereal both probe/store TT in qsearch. CPW does
not, and remains useful only as the simple correctness model.

## Benchmark Baselines

| Change | Mode | NPS | Nodes / Time | Depth | Interpretation |
|---|---|---:|---:|---:|---|
| P0 vs pre-P0 | `release-dev`, fixed-time | `-1.99%` | nodes `-2.01%` | `+0.03` | Local control band, not a proven slowdown. |
| P0 stats vs P0 normal | `release-stats-dev`, fixed-time | `-0.70%` | nodes `-0.70%` | `-0.07` | Acceptable diagnostic-build cost. |
| P1 vs pre-P1 | `release-dev`, fixed-time | `-0.66%` | roughly flat | `+0.03` | Neutral; keep due fixed-depth gain. |
| P1 vs pre-P1 | `release-dev`, depth 12, 1 thread | `+0.56%` | nodes `-15.00%`, time `-15.15%` | same target | Fewer nodes and less time. |
| P1 vs pre-P1 | `release-dev`, depth 13, 4 threads | `-0.17%` | nodes `-4.36%`, time `-3.82%` | same target | Fewer nodes and less time. |
| P2 adapter-style lazy attempt | `release`, fixed-time | `+0.94%` | nodes `+0.91%` | `+0.25` | Rejected: fixed-depth regressed. |
| P2 adapter-style lazy attempt | `release`, fixed-depth | `-5.27%` | nodes `+15.59%`, time `+15.27%` | same target | Reverted. Partial staging plus full-list fallback was not keepable. |
| P2 narrowed adapter attempt | `release`, fixed-time | `+66.13%` | nodes `+65.94%` | `+0.75` | Rejected: fixed-depth regressed despite fixed-time depth gain. |
| P2 narrowed adapter attempt | `release`, fixed-depth | `-6.95%` | nodes `+10.82%`, time `+16.63%` | same target | Reverted. Do not repeat this wrapper shape unchanged. |
| P2 true staged attempt | `release`, fixed-time | `-7.14%` | nodes `-7.12%` | `-0.08 avg` | Rejected: production NPS dropped without a depth gain. |
| P2 true staged attempt | `release`, fixed-depth | `-4.89%` | nodes `+8.59%`, time `+28.01%` | same target | Reverted. True staging avoided quiet generation, but the implementation still lost too much node/time efficiency. |

P0 comparison commits:

- Pre-P0: `291010377f74ef9838b24d9dcc26ed48ee90ee1b`
- P0: `9d7f7ca02a2b55102dcc55576a9eaf0de7ffff75`

## Stats Baselines

| Metric | P0 Control | P1 Result | Use |
|---|---:|---:|---|
| Beta cutoffs | `185k` | not a keep signal | Move ordering and pruning context. |
| Early cutoffs | `90.9%` | not a keep signal | Compare P2/P4 ordering changes. |
| Cutoff average index | `1.36` | not a keep signal | Lower is usually better if node/time also holds. |
| PVS re-searches | `652` | not a keep signal | Compare P3 aspiration/PVS changes. |
| Main TT hit | `25.0%` | `25.1%` | Main TT behavior stayed comparable. |
| Main TT cutoff | `94.3%` | `94.3%` | Main TT behavior stayed comparable. |
| QTT hit | `0.0%` | `24.6%` | P1 qsearch TT is active. |
| QTT cutoff | `0.0%` | `95.5%` | P1 qsearch TT hits mostly cut off. |
| QNode ratio | `65.6%` | `66.4%` | Watch for qsearch growth in later changes. |

## Board-Core Smoke Baseline

Baseline command:

```bash
python3 bench/bench.py run board-core --label p2-5-board-smoke --profile smoke --build-preset release-dev
```

Revision: `dfcc26e`

| Mode | Avg ns/op | Cases | Use |
|---|---:|---:|---|
| `generate_all` | `86.857` | `4` | Raw pseudo-legal full movegen cost. |
| `generate_captures` | `41.053` | `4` | Raw tactical movegen cost. |
| `legal_filter_all` | `140.525` | `4` | Full generation plus legality filter. |
| `make_unmake_legal` | `48.762` | `4` | Legal make/unmake over all legal moves. |
| `make_unmake_quiet` | `47.678` | `4` | Quiet make/unmake cost. |
| `make_unmake_capture` | `50.561` | `3` | Capture make/unmake cost. |
| `make_unmake_special` | `53.770` | `3` | Promotion, en passant, and castling make/unmake cost. |
| `null_make_unmake` | `29.775` | `4` | Null move make/unmake cost. |

Perft smoke command:

```bash
python3 bench/bench.py run perft --label p2-5-perft-smoke --profile smoke --build-preset release-dev
```

| Case | Mode | Nodes/sec | ns/node |
|---|---|---:|---:|
| `startpos` | `depth_4` | `12728525` | `78.564` |
| `pos3` | `depth_4` | `12746289` | `78.454` |
| `pos5` | `depth_3` | `11737546` | `85.197` |

## Board-Core Optimization Pass

Fresh baseline run directories:

- Board-core: `scratch/bench-runs/20260625-175349-board-core-before`
- Perft: `scratch/bench-runs/20260625-175420-perft-before`
- Direct-UCI smoke:
  `scratch/bench-runs/20260625-175425-board-core-search-sanity-before`

Kept change:

- `MoveList::add` now assigns a constructed `Move` into the existing
  `std::array<Move, MAX_MOVES>` slot instead of placement-new constructing over
  an already-live object.

Best after-run evidence:

- Board-core:
  `scratch/bench-runs/20260625-180211-board-core-after-movelist-assign-repeat`
- Perft:
  `scratch/bench-runs/20260625-180155-perft-after-movelist-assign-repeat`
- Direct-UCI smoke:
  `scratch/bench-runs/20260625-180303-board-core-search-sanity-after-movelist-assign`

| Mode | Result |
|---|---:|
| `generate_all` | `-8.03%` avg ns/op |
| `generate_captures` | `-6.60%` avg ns/op |
| `legal_filter_all` | `-5.30%` avg ns/op |
| `make_unmake_legal` | `-0.30%` avg ns/op |
| `make_unmake_quiet` | `-0.22%` avg ns/op |
| `make_unmake_capture` | `-0.04%` avg ns/op |
| `null_make_unmake` | `-0.43%` avg ns/op |

Perft repeat was neutral-to-positive across the standard cases:

| Case | Nodes/sec delta |
|---|---:|
| `pos2` | `+0.8%` |
| `pos3` | `+2.4%` |
| `pos4b` | `+0.1%` |
| `pos4w` | `-0.0%` |
| `pos5` | `+1.2%` |
| `pos6` | `+0.5%` |
| `startpos` | `+0.8%` |

Rejected experiments:

- Reusing a precomputed occupancy through `update_check_data()` was too noisy:
  some make/unmake rows improved, but perft was mixed and included a clear
  `pos6` regression in the first after-run.
- `is_legal_pseudo_move()` state-access cleanups improved isolated
  `legal_filter_all` in one variant, but perft either regressed or stayed mixed.

Next recommendation: profile the remaining `legal_filter_all` cost separately
from raw generation before revisiting staged move generation. The append-path
fix shows generation still matters, but staged generation should wait until
legality filtering and make/unmake costs are better understood.

## Board-Core Attribution Pass

Fresh attribution run directories:

- Board-core:
  `scratch/bench-runs/20260626-024304-board-core-attribution`
- Perft: `scratch/bench-runs/20260626-024339-board-core-attribution`

Benchmark change:

- Added prebuilt legal-filter rows so generation is not timed with
  `is_legal_pseudo_move()`.
- Split the old special make/unmake bucket into castle, en passant, promotion,
  and capture-promotion rows.

Average board-core standard rows:

| Mode | Avg ns/op | Cases | Read |
|---|---:|---:|---|
| `generate_all` | `74.863` | `50` | Full pseudo-legal generation remains much larger than a single legality check. |
| `generate_captures` | `41.818` | `50` | Capture/noisy generation is still cheap relative to full generation. |
| `legal_filter_all` | `117.426` | `50` | Full generation plus all legality checks. |
| `legal_filter_prebuilt_all` | `2.625` | `50` | Isolated legality check is small per move. |
| `legal_filter_prebuilt_quiet` | `2.723` | `50` | Similar to all-move legality filtering. |
| `legal_filter_prebuilt_capture` | `2.319` | `25` | Normal capture legality filtering is not a standalone bottleneck. |
| `legal_filter_prebuilt_king` | `4.678` | `45` | King legality is higher, as expected from attack checks. |
| `legal_filter_prebuilt_ep` | `3.963` | `5` | En passant legality is higher but rare. |
| `legal_filter_prebuilt_evasion` | `4.146` | `5` | Evasion filtering is higher but position-specific. |
| `make_unmake_legal` | `48.887` | `50` | Current make/unmake baseline. |
| `make_unmake_quiet` | `47.737` | `50` | Quiet make/unmake is near the all-legal average. |
| `make_unmake_capture` | `52.249` | `30` | Captures are modestly higher. |
| `make_unmake_castle` | `48.863` | `5` | Castling is not a standout cost. |
| `make_unmake_promotion` | `51.775` | `5` | Promotion is modestly higher. |
| `make_unmake_capture_promotion` | `58.010` | `5` | Capture-promotion is a slower special case but rare. |
| `make_unmake_en_passant` | `60.174` | `5` | En passant is a slower special case but rare. |
| `null_make_unmake` | `30.391` | `50` | Null move remains cheap. |

Perft standard remained healthy, with median depth-4 throughput between
`14.8M` and `18.1M` nodes/sec across the standard fixtures.

Read: do not refactor `is_legal_pseudo_move()` or make/unmake broadly before
the staged movegen work. The isolated legality checks are not dominating on
their own; the likely payoff remains reducing full generation or avoiding later
batches. Keep en passant and capture-promotion in mind as correctness-sensitive
special paths, not immediate optimization targets.

## Active Backlog

## P2: Staged move generation

Hypothesis: staged move generation can avoid unnecessary work in cutoff-heavy
nodes. Lazy generation by move class may reduce nodes' per-move overhead and
improve cutoff efficiency.

Status: deferred pending generator diagnostics.

Next attempt: use `roadmap/movegen-stage-refactor.md` for public
noisy/quiet/all/evasion generator contracts, union/duplicate tests, and
board-core rows for split-generation cost. Do not add search-stage counters or
change the picker until those contracts exist.

Keep criteria: no duplicate or missing moves, same legality behavior, measured
split-batch cost, and a credible fixed-depth retry plan.

## P3: Aspiration, LMR, And Pruning Tuning

Hypothesis: current aspiration windows, LMR, razoring, futility, and related
forward-pruning choices are conservative but may leave strength or speed on the
table.

Scope: tune one family at a time. Use P0 instrumentation to guide changes. Do
not import Stockfish, Ethereal, or Minic constants directly.

Evidence: aspiration fail-low/fail-high counts, re-search counts,
beta-cutoff-by-index data, tactical/search tests, fixed-depth node counts, and
direct-UCI benchmark runs.

Keep criteria: fewer harmful re-searches or better node efficiency without
tactical regressions; fixed-time behavior improves or stays neutral.

Revert criteria: improved NPS but worse fixed-depth nodes, tactical failures,
unstable PVs, or parameter changes that cannot be justified by measured data.

## P4: Additional Move-Ordering Heuristics

Hypothesis: capture history, countermove history, or continuation history may
improve cutoff quality beyond the current TT/PV, tactical, killer, and simple
history model.

Scope: evaluate one heuristic at a time. Keep the existing history table and
killer behavior intact unless evidence shows a conflict. Avoid large picker
architecture rewrites here; those belong in P2.

Evidence: move-ordering tests, history/killer tests, beta-cutoff-by-index data,
fixed-depth search results, and direct-UCI runs.

Keep criteria: earlier cutoffs, fewer searched moves before beta, and neutral or
better fixed-depth/fixed-time results.

Revert criteria: larger state with no cutoff improvement, worse killer/history
priority behavior, or tuning that depends on copied constants.

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
