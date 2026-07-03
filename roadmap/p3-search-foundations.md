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

This pass comes before aspiration, LMR, and pruning tuning. Each rung should be
small enough to understand, benchmark, and revert.

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

Result 2026-06-30: kept. Depth-zero search now enters qsearch; qsearch covers
stand-pat, noisy moves/promotions, and in-check evasions without TT or pruning.
Evidence: focused/full tests plus before-TT baselines
`scratch/bench-runs/20260630-022157-p3-s1-before-tt-smoke`,
`scratch/bench-runs/20260630-022207-p3-s1-before-tt-mid`, and
`scratch/bench-runs/20260630-022314-p3-s1-before-tt-stats`. Fixed-time runs
remain depth-0/`bestmove none` until S2 restores iterative deepening.

### S2: Root-Line List And Iterative Deepening

Add a root-line list and full-window iterative-deepening root driver. Reuse
`RootLine` as the per-root-move line record: each legal root move gets one line
with `best_move`, value, completed depth, completion state, and PV. Keep one
selected/published `RootLine` as the worker result.

Build legal root lines before searching. Search depth `1..options.depth`,
storing each completed candidate's score and PV, then reorder root lines so the
next iteration starts with the best completed line. If a search is stopped
mid-depth, final reporting must use the last completed selected line. Keep all
iterations full-window; aspiration is a later rung.

Evidence: root-line list/search tests, completed-depth publication tests,
fixed-time direct-UCI smoke/mid with real depth and bestmove output, and
stats-build smoke.

Result 2026-06-30: kept. Each worker now builds legal root lines, searches them
with full-window iterative deepening, reorders by the best completed line, and
preserves the last completed line when stopped mid-depth. Evidence:
focused/full tests plus direct-UCI runs
`scratch/bench-runs/20260630-135318-p3-s2-id-smoke`,
`scratch/bench-runs/20260630-135327-p3-s2-id-mid`, and
`scratch/bench-runs/20260630-135430-p3-s2-id-stats`. The mid run now reports
real fixed-time depths (`5-9`) and non-`none` best moves before TT is restored.

### S3: Main-Search TT

Add main-search TT probe/store with correct bound semantics, mate-score
adjustment, depth handling, and root-line conservatism.

Evidence: TT/search tests, main TT stats in stats builds, fixed-depth node
change, and direct-UCI mid sanity.

Result 2026-06-30: kept. Non-root alpha-beta now probes TT for main-search
cutoffs/hash-move ordering and stores exact/lower/upper bounds with existing
mate-score adjustment. Root positions and qsearch remain TT-free. Evidence:
focused/full tests plus direct-UCI runs
`scratch/bench-runs/20260630-214802-p3-s3-main-tt-refresh-smoke`,
`scratch/bench-runs/20260630-214808-p3-s3-main-tt-refresh-mid`, and
`scratch/bench-runs/20260630-214909-p3-s3-main-tt-refresh-stats`. Compared with
S2 mid, fixed-time depth improved on `arasan20-08`, `arasan20-16`,
`arasan20-21`, and `arasan20-30`; NPS was mostly lower from TT overhead, ranging
from about `-9.6%` to `+0.5%`; stats smoke showed MainTT hit/cut activity with
QTT still `0/0`. Fixed-depth depth-6 controls against S2 are in
`scratch/bench-runs/20260630-220921-p3-s2-id-fixeddepth-d6` and
`scratch/bench-runs/20260630-220924-p3-s3-main-tt-fixeddepth-d6`; S3 returned
the same best moves and scores while reducing nodes by `29.6-49.4%` and time by
`22.2-49.6%` across the six-position suite.

### S4: QSearch TT

Add qsearch TT probe/store using the existing qTT contract. Keep qsearch move
ordering and static eval behavior explicit.

Evidence: qTT stats, qsearch tests, fixed-depth node/time comparison, and
direct-UCI smoke/mid.

Result 2026-07-01: kept. Qsearch now probes/stores depth-0 TT records after
draw/max-ply terminal handling, uses valid qsearch hash moves for ordering, and
stores lower/exact/upper bounds for stand-pat, capture cutoff, normal exit, and
in-check mate exits. Evidence: focused/full tests plus direct-UCI runs
`scratch/bench-runs/20260701-033753-p3-s4-qsearch-tt-smoke`,
`scratch/bench-runs/20260701-033802-p3-s4-qsearch-tt-mid`, and
`scratch/bench-runs/20260701-033904-p3-s4-qsearch-tt-stats`. Compared with S3
mid, fixed-time depth was mostly unchanged or improved (`arasan20-16` t4
`8.7 -> 9`, `startpos` t4 `7 -> 7.3`), with one averaged dip on
`arasan20-01` t4 (`7 -> 6.7`). One-thread NPS moved `-1.8%` to `+11.1%`, while
four-thread NPS moved `+5.0%` to `+22.8%`. Stats runs parsed cleanly:
`scratch/bench-runs/20260701-033904-p3-s4-qsearch-tt-stats` and
`scratch/bench-runs/20260701-033914-p3-s4-qsearch-tt-stats-mid-lite` show QTT
hit/cut activity across smoke and suite positions. Fixed-depth depth-6 control
is in `scratch/bench-runs/20260701-033928-p3-s4-qsearch-tt-fixeddepth-d6`;
compared with S3 depth-6, scores and best moves were unchanged while nodes
dropped `0.5-10.7%` across the six-position suite.

### S5: Aspiration Windows

Add aspiration widening around the existing iterative-deepening root search.
Preserve fail-soft score behavior across fail-low/fail-high re-searches.

Evidence: aspiration stats, root bestmove stability, fixed-depth controls, and
direct-UCI mid.

Result 2026-07-02: kept. Root iterative deepening now searches each completed
depth through an aspiration loop seeded from the previous accepted root score,
with a local `50` cp starting window and fail-soft widening on fail-low/fail-high.
Failed attempts may reorder root lines for the retry, but only an accepted
in-window depth updates and publishes the worker root. Evidence: focused/full
release and stats tests plus direct-UCI runs
`scratch/bench-runs/20260702-031745-p3-s5-aspiration-rebuilt-smoke`,
`scratch/bench-runs/20260702-031751-p3-s5-aspiration-rebuilt-mid`, and
`scratch/bench-runs/20260702-031851-p3-s5-aspiration-rebuilt-stats`. Focused
stats-build tests forced both aspiration fail-high and fail-low paths and
asserted nonzero miss/re-search counters; the rebuilt stats smoke parsed natural
aspiration misses (`startpos`: `1/1/2` one-thread, `4/4/8` four-thread;
`arasan20-01`: `4/5/9` one-thread, `16/20/36` four-thread). Fixed-depth depth-6
control is in
`scratch/bench-runs/20260702-031856-p3-s5-aspiration-rebuilt-fixeddepth-d6`;
compared with S4, best moves, scores, and depths were unchanged while nodes
dropped `67.4-84.6%` across the six-position suite. Direct-UCI mid reported
non-`none` best moves for every row and generally reached one to two deeper
plies than S4/S5 stale-run readings.

### S6: PVS

Add principal variation search after the first legal move. Full-window
re-searches must preserve correctness and fail-soft behavior.

Evidence: PVS re-search stats, search tests, fixed-depth node/time comparison,
and direct-UCI mid.

Result 2026-07-02: kept. Search now uses templated `NodeType` dispatch for
`PV` and `NON_PV` nodes. PV nodes search the first legal move full-window; later
PV moves scout with a null window and re-search full-window only on strict
`value > alpha`. Root PVS remains conservative inside the S5 aspiration attempt:
fixed-window root attempts search full-window until a root PV is established,
then scout later root moves. PV TT cutoffs require exact depth-sufficient
records; non-PV nodes keep bound cutoffs. PV qsearch propagates the same PV-node
TT cutoff conservatism recursively. Root publication remains accepted-depth
only, so stopped attempts preserve the last accepted root result.

Final evidence: release direct-UCI runs
`scratch/bench-runs/20260702-190520-p3-s6-pvs-final-smoke` and
`scratch/bench-runs/20260702-190520-p3-s6-pvs-final-mid` completed with no
blank or `none` best moves; mid fixed-time depths were `7-12` across the suite.
Stats runs
`scratch/bench-runs/20260702-190520-p3-s6-pvs-final-stats` and
`scratch/bench-runs/20260702-190520-p3-s6-pvs-final-stats-arasan20-01-d8`
reported nonzero PVS re-searches (`404` in stats smoke, `100` for
`arasan20-01` at depth 8).

Fixed-depth depth-8 control is in
`scratch/bench-runs/20260702-190520-p3-s6-pvs-final-fixeddepth-d8`. Compared
with S5 baseline `scratch/bench-runs/20260702-093522-p3-s5-8d444-fixeddepth-d8`,
all six scores stayed stable; best moves stayed stable except the known
equal-score `startpos` tie (`b1c3 -> d2d4`, both `cp 14`). Total nodes moved
`11.22M -> 9.29M` (`-17.3%`) and total measured time moved `5177 -> 5127 ms`
(`-1.0%`). Per-position node deltas were `-6.5%`, `-10.7%`, `-59.6%`,
`-24.8%`, `+20.4%`, and `-48.5%`; time deltas were `+20.0%`, `-4.5%`,
`-56.1%`, `-12.2%`, `+45.2%`, and `-34.2%`. Compared with the previous S6
cleanup run, scores, best moves, and nodes were unchanged; only measured time
varied.

### S7: Null-Move Pruning

Add null-move pruning with the current local guards: not in check, sufficient
material, depth guard, and no immediate repeated null move.

Evidence: null-move try/cutoff stats if available, tactical regression tests,
fixed-depth controls, and direct-UCI mid.

Result 2026-07-02: kept. Non-PV main search now tries null-move pruning after
terminal handling, mate-distance pruning, and the main TT probe. Current guards
are `can_null`, not in check, `depth >= R`, side-to-move non-pawn material
`> ROOK_MG`, and no depth-sufficient parent TT upper bound below beta. The null
child searches a zero-width window at `depth - R` with `can_null=false`, where
`R` is `3`, or `4` when depth is greater than `6`.

Null cutoffs return the fail-soft null value and do not store TT bounds or
update PV, killers, or history. Stats builds record null-move tries/cutoffs, and
the benchmark parser reports them in direct/fixed summaries and comparisons.

Evidence: focused release and stats `SearchTest.*:SearchInstrumentation.*`
suites passed, full `ctest --preset release-dev` passed, benchmark parser
compile passed, and `git diff --check` passed. Compared with S6 fixed-depth
baseline `scratch/bench-runs/20260702-190520-p3-s6-pvs-final-fixeddepth-d8`,
S7 fixed-depth depth-8
`scratch/bench-runs/20260702-193626-p3-s7-nullmove-fixeddepth-d8` kept all six
scores and best moves unchanged while total nodes moved `9.29M -> 2.18M`
(`-76.5%`) and total measured time moved `5127 -> 1035 ms` (`-79.8%`).

### S8: Razoring And Futility

Add static-eval razoring and futility pruning. Keep guards non-PV and shallow as
appropriate. Reintroduce `MovePicker::skip_quiet_moves()` only through the
existing guarded futility path.

Evidence: razor/futility stats if available, tactical tests, fixed-depth
controls, and direct-UCI mid.

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
root-line list setup are deferred until S2.

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
./build/release-dev/bin/tests --gtest_filter='SearchTest.*:SearchInstrumentation.*'
```

Stats-build proof when stats fields or search-shape counters matter:

```bash
cmake --preset release-stats-dev
cmake --build --preset release-stats-dev
./build/release-stats-dev/bin/tests --gtest_filter='SearchTest.*:SearchInstrumentation.*'
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

- each rung from S0 through S10 has been implemented or explicitly deferred;
- each kept rung has correctness and benchmark evidence;
- failures are recorded with the reason and the revert point;
- `roadmap/perf-roadmap.md` records the final S10 baseline;
- later P3 tuning can start from a known-good full search baseline.
