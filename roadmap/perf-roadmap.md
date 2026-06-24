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
cmake --build --preset release-dev --target benchmark
./build/release-dev/bin/benchmark
python3 bench/direct_uci_bench.py
python3 bench/search_experiment.py run --profile mid
```

Use focused tests first, then full CTest. Use reference engines as shape checks,
not as tuning sources.

## Completed Work

P0 and P1 are complete. Use these rows as controls for later experiments, not as
tuned targets. Future changes should compare against a fresh same-command
control when the result is within a few percent.

| Task | Implemented Shape | Result | Future Boundary |
|---|---|---|---|
| P0: Search-Shape Instrumentation | Stats for beta cutoffs by index, PVS re-searches, aspiration fails, qnode ratio, and separate main/qsearch TT fields behind stats builds. | Kept. Normal builds have no active stats-counter cost; stats output is stable enough for iteration. | Treat the measured P0 delta as the local control band for P1-P4. |
| P1: Qsearch TT Integration | Non-PV qsearch-root TT probe/store only, depth `0`, existing mate-score adjustment, existing bound semantics, `NULL_MOVE` storage. | Kept. Fixed-depth searched fewer nodes/time; fixed-time was roughly neutral. | Deeper qTT use, qTT move ordering, or TT static-eval payloads need separate evidence. |

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

## Active Backlog

## P2: Staged move generation

Hypothesis: staged move generation can avoid unnecessary work in cutoff-heavy
nodes. Lazy generation by move class may reduce nodes' per-move overhead and
improve cutoff efficiency.

Scope: experiment with staged generation for hash/PV, promotions, captures,
killers, quiets, and weak tacticals. Preserve current priority-band semantics
and legal filtering. Do not add new ordering heuristics in the same patch.

Evidence: move-list and search tests, beta-cutoff-by-index data from P0,
fixed-depth node/time comparison, and direct-UCI benchmark comparison.

Keep criteria: no duplicate or missing moves, same legality behavior, and clear
hot-path benefit in fixed-depth or fixed-time evidence.

Revert criteria: complex picker code without measured gain, move ordering
contract regressions, or higher node/time cost.

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
