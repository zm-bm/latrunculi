# P3 Search Foundations

search: establish bare alpha-beta foundation
search: add quiescence foundation
search: restore main tt integration
search: restore qsearch tt integration
search: restore iterative deepening aspiration
search: restore pvs
search: restore null move pruning
search: restore razoring and futility
search: restore lmr

## Summary

Rebuild and validate the search stack from a bare, correct alpha-beta core
upward. The point is not to rewrite the whole engine. Keep the current `Board`,
`MovePicker`, eval, UCI/threading, and benchmark infrastructure, then add search
features back one layer at a time with evidence.

This pass comes before aspiration, LMR, and pruning tuning. Each rung should be
small enough to understand, benchmark, and revert.

## Ground Rules

- Start with correctness and tree-shape clarity, not strength tuning.
- Keep current board/movegen/make-unmake/eval contracts intact.
- Keep the single owning staged `MovePicker` baseline from P2.
- Use commit-sized ladder steps instead of a new runtime experiment framework.
- Do not add `LATRUNCULI_SEARCH_EXPERIMENTS` yet; revisit it only if branch or
  commit comparisons become painful.
- Do not copy constants from Stockfish, Ethereal, Minic, or CPW.
- Record what each rung changes, what evidence passed, and whether the feature
  is kept, adjusted, or deferred.

## Foundation Ladder

### S0: Bare Fail-Soft Alpha-Beta

Goal: prove the basic negamax alpha-beta core is correct and understandable.

Keep:

- legal generated moves through current `MovePicker`;
- make/unmake and active `PositionState` handling;
- draw/repetition checks;
- mate/stalemate returns;
- fail-soft score contract;
- root result publication and halt/cancellation behavior.
- alpha-beta node counts include terminal draw, depth-zero, and max-ply entries;
- root draw detection is deliberately non-root only so UCI still gets a legal
  best move.

Remove or disable for this rung:

- qsearch;
- TT probe/store;
- iterative aspiration windows;
- PVS;
- null-move pruning;
- razoring;
- futility pruning and skip-quiet pruning;
- LMR.

Evidence: focused search tests, fixed-depth startpos and Arasan controls, and a
direct-UCI smoke run. This rung does not need to be strong.

### S1: Quiescence Search

Add qsearch with stand-pat, noisy moves, promotions, and evasions. Stand-pat is
valid only when not in check. In-check qsearch must search legal evasions.

Evidence: qsearch/search tests, tactical sanity positions, fixed-depth node
impact, and direct-UCI smoke.

### S2: Main-Search TT

Add main-search TT probe/store with correct bound semantics, mate-score
adjustment, depth handling, and PV/root conservatism.

Evidence: TT/search tests, main TT stats in stats builds, fixed-depth node
change, and direct-UCI mid sanity.

### S3: QSearch TT

Add qsearch TT probe/store using the existing qTT contract. Keep qsearch move
ordering and static eval behavior explicit.

Evidence: qTT stats, qsearch tests, fixed-depth node/time comparison, and
direct-UCI smoke/mid.

### S4: Iterative Deepening And Aspiration

Add iterative-deepening root search and aspiration widening. Preserve fail-soft
score behavior across fail-low/fail-high re-searches.

Evidence: aspiration stats, root bestmove stability, fixed-depth controls, and
direct-UCI mid.

### S5: PVS

Add principal variation search after the first legal move. Full-window
re-searches must preserve correctness and fail-soft behavior.

Evidence: PVS re-search stats, search tests, fixed-depth node/time comparison,
and direct-UCI mid.

### S6: Null-Move Pruning

Add null-move pruning with the current local guards: not in check, sufficient
material, depth guard, and no immediate repeated null move.

Evidence: null-move try/cutoff stats if available, tactical regression tests,
fixed-depth controls, and direct-UCI mid.

### S7: Razoring And Futility

Add static-eval razoring and futility pruning. Keep guards non-PV and shallow as
appropriate. Reintroduce `MovePicker::skip_quiet_moves()` only through the
existing guarded futility path.

Evidence: razor/futility stats if available, tactical tests, fixed-depth
controls, and direct-UCI mid.

### S8: LMR

Add late-move reductions with the local guards and full-window re-search when a
reduced move improves alpha.

Evidence: LMR attempt/re-search stats if available, fixed-depth node/time
comparison, tactical tests, and direct-UCI mid.

### S9: Full Search Baseline

Restore the intended current search stack and compare it against the P2
baseline. This becomes the new P3 baseline before any tuning pass.

Evidence: full correctness gate, stats-build smoke, fixed-depth controls, and
direct-UCI mid. Record durable run directories and aggregate readings in
`roadmap/perf-roadmap.md`.

## Current Full-Search Baseline

Baseline commit: `77c333a7a5385e734eb61bac5bf06947823b1596`.

Reference runs:

- Direct-UCI mid:
  `scratch/bench-runs/20260627-150916-p3-full-before-mid`
- Stats smoke:
  `scratch/bench-runs/20260627-151040-p3-full-before-stats`

Direct-UCI mid aggregate: depth `15.81`, nodes `6,851,780`, NPS `4,566,045`.

Fixed-depth `go depth 12`, one thread:

| Position | Nodes | Time ms | NPS | Score | Bestmove |
|---|---:|---:|---:|---:|---|
| `startpos` | `698998` | `497` | `1406434` | `cp 39` | `d2d4` |
| `arasan20-01` | `526207` | `306` | `1719630` | `cp -119` | `e4a8` |
| `arasan20-08` | `1387961` | `879` | `1579022` | `cp -271` | `h6h5` |

## S0 Result

Status: implemented as one full-window fail-soft alpha-beta search at the
requested depth. Qsearch, TT search use, aspiration, PVS, null move, razoring,
futility, skip-quiet pruning, LMR, and iterative deepening are disabled.
S0 reports the completed root best move as the UCI PV. TT-followed PV and full
`RootLine` PV storage are deferred until before S4.

Reference runs:

- Direct-UCI smoke:
  `scratch/bench-runs/20260627-160141-p3-s0-smoke`
- Direct-UCI mid:
  `scratch/bench-runs/20260627-160150-p3-s0-mid`
- Stats smoke:
  `scratch/bench-runs/20260627-160126-p3-s0-stats`

Direct-UCI mid aggregate: completed depth `0`, nodes `3,423,307`, NPS
`2,279,271`. The completed depth is `0` because S0 does not use iterative
deepening and did not complete the default max-depth search inside movetime.

Fixed-depth `go depth 5`, one thread:

| Position | Nodes | Time ms | NPS | Score | Bestmove |
|---|---:|---:|---:|---:|---|
| `startpos` | `2911` | `11` | `264636` | `cp 80` | `b1c3` |
| `arasan20-01` | `4478` | `29` | `154413` | `cp 413` | `e4a8` |
| `arasan20-08` | `7655` | `26` | `294423` | `cp -35` | `h6h3` |

## Reference Use

- Ethereal: closest compact alpha-beta/search organization shape.
- Stockfish: mature conventions for TT, PVS, qsearch, and pruning boundaries.
- Minic: modern second opinion on pruning and qsearch edge cases.
- CPW: simple correctness model for alpha-beta, qsearch, and bound contracts.

Use references to validate contracts and shape. Do not import tuning constants
without local evidence.

## Evidence Commands

Baseline correctness:

```bash
git status --short
git rev-parse HEAD
cmake --preset release-dev
cmake --build --preset release-dev
ctest --preset release-dev
git diff --check
```

Focused search evidence:

```bash
./build/release-dev/bin/tests --gtest_filter='SearchTest.*:SearchStats.*'
```

Stats-build proof when stats fields or search-shape counters matter:

```bash
cmake --preset release-stats-dev
cmake --build --preset release-stats-dev
./build/release-stats-dev/bin/tests --gtest_filter='SearchTest.*:SearchStats.*'
python3 bench/bench.py run direct-uci \
  --label p3-foundations-stats \
  --profile smoke \
  --build-preset release-stats-dev \
  --skip-build
```

Fixed-depth controls:

```bash
printf 'uci\nisready\nposition startpos\ngo depth 12\nquit\n' \
  | ./build/release-dev/bin/latrunculi
```

For `arasan20-01` and `arasan20-08`, use the FEN fields from
`bench/arasan20.epd` with the same `go depth 12` command.

Direct-UCI sanity:

```bash
python3 bench/bench.py run direct-uci \
  --label p3-foundations-mid \
  --profile mid \
  --build-preset release-dev
```

## Definition Of Done

P3 foundations are complete when:

- each rung from S0 through S9 has been implemented or explicitly deferred;
- each kept rung has correctness and benchmark evidence;
- failures are recorded with the reason and the revert point;
- `roadmap/perf-roadmap.md` records the final S9 baseline;
- later P3 tuning can start from a known-good full search baseline.
