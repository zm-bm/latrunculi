# Search Development

This document coordinates `SW-XX` search experiments. It holds the operational baseline,
durable findings, workflow, and queue; search work is not duplicated in `docs/roadmap.md`.

## Goal and baseline

Improve useful depth and convergence by searching fewer nodes per useful ply and more nodes per
second, without sacrificing correctness or move quality. Reported depth alone is not an
optimization target.

Update these fields together only after an approved candidate is integrated:

| Operational field | Current value |
|---|---|
| Search-behavior baseline | `c6eb5547ac5e883d07be4ff560a8b13820a4d415` (`c6eb554`) |
| Deterministic benchmark | 5,168,111 nodes |
| Cached corpus baseline | `tools/measurements/output/search-baseline-c6eb554/` |

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

Cross-engine depth and NPS are context only because engines count reduced, extended, and
quiescence work differently. Within Latrunculi, fixed-depth nodes measure selectivity; repeated
timing with exact search signatures measures throughput; objective guards and paired games decide
whether a changed tree is useful.

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
| Futility | Guarded reverse futility pruning passed OpenBench and was retained in `8e64048`. Main-search futility now preserves its quiet-check exemption by filtering only nonchecking quiets after activation; SW-08 was neutral at 10,426 games and retained by explicit correctness policy. |
| Historical depth gap | In the pinned release gauntlet PGNs under `tools/measurements/output/search-001-470a3d7/sources/`, Latrunculi's median reported depth was 12 versus 16 for 4ku, 16 for Willow, and 18 for Weiss. The repeated 4-6-ply gap motivates both selectivity and throughput work, but the old revision and engine-specific depth accounting make it neither a current benchmark nor an acceptance gate. |
| Open mechanisms | Null move remains eligible for a materially different small experiment; the audit lacked eligibility and counterfactual-failure denominators, while SW-04 rejected only the tested `static_eval >= beta` gate. |

The operational `c6eb554` sentinel horizons/useful depths are `startpos` 14/11,
`arasan20-16` 16/16, `pilot14-g171-abrupt` 18/14, and `pilot15-g078-secondary` 13/13.
Use the candidate rows in `sw-08-6c01040/derived/sentinel-summary.tsv`; the earlier
`sa-02-470a3d7/derived/resolved-horizons.tsv` rows remain immutable audit evidence, not the
current comparison baseline.

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
tested revision, and follow the [OpenBench guide](openbench.md). Predeclare the profile and
OpenBench-enforced game budget; a larger-than-default budget needs explicit approval, and a
missing cap blocks submission. Confirm the test once and return control. Reaching the cap inside
both bounds is inconclusive; further testing needs separate justification and authorization.
Only an approved integrated revision updates the baseline table.

Put new evidence in `tools/measurements/output/sw-XX-<baseline>/`: untouched output in `raw/`,
one compact manifest in `meta/`, and optional analysis in `derived/`. Save a patch only when no
immutable commit represents the candidate. Completing one task never authorizes starting another.

The immutable audit anchor is `470a3d75c31a13e5f791dc0ee2476c6974be346d`. Sealed roots are
`search-001-470a3d7/`, `sa-02-470a3d7/`, `sa-03-470a3d7/`,
`sa-03-capture-history-b415f5a/`, `sa-04-470a3d7/`, `sa-05-470a3d7/`,
`sa-05-verifier-25bb133/`, and `sa-07-470a3d7/`; do not modify them. Retrieve the deleted
audit document from commit `8c0d46d` only for forensic replay.

## Queue

Priorities reflect Latrunculi's evidence plus local Stockfish `86f1df7134fa`, Ethereal
`0e47e9b67f34`, Minic `4317c14559aa`, CPW-Engine `2e3cf29ab0a7`, and
[Chess Programming Wiki](https://chessprogramming.org/) reviews. Borrow mechanisms, not code or
tuning constants. The queue favors small selective-search changes first, then behavior-preserving
throughput work, then bounded adaptive pruning. Do not interpret a higher nominal depth as a pass.

### SW-09 — Extend reverse futility pruning to depth 4 [active]

Hypothesis / candidate: the retained shallow RFP can safely remove more clearly high fail-high
nodes. Give RFP its own depth-4 limit and margins `{0, 250, 400, 550, 700}`; leave ordinary
futility at depths 1-3 and preserve every existing RFP guard and return value.

Baseline / panels / stops: task HEAD `cbb6a683fbcc33b40298c1399fb427d35083edb5`, operational
search baseline `c6eb554`, benchmark 5,168,111 nodes, cached corpus
`search-baseline-c6eb554/`, and artifacts `sw-09-c6eb554/` beneath the measurements output.
Screen with focused RFP and objective guards, one 200-position depth-10 corpus pass, all four
current sentinel trajectories, and one benchmark. Reject on a correctness, determinism, or
output failure; useful-depth loss in at least two sentinels; or corpus geometric-mean nodes at
least 1.05 times baseline with at least 120 regressions. Root/score changes and marginal
efficiency alone do not reject a correct candidate. Qualification adds the complete
`release-dev` and ASan/UBSan suites, a second exact corpus pass, and repeated guards, sentinels,
and benchmark. `release-stats`, separate timing, and TSan are `N/A`: no counters, new hot-path
work, or concurrency are involved.

Result / artifacts: not run.

Disposition / next boundary: active; implement and run Screen only when requested.

### SW-10 — Order quiet checks first at futility nodes [pending]

When node-wide futility is active, stage checking quiet hints and generated quiet checks before
ordinary quiets, then retain the SW-08 bulk nonchecking-quiet filter. Change no eligibility,
margin, or searched checking move; cache check classification and test the first-legal interaction.

### SW-11 — Prune severe depth-1 SEE-losing captures [pending]

At depth-1 NonPV nodes, after one legal move, skip only non-TT, nonpromotion, nonchecking captures
with `board.see(move) < -eval::pawn.mg`. Exempt check, mate windows, and in-check nodes; leave
qsearch and the picker SEE bands unchanged. Include repeated timing because this may add hot-path
SEE work.

### SW-12 — Add conservative depth-1 late-move pruning [pending]

When ordinary futility is inactive, suppress remaining ordinary nonchecking quiets only after
eight legal moves have been searched. Preserve TT moves, killers, countermoves, captures,
promotions, checks, mate safety, and the first legal move; add no history or improving modifier.

### SW-13 — Prefetch child transposition-table clusters [pending]

Add behavior-preserving child-key prefetching without changing TT layout or policy. Require exact
baseline search signatures and a repeatable same-core throughput gain; dispose of it offline if
either condition fails, and do not use OpenBench unless behavior unexpectedly changes.

### SW-14 — Enable release link-time optimization [pending]

Enable supported CMake interprocedural optimization for production release builds, including the
OpenBench adapter, without adding architecture flags or changing debug and sanitizer builds.
Require exact baseline search signatures, successful GCC and Clang builds, and a repeatable
same-core throughput gain; reject it offline on a build, portability, signature, or timing failure.

### SW-15 — Remove atomic work from per-node accounting [pending]

Replace the per-node atomic increment with worker-owned counting, race-free periodic publication,
and exact publication at completion. Preserve the main worker's polling cadence, exact final
counts, concurrent UCI progress, and a bounded node-limit overshoot. Require exact fixed-depth
signatures, repeated 1/2/4-thread timing, focused limit/lifecycle checks, and TSan.

### SW-16 — Reduce clearly bad-history quiets further [pending]

Add one extra reduction ply only to already-LMR-eligible NonPV quiets that are nonchecking,
nonpromotion, nonkiller, and below one predeclared negative combined-history threshold. Preserve
the base LMR formula, existing reduction bounds, and all other moves; test one threshold, not a
sweep. This is distinct from the rejected blanket divisor and high-history protection shapes.

### SW-17 — Make null-move reduction adaptive [pending]

Keep the current eligibility, material guard, TT veto, and SW-04 behavior. Replace only the fixed
3/4-ply reduction with one bounded formula based on depth and clamped static-evaluation surplus
over beta; add no new eligibility gate or verification search. Require objective and convergence
guards plus paired strength validation for any offline survivor.

Do not queue broader machinery merely to raise displayed depth. After these tasks, use one bounded
sampling profile to decide whether full evaluation, repetition detection, make/unmake, or TT work
justifies one concrete optimization; transfer a broader architectural result to `docs/roadmap.md`.
Introduce an `improving` flag only with a specific LMP, LMR, or RFP candidate. Defer IIR, ProbCut,
singular extensions, evaluation caching, and incremental state until simpler evidence identifies
the need and validates their added complexity.

## Completed experiments

Full SW-04–SW-08 records remain in commit `cbb6a68`; the paths below retain their evidence.

| Task | Candidate | Decisive result | Disposition and evidence |
|---|---|---|---|
| SW-01 [rejected] | Aspiration window 50 -> 32 cp | Corrected performance node ratio 1.1227, median 1.1512; 9 of 11 cases regressed. | Retain 50 cp. `tools/measurements/output/sw-01-6767e74/` |
| SW-02 [rejected] | Exact lookup table for the existing LMR formula | Search signatures and 6,068,328-node benchmark were unchanged, but 7 same-core pairs yielded one win and median NPS -1.767%. | Keep the formula. `tools/measurements/output/sw-02-cef892a/` |
| SW-03 [done; manually stopped/inconclusive] | Quiet LMR divisor 2.5 -> 2.4 | Offline corpus node ratio 0.9736 and repeatable 6,609,227-node candidate benchmark. STC `[0,3]` OpenBench #21 (`de77ab7` vs `72d2c88`) stopped at 8,018 games: LLR +0.9037 inside the +/-2.9444 bounds, +4.42 +/-5.57 Elo, no crashes or time losses. | Not integrated; main remains 2.5. Preserve branch `sw-03-quiet-lmr`, test #21, `Media/PGNs/21.pgn.tar`, and `tools/measurements/output/sw-03-72d2c88/`. |
| SW-04 [rejected] | Require `static_eval >= beta` before null-move search | The focused descendant-null regression failed twice, including with a deliberately low window, before corpus measurement. | Retain existing null-move eligibility. `tools/measurements/output/sw-04-08e5c2f/` |
| SW-05 [done] | Guarded reverse futility pruning at NonPV depths 1-3 | Offline corpus node ratio 0.7855 with exact repeats and a repeatable 4,697,330-node benchmark. STC `[0,3]` OpenBench #22 (`8e64048` vs `02a9537`) passed at 2,966 games: LLR +2.9698, +31.01 +/-9.19 Elo, no crashes or time losses. | Integrated and retained beneath the current SW-08 baseline. Preserve branch `sw-05-reverse-futility`, test #22, `Media/PGNs/22.pgn.tar`, and `tools/measurements/output/sw-05-02a9537/`. |
| SW-06 [done; manually stopped/inconclusive] | Confirm reduced PV fail-highs with a full-depth scout | Offline corpus node ratio 0.9949, exact repeats, no sentinel regression, and a repeatable 4,345,849-node benchmark. OpenBench #23 (`2d52595` vs `1b2b8fc`) was stopped at 8,676 scored games: LLR +1.1808 inside the bounds, +5.29 +/-5.54 Elo, no crashes or time losses. | Not integrated because the test did not cross its predeclared upper bound. Preserve branch `sw-06-pv-lmr-confirmation`, test #23, and `tools/measurements/output/sw-06-1b2b8fc/`. |
| SW-07 [rejected] | Prune qualifying quiets individually so later checking quiets remain searchable | The focused regression was fixed and all tests passed, but corpus nodes rose 5.39% geometrically and the `pilot14-g171-abrupt` trajectory lost useful depth from 17 to 14; aggregate late root changes and zero-prefix transitions each increased by one. | Rejected by the predeclared convergence stop before OpenBench. Retain the picker-wide skip and preserve `tools/measurements/output/sw-07-b8a5335/`. |
| SW-08 [done; policy-retained] | Filter nonchecking quiets after futility while preserving later quiet checks | Corpus nodes rose 5.34%, benchmark nodes 10.02%, and `pilot14-g171-abrupt` useful depth fell 17 to 14; NPS was neutral. `[-3,0]` OpenBench #25 stopped at 10,426 games inside its bounds: LLR +0.1433, -0.70 +/-4.93 Elo, no crashes or time losses. | Retained to fix the quiet-check exemption despite an inconclusive SPRT; follow-up is SW-10. Preserve branch `sw-08-futility-quiet-checks`, test #25, `Media/PGNs/25.pgn.tar`, and `tools/measurements/output/sw-08-6c01040/`. |
