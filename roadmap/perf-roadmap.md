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

P2 now has the reference-style single owning `MovePicker` architecture in
production. The picker owns staged generation for main search and qsearch; any
remaining P2 work should improve ordering/performance inside this interface
rather than reintroducing a prebuilt-vs-staged split.

Completed pieces:

- generator API cleanup is complete: callers have named
  pseudo-legal/noisy/quiet/all/evasion contracts.
- noisy/quiet duplicate, disjointness, and union tests exist.
- split board-core rows exist for all/noisy/quiet/evasion generation.
- stats-gated staged picker counters exist for noisy generation, quiet
  generation, and cutoff source rates.
- picker scoring uses a separate signed `MoveScore` lane; capture victim-bonus
  scoring was kept, while the plain widened-history-range experiment was not.
- prior staged search retry failed and was reverted.
- a single owning `MovePicker` interface is used by root, PV, non-PV, and
  qsearch paths.
- `MovePicker` owns its active move/score buffers directly; the former
  thread-owned `MovePickerScratch` allocation was removed.
- qsearch now has one public picker mode; the picker internally chooses noisy
  generation when out of check and evasion generation when in check.
- qsearch-in-check returns generated evasions, including quiet evasions, without
  using main-search killer stages.

The architecture cleanup is intentionally accepted even if it carries short-term
performance cost. The remaining P2 risk is fixed-depth tree-shape drift from the
new staged ordering. Future work should refine ordering inside the unified
picker, not restore production search paths that prebuild a full move list
before constructing the picker.

Quiet ordering remains intentionally simple: killers before history quiets, with
no good/bad quiet threshold, countermove history, or continuation history yet.
Treat those as measured P2/P4 follow-ups using beta-cutoff index, staged
cutoff-source stats, and fixed-depth evidence; do not bundle them into cleanup
patches.

Latest unified-picker evidence:

- Focused `MovePickerTest.*:MoveGenTest.*:SearchTest.*:SearchStats*.*`: `81`
  tests passed.
- Full `ctest --preset release-dev`: passed.
- `git diff --check`: clean.
- Direct-UCI smoke:
  `scratch/bench-runs/20260626-124612-unified-staged-picker-final-smoke`.
- Direct-UCI mid:
  `scratch/bench-runs/20260626-124619-unified-staged-picker-final-mid`.
- Board-core smoke:
  `scratch/bench-runs/20260626-124724-unified-staged-picker-final-board`.
- Picker-owned storage direct-UCI smoke:
  `scratch/bench-runs/20260626-131905-picker-owned-storage-smoke`.
- Picker-owned storage board-core smoke:
  `scratch/bench-runs/20260626-131916-picker-owned-storage-board`.
- No-`MoveSink` direct-UCI smoke:
  `scratch/bench-runs/20260626-133909-no-movesink-smoke`.
- No-`MoveSink` board-core smoke:
  `scratch/bench-runs/20260626-133903-no-movesink-board`.
- Stockfish-style picker storage direct-UCI smoke:
  `scratch/bench-runs/20260626-151030-stockfish-style-picker-smoke`.
- Stockfish-style picker storage board-core smoke:
  `scratch/bench-runs/20260626-151024-stockfish-style-picker-storage`.
- Direct `ScoredMove` output smoke:
  `scratch/bench-runs/20260626-153814-direct-scored-output-smoke`.
- Direct `ScoredMove` output mid:
  `scratch/bench-runs/20260626-153823-direct-scored-output-mid`.
- Direct `ScoredMove` output stats smoke:
  `scratch/bench-runs/20260626-153930-direct-scored-output-stats`.
- Direct-output smoke comparison vs. Stockfish-style picker storage:
  `scratch/bench-runs/20260626-153814-direct-scored-output-smoke/comparison-vs-20260626-151030-stockfish-style-picker-smoke.md`.
- Startpos fixed-depth spot check: depth `12`, `722785` nodes, `486 ms`,
  bestmove `g1f3`.

Prior P2 attempts:

| Attempt | Fixed-Time | Fixed-Depth | Lesson |
|---|---:|---:|---|
| Adapter-style lazy picker | NPS `+0.94%`, depth `+0.25` | NPS `-5.27%`, nodes `+15.59%`, time `+15.27%` | Front-running a full-list picker pays staged overhead plus old generation cost. |
| Narrowed adapter front-end | NPS `+66.13%`, depth `+0.75` | NPS `-6.95%`, nodes `+10.82%`, time `+16.63%` | Fixed-time depth gains are not enough when fixed-depth efficiency regresses. |
| True staged main-search picker | NPS `-7.14%`, depth `-0.08 avg` | NPS `-4.89%`, nodes `+8.59%`, time `+28.01%` | Staging avoided quiet generation, but overhead or ordering drift dominated. |
| Stage-safe generator retry | mixed, mostly neutral NPS, startpos depth down | startpos regressed badly; Arasan improved | Position-dependent result. Reverted; this led to the current single owning `MovePicker` shape. |

Latest result: generator contracts are stage-safe, counters show a real
skip-quiet opportunity, and the search code now uses one owning `MovePicker`
interface. The next task is not more API cleanup; it is a picker-order follow-up
that explains and reduces the remaining fixed-depth tree-shape regression.

Findings to preserve:

- `generate_noisy()` is cheap enough to be useful in cutoff-heavy nodes.
- `generate_noisy() + generate_quiet()` is materially slower than
  `generate_all()` if both batches are always needed.
- Stats smoke showed roughly `50-66%` skip-quiet opportunity, but the staged
  search retry still regressed startpos fixed-depth node/time.
- Closure work must explain or reduce the remaining fixed-depth tree-shape
  regression before P2 is called done.

Historical run directories:

- Board-core:
  `scratch/bench-runs/20260625-212323-p2-diagnostics-board-before`
- Perft: `scratch/bench-runs/20260625-212353-p2-diagnostics-perft-before`
- Direct-UCI smoke:
  `scratch/bench-runs/20260625-212359-p2-diagnostics-search-smoke`
- Capture victim-bonus scoring:
  `scratch/bench-runs/20260626-095450-capture-score-before-smoke`,
  `scratch/bench-runs/20260626-095804-capture-score-after-smoke`,
  `scratch/bench-runs/20260626-095459-capture-score-before-mid`,
  `scratch/bench-runs/20260626-095815-capture-score-after-mid`
- Staged main-search picker candidate:
  `scratch/bench-runs/20260626-100940-staged-main-before-smoke`,
  `scratch/bench-runs/20260626-101659-staged-main-after-smoke`,
  `scratch/bench-runs/20260626-100953-staged-main-before-mid`,
  `scratch/bench-runs/20260626-101712-staged-main-after-mid`,
  `scratch/bench-runs/20260626-102417-staged-main-stats-smoke`
- Single owning picker interface:
  `scratch/bench-runs/20260626-105958-unified-picker-final-smoke`,
  `scratch/bench-runs/20260626-105959-unified-picker-final-mid`
- Unified staged picker:
  `scratch/bench-runs/20260626-124612-unified-staged-picker-final-smoke`,
  `scratch/bench-runs/20260626-124619-unified-staged-picker-final-mid`,
  `scratch/bench-runs/20260626-124724-unified-staged-picker-final-board`
- Picker-owned storage cleanup:
  `scratch/bench-runs/20260626-131905-picker-owned-storage-smoke`,
  `scratch/bench-runs/20260626-131916-picker-owned-storage-board`
- No-`MoveSink` cleanup:
  `scratch/bench-runs/20260626-133909-no-movesink-smoke`,
  `scratch/bench-runs/20260626-133903-no-movesink-board`
- Stockfish-style picker storage:
  `scratch/bench-runs/20260626-151030-stockfish-style-picker-smoke`,
  `scratch/bench-runs/20260626-151024-stockfish-style-picker-storage`
- Direct `ScoredMove` output:
  `scratch/bench-runs/20260626-153814-direct-scored-output-smoke`,
  `scratch/bench-runs/20260626-153823-direct-scored-output-mid`,
  `scratch/bench-runs/20260626-153930-direct-scored-output-stats`

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
| Capture victim-bonus scoring | `release-dev`, fixed-time smoke | `+0.45%` | nodes `+0.50%` | `+0.00` | Kept. Neutral small-profile result with no depth loss. |
| Capture victim-bonus scoring | `release-dev`, fixed-time mid | `+0.82%` | nodes `+0.82%` | `-0.11 avg` | Kept. Mixed rows but positive aggregate; capture history remains separate. |

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

## Movegen Stage Ledger

Kept changes:

- Added public generator functions: `generate_pseudo_legal()`,
  `generate_all()`, `generate_noisy()`, `generate_quiet()`, and
  `generate_evasions()`.
- Fixed quiet generation so promotions belong to noisy generation, not quiet
  generation.
- Added movegen contract tests proving noisy/quiet disjointness,
  duplicate-freedom, and `noisy + quiet == all` for representative non-check
  positions.
- Added board-core rows for `generate_noisy`, `generate_quiet`,
  `generate_evasions`, and `generate_noisy_quiet_union`.
- Added stats-gated staged picker counters and direct-UCI columns for quiet
  generation and cutoff-source rates.

Run directories:

- Split board-core:
  `scratch/bench-runs/20260626-025617-movegen-stage-baseline`
- Perft: `scratch/bench-runs/20260626-025653-movegen-stage-baseline`
- Stats counters:
  `scratch/bench-runs/20260626-030232-movegen-stage-counters`
- Failed staged-search retry:
  `scratch/bench-runs/20260626-030746-movegen-stage-search`
- Clean baseline for retry comparison:
  `scratch/bench-runs/20260626-030843-movegen-stage-search-baseline`

Split-generation standard medians:

| Mode | Median ns/op | Read |
|---|---:|---|
| `generate_all` | `58.088` | Current full pseudo-legal generation baseline. |
| `generate_noisy` | `32.686` | Cheap enough to try before quiets. |
| `generate_quiet` | `52.886` | Most of full generation cost remains in quiets. |
| `generate_noisy_quiet_union` | `84.919` | About `46%` slower than full generation if both batches are always needed. |
| `generate_evasions` | `34.513` | Evasion generation remains separate from the first staged retry. |

Stats-build direct-UCI smoke:

| Position | Threads | SkipQuiet% | HintCut% |
|---|---:|---:|---:|
| `startpos` | `1` | `50.1` | `8.5` |
| `startpos` | `4` | `50.6` | `10.2` |
| `arasan20-01` | `1` | `65.8` | `12.6` |
| `arasan20-01` | `4` | `65.7` | `12.7` |

Rejected Phase 5 retry:

- Shape tried: main-search non-check staging with direct PV/hash hints, noisy
  generation before quiet generation, weak tacticals deferred until after
  quiets; qsearch was not part of that retry.
- Fixed-time smoke was mostly neutral on NPS, but `startpos` lost depth.
- Fixed-depth was mixed: Arasan improved, but startpos regressed badly.

Fixed-depth spot check:

| Position | Threads | Depth | Nodes | Time | Read |
|---|---:|---:|---:|---:|---|
| `startpos` | `1` | `12` | `+84.8%` | `+92.2%` | Rejected. |
| `startpos` | `4` | `13` | `+51.8%` | `+62.5%` | Rejected. |
| `arasan20-01` | `1` | `12` | `-8.1%` | `-6.6%` | Improved. |
| `arasan20-01` | `4` | `13` | `-9.1%` | `-11.8%` | Improved. |

Read: the generator contracts are no longer the blocker, and the current code
now uses a single owning `MovePicker` interface. The remaining P2 task is
closing the position-dependent search behavior, likely from order drift, hint
handling, or the cost shape of splitting the picker.

Closure attempt:

- Correctness gates passed:
  - `cmake --build --preset release-dev`
  - `./build/release-dev/bin/tests --gtest_filter='MoveGenTest.*:MovePickerTest.*:SearchTest.*'`
  - `ctest --preset release-dev`
  - `git diff --check`
- Fixed-time runs:
  - `scratch/bench-runs/20260626-111838-staged-closure-smoke`
  - `scratch/bench-runs/20260626-111844-staged-closure-mid`
- Fixed-depth runs:
  - baseline `cb14a9a`:
    `scratch/bench-runs/20260626-112603-staged-closure-fixeddepth-baseline-cb14a9a`
  - current candidate:
    `scratch/bench-runs/20260626-112513-staged-closure-fixeddepth-current`

Fixed-depth depth-12, one-thread comparison:

| Position | Nodes | Time | NPS | Bestmove | Read |
|---|---:|---:|---:|---|---|
| `startpos` | `+24.7%` | `+26.6%` | `-1.5%` | `b1c3` -> `d2d4` | Regressed. |
| `arasan20-01` | `-19.0%` | `-23.7%` | `+6.2%` | `e4a8` -> `e4a8` | Improved. |
| `arasan20-08` | `+53.3%` | `+49.8%` | `+2.4%` | `h6h5` -> `h6h3` | Regressed. |

Variant findings:

| Variant | Result | Read |
|---|---|---|
| full prebuilt main inside current picker | `startpos` worse than current | Split generation is not the only cause. |
| old capture scoring without victim bonus | partial node improvement, still regressed | Victim bonus contributes but is not the blocker. |
| generated-list killer ordering | much worse | Direct killer slots are not the blocker. |
| generated-list validation for PV/TT hints | no material node change | Hint pseudo-legality admission is not the blocker. |
| legacy qsearch picker | almost no change | Qsearch sharing is not the main cause. |
| legacy prebuilt picker semantics | `startpos` near baseline, `arasan20-08` still worse | The regression is in main picker ordering semantics. |
| swap-discard generated PV/TT duplicates | helped `arasan20-08`, worsened `startpos` | Old full-list swap artifacts matter but are not a complete fix. |

Updated decision: keep the unified staged `MovePicker` architecture as the
reference-like shape, even though fixed-depth tree growth remains material in
some positions. P2 performance closure is still deferred. The next follow-up
should refine main-search ordering semantics inside the unified picker,
especially PV/TT consumption effects, quiet tie order after staged batches, and
staged cutoff behavior before quiet generation. Do not reintroduce a production
prebuilt picker split or layer on unrelated heuristics until this picker-order
issue is resolved.

## Active Backlog

## P2: Staged move generation

Hypothesis: staged move generation can avoid unnecessary work in cutoff-heavy
nodes. Lazy generation by move class may reduce nodes' per-move overhead and
improve cutoff efficiency.

Status: unified staged `MovePicker` architecture is implemented and accepted.
Root, PV, non-PV, and qsearch paths use one picker interface; qsearch has one
public mode and the picker selects noisy versus evasion generation internally.
P2 performance closure is still deferred because fixed-depth evidence shows
remaining tree-shape drift.

Current evidence:

| Check | Result | Read |
|---|---:|---|
| Fixed-time smoke | mostly positive NPS; one four-thread startpos row `-0.7%` NPS | Acceptable smoke. |
| Fixed-time mid | all rows positive NPS, roughly `+1.2%` to `+6.6%`; depth mostly neutral | Good candidate signal. |
| Single-picker interface, fixed-time mid | mixed versus the staged-main candidate, roughly `-3.3%` to `+6.3%`; depth mostly neutral | Acceptable cleanup signal; keep watching startpos and single-thread rows. |
| Fixed-depth startpos depth 12 | nodes `+24.7%`, time `+26.6%`, NPS `-1.5%` | Regressed. |
| Fixed-depth arasan20-01 depth 12 | nodes `-19.0%`, time `-23.7%`, NPS `+6.2%` | Improved. |
| Fixed-depth arasan20-08 depth 12 | nodes `+53.3%`, time `+49.8%`, NPS `+2.4%` | Regressed. |
| Unified staged picker smoke | `scratch/bench-runs/20260626-124612-unified-staged-picker-final-smoke` | Architecture evidence. |
| Unified staged picker mid | `scratch/bench-runs/20260626-124619-unified-staged-picker-final-mid` | Architecture evidence. |
| Unified staged picker board-core | `scratch/bench-runs/20260626-124724-unified-staged-picker-final-board` | Board-core cost record. |
| Unified staged picker startpos depth 12 | `722785` nodes, `486 ms`, bestmove `g1f3` | Spot check. |

Next task: do a larger picker-order follow-up before adding new ordering
heuristics. Candidate areas are PV/hash consumption effects, quiet tie order
after captures are split out, staged cutoff behavior before quiet generation,
and whether PV nodes should use a different generation policy than non-PV nodes.

Closure criteria for the next follow-up:

- keep the single owning `MovePicker` interface as the production architecture;
- tune the staged picker only when the fix is tied to measured ordering drift,
  duplicate handling, tie order, or staged cutoff evidence;
- consider reverting only a specific ordering change if it worsens correctness
  or makes the picker materially less clear;
- avoid unrelated search tuning, capture history, countermove history,
  continuation history, qsearch TT, or eval changes until this picker-order
  issue is resolved.

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

Scope: evaluate one heuristic at a time. Current kept baseline is signed
`MoveScore` storage plus SEE-gated capture victim-bonus scoring. Keep the
existing history table and killer behavior intact unless evidence shows a
conflict. Avoid large picker architecture rewrites here; those belong in P2.

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
