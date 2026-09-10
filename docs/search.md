# Search Development

This document coordinates `SW-XX` search experiments. It holds the operational baseline,
durable findings, workflow, and queue; search work is not duplicated in `docs/roadmap.md`.

## Goal and baseline

Improve useful depth and convergence through better selectivity without sacrificing correctness,
move quality, or wall-clock performance. Reported depth alone is not an optimization target.

Update these fields together only after an approved candidate is integrated:

| Operational field | Current value |
|---|---|
| Search-behavior baseline | `6767e7447bfa285bef1daeea30d62f8a69367c42` (`6767e74`) |
| Deterministic benchmark | 6,068,328 nodes |
| Cached corpus baseline | `tools/measurements/output/search-baseline-6767e74/` |

The cached rows use `search_measurement_v3` and `tools/measurements/search.epd`. Recollect them
only when search behavior, the workload, or measurement semantics change. The
[measurement guide](../tools/measurements/README.md) owns the command interface.

Keep evidence panels separate and use only those relevant to the mechanism:

- focused mate and material tests are objective correctness guards;
- all 200 source-pinned Arasan positions in `tools/measurements/search.epd` form the broad
  tactical performance corpus; source `bm` and `am` labels are context, not oracles;
- `tools/measurements/search-sentinels.epd` holds four focused convergence cases; and
- task-specific timed, scaling, or protocol panels cover clock, stopping, threading, Hash,
  and protocol work.

Fresh-process, one-thread deterministic repeats are reproducible when completed depth, score,
actual nodes, best move, and PV agree exactly; timing and NPS are excluded. For sentinel
trajectories, useful depth is the greatest depth `d >= 3` for which `d-2..d` share a root move
and score class, both adjacent PV transitions have a nonempty common prefix, and objective
predicates hold.

## Durable findings

| Area | Finding and consequence |
|---|---|
| Determinism and Hash | All 33 fixed-node triplicate groups were exact, dev/stats signatures agreed, and 32 versus 256 MiB Hash changed no result. Depth-to-depth movement is convergence behavior, not run-to-run instability. |
| Objective guards | Mate in one was stable at depth 1 and 46 nodes, mate in two at depth 3 and 767 nodes, and the loose-rook capture at depth 1 and 29 nodes. Keep these as tests, not performance rows. |
| Convergence context | Three of four historical pilot moves were not reproduced at 524,288 nodes and had no shared counter profile. The 14-case audit had five late root changes, one sign flip, one A-B-A, 46 cp late-jump p90, 61.5% median PV survival, and five zero-prefix transitions. Pilots and sentinels diagnose convergence; they do not define the correct move. |
| Capture ordering | Late captures succeeded in 29,374 of 504,598 attempts (5.82%): 7.20% in exact-SEE-good and 0.47% in exact-SEE-bad. The tested CaptureHistory table used 0.9716 times the nodes but 1.0646 times the time, including 1.4129 times at start position. Preserve the current SEE bands unless a distinct proposal earns a new test. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. The tested 200, 300, and 400 cp delta margins would have skipped 3,200, 732, and 206 real NonPV cutoffs. NonPV value usually appears as a beta cutoff, so zero alpha raises is not a safety result. Do not retry those rules. |
| LMR | All 107 sampled high-history reduced fail-lows remained fail-lows at full depth. Do not retry the tested one-ply history protection. This does not rule out different LMR formulas or re-search sequencing. |
| Clock | Unfinished iterations consumed median node shares of 24-26%, but all 210 timed searches matched fresh-depth results. The tested next-iteration predictors fired prematurely 21, 65, and 129 times. Explicit `movetime` remains a hard-duration request. |
| Open mechanisms | Null move and futility remain eligible for small production experiments; the audit lacked eligibility and counterfactual-failure denominators and therefore neither proved nor disproved a change. |

The sentinel baselines are `startpos` (horizon/useful depth 14/11; late root/PV change),
`arasan20-16` (16/14; late root changes and A-B-A), `pilot14-g171-abrupt` (18/17; 4M-to-33M
root/PV change), and `pilot15-g078-secondary` (13/11; late sign flip). Their resolved horizons
are in `sa-02-470a3d7/derived/resolved-horizons.tsv` beneath the measurements output.

Closed shapes require materially new evidence before retrying. The exact CaptureHistory patch is
`tools/measurements/output/sa-03-capture-history-b415f5a/meta/candidate.patch`. The rejected
qsearch predicate was `stand_pat + captured_value + margin <= alpha_before_move` at margins
200, 300, and 400. The rejected clock predictor was
`T_d + m * (T_d - T_(d-1)) >= allocated_time` for `m` in `{1, 2, 4}`. The rejected
history-aware LMR trial protected quiet moves with combined history at least 1024.

The corpus is tactical offline evidence, not Elo, and the clock data came from one machine and
thread. Aggregate counters may overlap iterations or aspiration attempts; use mechanism-specific
denominators when counters decide a change.

## Workflow

Tasks use sequential `SW-XX` IDs and states `pending`, `active`, `done`, `rejected`, and
`skipped`. Keep at most one active task. A pending task may stay compact; expand it to this record
when activated:

```text
SW-XX — title [active]
Hypothesis / candidate:
Baseline / panels / stops:
Result / artifacts:
Disposition / next boundary:
```

Before implementation, pin HEAD and the operational search baseline; predeclare one reversible
candidate, relevant panels and checks, one mechanism-specific material stop (or justified `N/A`),
and an artifact root. Freeze the candidate and result-affecting build inputs before Screen. A
later candidate or build-input change requires an updated declaration and a fresh Screen.

### Screen

Run only enough to reject a clear failure:

1. Focused correctness tests.
2. One cheapest relevant measurement pass. Use the complete 200-position depth-10 corpus for
   pruning, reductions, move ordering, aspiration, and comparable deterministic changes; use a
   task-specific panel for clock, stopping, threading, Hash, or protocol work.
3. One deterministic benchmark for a search or binary candidate.
4. Timing or sentinel trajectories only when the mechanism makes them relevant.

Compare corpus nodes with the cached baseline using geometric mean, median, and case counts.
There is no universal node threshold. Apply only the predeclared stop; a correct marginal or
mixed result may continue. On rejection, preserve decisive evidence, remove candidate-only code
and output, restore the baseline, and do not run Qualification.

### Qualification

Only a Screen survivor proceeds:

1. Run focused tests and the complete `release-dev` suite. Use ASan/UBSan for storage, indexing,
   bounds, depth arithmetic, ownership, representation, parsing, or core board/search/TT state;
   use TSan for concurrency or shared state. A scalar or condition-only change may record these
   as `N/A` with a reason. Use `release-stats` only when counters are needed.
2. Run a second fresh-process copy of each deterministic decision panel and require exact
   candidate-internal agreement in the reproducibility fields above.
3. Recheck applicable objective, legal, and protocol guards; repeat the benchmark and require
   its candidate fingerprint to repeat.
4. Run predeclared repeated timing for clock/stopping changes or changed hot-path work, plus only
   the other mechanism-specific checks named by the task.

A behavior-preserving optimization may finish offline when its exact-signature and throughput
gates pass. A behavior-changing survivor is offline-qualified for paired games even when its
offline performance is marginal or mixed.

### Publish and dispose

Offline qualification does not authorize publication. Commit, push, and OpenBench require
explicit authorization in the current request or active `/goal`; authorization for one stage
does not imply another. Publish from an unmerged `sw-XX-<slug>` branch without rewriting the
tested revision, and follow the [OpenBench guide](openbench.md). An upper-bound result may be
integrated, a lower-bound result rejects, and a stopped or capped result is inconclusive until
explicitly disposed. Only an approved integrated revision updates the baseline table.

Put new evidence in `tools/measurements/output/sw-XX-<baseline>/`: untouched output in `raw/`,
one compact manifest in `meta/`, and optional analysis in `derived/`. Save a patch only when no
immutable commit represents the candidate. Completing one task never authorizes starting another.

The immutable audit anchor is `470a3d75c31a13e5f791dc0ee2476c6974be346d`. Sealed roots are
`search-001-470a3d7/`, `sa-02-470a3d7/`, `sa-03-470a3d7/`,
`sa-03-capture-history-b415f5a/`, `sa-04-470a3d7/`, `sa-05-470a3d7/`,
`sa-05-verifier-25bb133/`, and `sa-07-470a3d7/`; do not modify them. Retrieve the deleted
audit document from commit `8c0d46d` only for forensic replay.

## Completed experiments

| Task | Candidate | Decisive result | Disposition and evidence |
|---|---|---|---|
| SW-01 [rejected] | Aspiration window 50 -> 32 cp | Corrected performance node ratio 1.1227, median 1.1512; 9 of 11 cases regressed. | Retain 50 cp. `tools/measurements/output/sw-01-6767e74/` |
| SW-02 [rejected] | Exact lookup table for the existing LMR formula | Search signatures and 6,068,328-node benchmark were unchanged, but 7 same-core pairs yielded one win and median NPS -1.767%. | Keep the formula. `tools/measurements/output/sw-02-cef892a/` |
| SW-03 [done; capped inconclusive] | Quiet LMR divisor 2.5 -> 2.4 | Offline corpus node ratio 0.9736 and repeatable 6,609,227-node candidate benchmark. STC `[0,3]` OpenBench #21 (`de77ab7` vs `72d2c88`) stopped at 8,018 games: LLR +0.9037 inside the +/-2.9444 bounds, +4.42 +/-5.57 Elo, no crashes or time losses. | Not integrated; main remains 2.5. Preserve branch `sw-03-quiet-lmr`, test #21, `Media/PGNs/21.pgn.tar`, and `tools/measurements/output/sw-03-72d2c88/`. |

## Reference review

The next queue was derived from Latrunculi's own gaps after reviewing local snapshots of Stockfish
`86f1df7134fa`, Ethereal `0e47e9b67f34`, Minic `4317c14559aa`, and CPW-Engine
`2e3cf29ab0a7`. These engines provide independent examples of mechanisms, not code or tuning
constants to transplant. Each candidate must fit Latrunculi's evaluation scale, TT policy, move
picker, node types, and evidence loop.

The strongest common gap is that Latrunculi already computes a NonPV static evaluation but does
not use it to screen null-move attempts or to make a shallow high-side cutoff. Ethereal and Minic
also confirm reduced PV fail-highs at full depth before paying for a full-window re-search. A
Latrunculi-specific review found that bulk futility skipping can discard later checking quiets.

Defer ProbCut, singular extensions, correction history, TT static-eval storage, and broad
late-move packages: they need more search state or coupled machinery than the next experiments
justify. Also defer IIR until Latrunculi represents cut/all-node roles. Severe main-search
SEE-negative pruning, history-gated killer/counter hints, delayed aspiration, and TT prefetch are
reasonable later candidates, but are not queued while the smaller questions below remain open.

## Candidate queue

No experiment is active. Activate and fully predeclare only one task at a time against the
then-current baseline; the order below is the current priority, not authorization to execute it.

### SW-04 — Gate null-move searches by static evaluation [rejected]

Hypothesis / candidate: null probes with `static_eval < beta` are usually wasted work. Add only
`static_eval >= beta` to the existing NonPV null-move eligibility test; preserve every other NMP
guard, depth, and reduction rule.

Baseline / panels / stops: task HEAD `08e5c2f`, operational search baseline `6767e74`, benchmark
6,068,328 nodes, artifacts `tools/measurements/output/sw-04-08e5c2f/`. Screen with the focused
mate/material root guards, one fresh-process depth-10 pass over all 200 corpus positions, and one
benchmark. Reject on a correctness or invalid-output failure, or when corpus geometric-mean nodes
are at least 5% worse with at least 120 positions regressing. Qualification adds the complete
`release-dev` suite, a second exact corpus pass, repeated objective guards, and a repeatable
benchmark. Sanitizers, `release-stats`, timing, and sentinels are `N/A`: this adds one scalar
condition without changing storage, arithmetic, concurrency, hot-path evaluation, or root control.

Result / artifacts: Screen stopped in the focused suite before corpus collection. The candidate
twice prevented `SearchTest.NullMoveReenablesAfterARealDescendantMove` from observing its expected
descendant null search; the second run used a deliberately low window and still failed. The other
seven focused null-move and objective guards passed. Evidence and the exact rejected patch are in
`tools/measurements/output/sw-04-08e5c2f/`.

Disposition / next boundary: rejected; retain the existing NMP eligibility and restore the
`6767e74` search baseline before activating SW-05.

### SW-05 — Add conservative reverse futility pruning [pending]

At shallow NonPV, non-check, non-mate-window nodes, test one high-side static-evaluation cutoff
before NMP. Reuse native evaluation units and existing safety guards; predeclare one depth bound,
margin schedule, and fail-hard/fail-soft return before implementation rather than importing them
from a reference engine.

### SW-06 — Confirm reduced PV fail-highs with a full-depth scout [pending]

Keep the LMR formula unchanged. When a reduced PV scout exceeds alpha, first confirm it with the
ordinary full-depth null-window search; run the existing full-window PV re-search only if that
confirmation still exceeds alpha. Measure whether avoided full-window searches outweigh the
extra confirmation scouts.

### SW-07 — Preserve later checking quiets under futility [pending]

The current futility path skips the entire remaining quiet stage after one nonchecking quiet.
Test per-move skipping so later checking quiets remain searchable, with a focused regression that
places such a check behind an earlier quiet. Treat this primarily as a move-quality correction:
predeclare a looser node-cost stop and let paired games decide a correct, reproducible survivor.
