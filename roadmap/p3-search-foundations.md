# P3 Search Foundations

search: establish bare alpha-beta foundation
search: add quiescence foundation
search: add root-line list iterative deepening
search: restore main tt integration
search: restore qsearch tt integration
search: restore aspiration windows
search: restore pvs
search: restore null move pruning
search: restore razoring and futility
search: restore lmr

## Summary

Rebuild and validate the search stack from a bare, correct alpha-beta core
upward. The point is not to rewrite the whole engine. Keep the current `Board`,
`MovePicker`, eval, UCI/threading, and benchmark infrastructure, then add search
features back one layer at a time with evidence.

Current status: S0-S8 are kept. S9 LMR and S10 final baseline are still ahead.
Keep future updates focused on the current rung and the latest benchmark anchor;
older scratch run directories were useful while building the stack but do not
need to remain in this forward-looking plan.

## Ground Rules

- Start with correctness and tree-shape clarity, not strength tuning.
- Keep current board/movegen/make-unmake/eval contracts intact.
- Use commit-sized ladder steps instead of a new runtime experiment framework.
- Do not add `LATRUNCULI_SEARCH_EXPERIMENTS` yet; revisit it only if branch or
  commit comparisons become painful.
- Do not copy constants from Stockfish, Ethereal, Minic, or CPW.
- Record what each rung changes, what evidence passed, and whether the feature
  is kept, adjusted, or deferred.

## Foundation Ladder

### S0: Bare Fail-Soft Alpha-Beta

Status: kept. Established the fail-soft negamax alpha-beta core with legal move
generation, make/unmake state, draw/repetition checks, mate/stalemate returns,
root result publication, and stop handling.

### S1: Quiescence Search

Status: kept. Depth-zero search enters qsearch; qsearch handles stand-pat,
noisy moves, promotions, and in-check evasions. Stand-pat remains disabled while
in check.

### S2: Root-Line List And Iterative Deepening

Status: kept. Root search builds legal root lines, searches with iterative
deepening, reorders by the best completed line, and preserves the last completed
result when stopped mid-depth.

### S3: Main-Search TT

Status: kept. Non-root alpha-beta probes TT for main-search cutoffs and hash
move ordering, then stores exact/lower/upper bounds with mate-score adjustment
and depth authority. Root positions remain TT-conservative.

### S4: QSearch TT

Status: kept. Qsearch probes/stores depth-0 TT records after terminal handling,
uses valid qsearch hash moves for ordering, and writes lower/exact/upper bounds
for stand-pat, tactical cutoffs, normal exits, and in-check mate exits.

### S5: Aspiration Windows

Status: kept. Root iterative deepening searches each completed depth through a
`50` cp aspiration window seeded from the previous accepted root score. Failed
attempts widen fail-soft windows, but only accepted in-window depths publish root
state.

### S6: PVS

Status: kept. PV nodes search the first legal move full-window, scout later
moves with a null window, and re-search full-window on strict alpha improvement.
PV TT cutoffs require exact depth-sufficient records; non-PV nodes keep bound
cutoffs.

### S7: Null-Move Pruning

Status: kept. Non-PV main search tries null-move pruning after terminal
handling, mate-distance pruning, and the main TT probe. Guards are `can_null`,
not in check, `depth >= R`, side-to-move non-pawn material `> ROOK_MG`, and no
depth-sufficient parent TT upper bound below beta. Null cutoffs return the
fail-soft null value without TT stores or PV/killer/history updates.

### S8: Razoring And Futility

Status: kept. Non-PV main search applies shallow static-eval razoring before
null-move pruning and move generation. Razoring is limited to depths `1..3`,
requires `can_null`, not in check, no TT move, and a local margin fail-low
condition, then verifies with qsearch before returning the fail-low qsearch
value. Futility is limited to non-PV depths `1..3` outside check and
mate-adjacent alpha; after the first legal move, quiet non-checking moves are
skipped through `MovePicker::skip_quiet_moves()`.

Latest evidence: focused release and stats
`SearchTest.*:MovePickerTest.*SkipQuiet*:SearchInstrumentation.*` suites passed,
full `ctest --preset release-dev` passed, benchmark parser compile passed, and
`git diff --check` passed. Current fixed-depth depth-10 anchor:
`scratch/bench-runs/20260703-005311-p3-s8-razor-before-null-fixeddepth-d10`.
Use this as the moving-forward S8 control; the six-position suite completed
with `5.76M` total nodes and `3147 ms` total measured time. Scores/best moves:
`startpos cp 27 b1c3`, `arasan20-01 cp -122 e4a8`,
`arasan20-08 cp -286 h6h5`, `arasan20-16 cp 138 f1e1`,
`arasan20-21 cp 93 d5c3`, and `arasan20-30 cp 338 g5e6`.
Stats spot-check
`scratch/bench-runs/20260703-005328-p3-s8-razor-before-null-stats-arasan20-01-d10`
reported `49615` null tries at `85.5%` cutoffs, `17676` razor tries at `96.7%`
cutoffs, and `20630` futility skips.

### S9: LMR

Add late-move reductions with the local guards and full-window re-search when a
reduced move improves alpha.

Evidence: LMR attempt/re-search stats if available, fixed-depth node/time
comparison, tactical tests, and direct-UCI mid.

### S10: Full Search Baseline

Restore the intended current search stack and compare it against the P2
baseline. This becomes the new P3 baseline before any tuning pass.

Evidence: full correctness gate, stats-build smoke, fixed-depth controls, and
direct-UCI mid. Record durable run directories and aggregate readings in
`roadmap/perf-roadmap.md`.

## Reference Use

- Local reference checkouts live under `/home/rick/code/chess-engine-refs`.
- Ethereal: closest compact alpha-beta/search organization shape; root state is
  tracked with per-depth PV lines, MultiPV lines, and root best-move arrays.
- Stockfish: mature conventions for root moves, TT, PVS, qsearch, and pruning
  boundaries; `RootMove`/`RootMoves` is the heavyweight root-list reference.
- Minic: modern second opinion on pruning and qsearch edge cases; its
  `RootScores` vector is the lightweight root-result reference.
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
./build/release-dev/bin/tests \
  --gtest_filter='SearchTest.*:MovePickerTest.*SkipQuiet*:SearchInstrumentation.*'
```

Stats-build proof when stats fields or search-shape counters matter:

```bash
cmake --preset release-stats-dev
cmake --build --preset release-stats-dev
./build/release-stats-dev/bin/tests \
  --gtest_filter='SearchTest.*:MovePickerTest.*SkipQuiet*:SearchInstrumentation.*'
python3 bench/bench.py run direct-uci \
  --label p3-foundations-stats \
  --profile smoke \
  --build-preset release-stats-dev \
  --skip-build
```

Fixed-depth controls:

```bash
python3 bench/bench.py run fixed-depth-uci \
  --label p3-foundations-fixeddepth-d10 \
  --depth 10 \
  --build-preset release-dev
```

Direct-UCI sanity:

```bash
python3 bench/bench.py run direct-uci \
  --label p3-foundations-mid \
  --profile mid \
  --build-preset release-dev
```

## Definition Of Done

P3 foundations are complete when:

- each rung from S0 through S10 has been implemented or explicitly deferred;
- each kept rung has correctness and benchmark evidence;
- failures are recorded with the reason and the revert point;
- `roadmap/perf-roadmap.md` records the final S10 baseline;
- later P3 tuning can start from a known-good full search baseline.
