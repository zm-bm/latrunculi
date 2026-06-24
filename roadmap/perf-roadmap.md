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

## P0: Search-Shape Instrumentation

Hypothesis: the next useful search improvements need better visibility before
tuning. Existing NPS/depth numbers are not enough to explain whether move
ordering, aspiration, reductions, qsearch, or TT behavior changed.

Scope: add stats for beta cutoffs by move index, re-searches, aspiration
fail-low/fail-high, qsearch node ratio, and separate main-search versus qsearch
TT hit/cutoff data. Keep counters behind the existing stats build path.

Evidence: stats-enabled focused search tests, direct-UCI output with the new
fields, and a before/after `search_experiment.py` run showing stable collection.

Keep criteria: counters are cheap when stats are disabled, output is stable
enough for benchmarks, and existing search behavior is unchanged.

Revert criteria: stats affect non-stats builds, materially slow normal search,
or make benchmark output noisy without explaining search behavior.

## P1: Qsearch TT Integration

Hypothesis: probing and optionally storing qsearch results can reduce repeated
tactical work and improve effective depth, especially in capture-heavy
positions.

Scope: add qsearch TT consumption conservatively. Keep mate-score adjustment,
bound semantics, legality boundaries, and PV/root conservatism consistent with
main search. Do not mix this with pruning or ordering changes.

Evidence: focused qsearch/search tests, TT tests for qsearch bounds if new
storage paths are added, fixed-depth node comparison, and fixed-time direct-UCI
comparison.

Keep criteria: no correctness regressions, no PV instability from stale TT
moves, and fixed-depth nodes or fixed-time NPS/depth improve or stay neutral.

Revert criteria: tactical regressions, illegal/stale move use, noisy score
swings without node savings, or worse fixed-depth behavior.

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
