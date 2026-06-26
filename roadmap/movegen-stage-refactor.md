# Movegen Stage Refactor

## Objective

Make move generation stage-ready without changing search behavior until the
generator contracts and measurements justify it.

This is an execution spec for a `/goal`. Follow the phases in order. Do not
skip gates. If a gate fails, stop, report the failed gate, and leave the code in
the smallest coherent state that preserves correctness.

Prerequisite complete: `Move` is identity-only, `MoveList` is a container-only
fixed buffer, and ordering scores are owned by `MovePicker`.

## Current Findings

The movegen foundation is sound: bitboard pseudo-legal generation,
fixed-capacity move lists, special moves, and search-side legality filtering are
all conventional choices.

The current generator surface is not stage-safe enough for another staged
search retry:

- `generate<QUIETS>()` exists as an internal mode, but is not a public contract.
- `CAPTURES` behaves like noisy generation because it includes promotions.
- Promotion generation does not expose a clear quiet/capture split.
- Board-core currently measures full generation and captures, but not noisy,
  quiet, evasions, or noisy-plus-quiet union cost.
- Prior staged-search attempts skipped some later generation but still
  regressed fixed-depth efficiency.

## Target Contracts

Add public wrappers with explicit names:

```cpp
MoveList generate_all(const Board&);
MoveList generate_noisy(const Board&);
MoveList generate_quiet(const Board&);
MoveList generate_evasions(const Board&);
```

Contract:

- `generate_all()`: all pseudo-legal moves for a non-check position.
- `generate_noisy()`: captures, en passant, and all promotions for a non-check
  position.
- `generate_quiet()`: non-captures and non-promotions, including castling, for
  a non-check position.
- `generate_evasions()`: all pseudo-legal evasions for an in-check position.
- `generate_noisy() + generate_quiet()` must equal `generate_all()` outside
  check.
- Noisy and quiet batches must be duplicate-free and disjoint.
- Good versus weak tactical classification remains in `MovePicker` / SEE, not
  in the generator.

Internal templates and `GeneratorMode` values may remain if they keep the code
simple. Public callers should use named wrappers only when relying on the
contract.

## Global Rules

Allowed throughout:

- Add or rename generator wrappers.
- Refactor movegen internals needed for noisy/quiet/evasion contracts.
- Add focused tests and board-core benchmark rows.
- Add stats-gated counters after real generator batches exist.
- Record durable benchmark findings in `roadmap/perf-roadmap.md`.

Forbidden until the gated staged-search phase:

- Do not change search behavior.
- Do not change `MovePicker` ordering, priority bands, or qsearch pruning.
- Do not add new move-ordering heuristics.
- Do not optimize legal filtering as part of this refactor.
- Do not copy reference-engine constants.

Reference use:

- Ethereal: closest staged noisy/quiet generator shape.
- Stockfish: mature generator and staged picker conventions.
- Minic: modern second opinion.
- CPW: simple full-list correctness model.

Use references for boundaries and edge cases, not for tuned constants.

## Phase 0: Baseline And Scope Check

Purpose: make the starting point explicit.

Files:

- `include/movegen.hpp`
- `src/movegen.cpp`
- `tests/movegen.test.cpp`
- `bench/benchmark.cpp`
- `roadmap/perf-roadmap.md`

Allowed changes:

- None, unless needed to fix stale docs in this file.

Forbidden changes:

- No engine code changes in this phase.

Evidence:

```bash
git status --short
git rev-parse HEAD
cmake --build --preset release-dev
./build/release-dev/bin/tests --gtest_filter='MoveGenTest.*'
```

Proceed if:

- The baseline movegen tests pass, or any existing failure is clearly unrelated
  and recorded.

Stop if:

- The baseline is broken in a way that makes generator contract tests
  meaningless.

## Phase 1: Public Generator Contracts

Purpose: add the named API and tests that define the contract before changing
search.

Files:

- `include/movegen.hpp`
- `src/movegen.cpp`
- `tests/movegen.test.cpp`

Allowed changes:

- Add `generate_all()`, `generate_noisy()`, `generate_quiet()`, and
  `generate_evasions()`.
- Keep `generate<M>()` as an internal or compatibility wrapper.
- Explicitly instantiate any internal modes needed by the public wrappers.
- Add test helpers that compare move sets by `Move::bits`.
- Add representative positions for quiet, tactical, promotion, castling,
  en-passant, and in-check cases.

Forbidden changes:

- Do not update search call sites.
- Do not change move ordering.
- Do not change legality filtering.

Evidence:

```bash
./build/release-dev/bin/tests --gtest_filter='MoveGenTest.*'
git diff --check
```

Proceed if:

- Public wrappers compile and are covered by focused tests.
- `generate_all()` matches current non-check `generate<ALL_MOVES>()`.
- `generate_evasions()` matches current in-check behavior.

Stop if:

- The wrapper API cannot be added without changing search semantics.

## Phase 2: Noisy / Quiet Internals

Purpose: make the named contracts true, especially around pawn promotions.

Files:

- `include/movegen.hpp`
- `src/movegen.cpp`
- `tests/movegen.test.cpp`

Allowed changes:

- Refactor pawn and promotion helpers as needed.
- Ensure all promotions belong to noisy generation.
- Ensure quiet generation emits no promotions.
- Ensure en passant belongs to noisy generation.
- Split push-promotion and capture-promotion helper logic if it makes the mode
  predicates clearer.

Forbidden changes:

- Do not add public `generate_promotions()` unless benchmark evidence later
  requires it.
- Do not classify good versus weak captures in movegen.
- Do not alter legal filtering or search-side legality authority.

Evidence:

```bash
./build/release-dev/bin/tests --gtest_filter='MoveGenTest.*:SearchTest.*'
ctest --preset release-dev
git diff --check
```

Proceed if:

- `generate_noisy() + generate_quiet()` equals `generate_all()` for
  representative non-check positions.
- Noisy and quiet batches are duplicate-free and disjoint.
- Promotion positions cover quiet promotions and capture promotions.
- Search tests still pass without search code changes.

Stop if:

- Noisy/quiet disjointness requires duplicate suppression that would be better
  owned by the picker. Record the issue before proceeding.

## Phase 3: Split-Generation Benchmarks

Purpose: measure the new batches before search consumes them.

Files:

- `bench/benchmark.cpp`
- `bench/bench.py` only if organizer parsing must change
- `roadmap/perf-roadmap.md`

Allowed changes:

- Add board-core rows for:
  - `generate_all`
  - `generate_noisy`
  - `generate_quiet`
  - `generate_evasions`
  - `generate_noisy_quiet_union`
- Keep C++ benchmark code as the timed authority.
- Record standard-run results and run directories in `roadmap/perf-roadmap.md`.

Forbidden changes:

- Do not tune generator code only to win one benchmark row.
- Do not change search behavior.

Evidence:

```bash
python3 bench/bench.py run board-core --label movegen-stage-baseline --profile standard --repeats 5
python3 bench/bench.py run perft --label movegen-stage-baseline --profile standard --repeats 3
ctest --preset release-dev
git diff --check
```

Proceed if:

- Benchmark rows are stable and compare full generation with split generation.
- Noisy-plus-quiet union cost is understood.
- Perft remains green.

Stop if:

- Split generation is materially slower and there is no plausible way for
  skipped quiet generation to recover the cost. Record the result in
  `roadmap/perf-roadmap.md` and do not proceed to search counters.

## Phase 4: Search-Stage Opportunity Counters

Purpose: measure whether staged search is worth retrying without changing move
order or search behavior.

Files:

- `include/search_stats.hpp`
- `src/search.cpp`
- `tests/search_stats.test.cpp`
- `roadmap/perf-roadmap.md`

Allowed changes:

- Add stats-gated counters only after real noisy/quiet functions exist.
- Count nodes that would skip quiet generation after PV/hash/noisy stages.
- Count nodes that would skip all generated stages beyond PV/hash hints.
- Count generated moves by noisy and quiet stage.
- Count legal-filter and SEE/classification calls by stage only if needed.
- Keep normal builds no-op through the existing stats path.

Forbidden changes:

- Do not change move ordering.
- Do not change which moves are searched.
- Do not add diagnostic cost to normal builds.

Evidence:

```bash
cmake --build --preset release-dev
./build/release-dev/bin/tests --gtest_filter='SearchStats.*:SearchTest.*'
cmake --build --preset release-stats-dev
./build/release-stats-dev/bin/tests --gtest_filter='SearchStats.*:SearchTest.*'
python3 bench/bench.py run direct-uci --label movegen-stage-counters --profile smoke --build-preset release-stats-dev
git diff --check
```

Proceed if:

- Normal builds stay behaviorally unchanged.
- Stats builds show a credible rate of avoidable quiet generation.
- The counter output is stable enough to compare later staged-search attempts.

Stop if:

- The counters show little opportunity to skip quiet generation, or diagnostic
  wiring would make normal builds pay for stats.

## Phase 5: Gated Staged Main-Search Retry

Purpose: retry staged search only if the measured generator and counter data
justify it.

This phase is allowed in the same `/goal` only if Phases 1-4 pass and the
evidence supports proceeding. Otherwise stop after Phase 4 with a report.

Files:

- `include/move_picker.hpp`
- `src/search.cpp`
- `tests/move_picker.test.cpp`
- `tests/search.test.cpp`
- `roadmap/perf-roadmap.md`

Allowed changes:

- Start with main-search, non-check nodes only.
- Use public generator batches where they preserve existing priority bands.
- Keep PV/TT moves as ordering hints only; legality remains authoritative.
- Add local duplicate suppression only if a test proves it is required.

Forbidden changes:

- Do not change qsearch generation in this phase.
- Do not change in-check generation in this phase.
- Do not add capture history, countermove history, continuation history, or new
  tuning constants.
- Do not weaken legal filtering.

Priority contract:

```text
PV > TT/hash > promotions > good tacticals > killers > history quiets > weak tacticals
```

Evidence:

```bash
./build/release-dev/bin/tests --gtest_filter='MovePickerTest.*:SearchTest.*'
ctest --preset release-dev
python3 bench/bench.py run direct-uci --label movegen-stage-search --profile smoke
python3 bench/bench.py run direct-uci --label movegen-stage-search-mid --profile mid
python3 bench/bench.py run board-core --label movegen-stage-search --profile standard --repeats 5
git diff --check
```

Keep if:

- Fixed-depth node/time is neutral or better.
- Fixed-time direct-UCI is neutral or better.
- Bestmove/PV drift is explainable by ordering or search nondeterminism.
- The implementation is simpler or at least clearly justified by the measured
  skip rate.

Revert or stop if:

- Fixed-depth node/time regresses.
- Search behavior changes before the generator contracts explain it.
- Duplicate handling becomes broad or fragile.
- The staged picker becomes harder to reason about than the current full-list
  picker without measured payoff.

## Follow-Up Lane: Legal Filter Attribution

Keep this separate from the staged generator refactor.

Problem: search checks legality for every picked pseudo-legal move. This is
safe and simple, but board-core measurements show `legal_filter_all` is much
more expensive than raw generation.

Future tasks:

- Add benchmark rows that filter a prebuilt move list so generation and legality
  are not measured together.
- Count king moves, en passant moves, pinned-piece moves, and obviously-safe
  non-pinned moves.
- Consider a legality fast path only after attribution is clear.

## Final Evidence

Run before marking the goal complete:

```bash
cmake --build --preset release-dev
ctest --preset release-dev
git diff --check
rg -n "Move::value|Move::priority|StagedMovePicker|QuiescenceMovePicker|MoveList::sort|p2-staged-movegen-diagnostics|move-movelist-refactor" roadmap include src tests --glob '!roadmap/movegen-stage-refactor.md'
```

If Phase 4 or 5 touched stats builds, also run:

```bash
cmake --build --preset release-stats-dev
./build/release-stats-dev/bin/tests --gtest_filter='SearchStats.*:SearchTest.*'
```

## Definition Of Done

The goal is complete when one of these outcomes is true:

- Diagnostic completion: Phases 1-4 are complete, results are recorded, and the
  Phase 5 gate did not justify a staged-search retry.
- Full completion: Phases 1-5 are complete, staged search passes all keep
  criteria, and results are recorded.
- Stopped by gate: a phase fails a stop condition, the failure is documented,
  and the worktree is left in a coherent correctness-preserving state.

Do not mark complete merely because code compiles. The evidence and gate result
must be explicit.
