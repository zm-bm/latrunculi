# Search Development

This is the live coordination document for search-algorithm work. It contains
the durable results of the completed search audit and the contract for future
experiments. Search work is coordinated here rather than in `docs/roadmap.md`.

## Objective and current state

Improve useful depth and convergence stability through better selectivity
without sacrificing correctness, move quality, or wall-clock performance.
Reported depth alone is not an optimization target.

- Immutable comparison anchor: `470a3d75c31a13e5f791dc0ee2476c6974be346d`
  (`470a3d7`).
- Accepted search and evaluation behavior still matches that anchor.
- Deterministic baseline benchmark: 6,068,328 nodes.
- Regression foundation: `search_measurement_v3`, eleven original positions,
  three independently verified objective guards, and the sealed artifacts
  beneath `tools/measurements/output/`.
- Current status: no behavior candidate was retained and no search experiment
  is active.

The [measurement guide](../tools/measurements/README.md) lists the fourteen case
IDs and command interface. The objective predicates are unique `h8h1#`, either
`d3c2` or `d3c3` forcing mate in three plies, and unique maximum material gain
`d1e2` capturing the loose rook.

## Working terms

- **Reproducibility:** fresh-process, one-thread fixed-node repetitions agree
  exactly in completed depth, score, actual nodes, best move, PV, and applicable
  counters; timing and NPS are excluded.
- **Convergence stability:** later accepted depths settle instead of repeatedly
  changing root move, score sign, or PV.
- **Useful depth:** the greatest measured depth `d >= 3` for which depths
  `d-2..d` share one root move and score class and both adjacent PV transitions
  have a nonzero common prefix. Objective predicates must also hold throughout
  that window.
- **Selectivity:** fewer nodes reach the same accepted depth or solution, or
  greater useful depth is reached at the same node budget.
- **Objective quality:** a source-pinned mate or material criterion verified
  independently of engine preference. Pilot positions and Arasan labels are
  context, not correctness oracles.

## Durable audit findings

| Area | Evidence | Decision |
|---|---|---|
| Reproducibility and Hash | All 33 fixed-node triplicate groups were exact, dev/stats signatures agreed, and 32 versus 256 MiB Hash changed no result. | No run-to-run instability or normal-Hash defect was found. |
| Pilot and control roots | Three of four primary historical pilot moves were not reproduced at 524,288 nodes, and the cases exposed no shared counter profile. | Keep them as convergence stress and context; they do not identify a mechanism or correct move. |
| Objective guards | Mate in one was stable at depth 1 and 46 nodes; mate in two at depth 3 and 767 nodes; rook capture at depth 1 and 29 nodes. | Retain these as minimum correctness guards, not a complete quality suite. |
| Convergence | The fourteen-case baseline had five late root changes, one sign flip, one A-B-A, a 46 cp late-jump p90, 61.5% median PV survival, and five zero-prefix transitions. Four cases ended below their measured horizon in useful depth; no objective case regressed. | Treat depth-to-depth movement as convergence behavior, distinct from nondeterminism. |
| Capture ordering | Ordinal-2-or-later captures succeeded 29,374 times in 504,598 attempts (5.82%): 7.20% in the exact-SEE-good stage and 0.47% in exact-SEE-bad. The tested CaptureHistory table produced a 0.9716 fixed-depth node ratio but a 1.0646 wall-time ratio; start position took 1.4129 times as long. | Within-stage ordering opportunity exists, but the exact CaptureHistory candidate was rejected and removed. Preserve the SEE bands in future work. |
| Qsearch margin pruning | Ordinary exact-SEE-negative captures are already excluded. The 200, 300, and 400 cp shadows would have skipped 3,200, 732, and 206 real NonPV beta cutoffs. | Do not retry merely adding SEE pruning or test those exact margin rules. |
| History-aware LMR | All 107 sampled high-history reduced fail-lows remained fail-lows at unreduced depth. | Do not test the exact one-ply protection for quiet moves with combined history at least 1024 without new evidence. |
| Game-clock soft stop | Unfinished iterations consumed median shares of 24-26%, but all 210 timed searches matched their fresh-depth score, move, and PV. Multipliers 1, 2, and 4 fired prematurely 21, 65, and 129 times. | This is an efficiency issue, not nondeterminism. Do not test those exact predictors. |
| Null move and main futility | Existing counters lack eligibility and counterfactual-failure denominators that isolate a repeated mechanism. | Not disproved, but not supported as the next experiment. |

Use these four cases as focused convergence sentinels. `D / U` is the measured
trajectory horizon and useful depth:

| Case | D / U | Durable signal |
|---|---:|---|
| `startpos` | 14 / 11 | Late root change and 15.4% late PV survival. |
| `arasan20-16` | 16 / 14 | Two late root changes, transient `f1e1 -> d2e2 -> f1e1` A-B-A, and 0% late PV survival. |
| `pilot14-g171-abrupt` | 18 / 17 | The root changed from `f5f4` at 4M nodes to `h4h5` at 33M, with no common PV prefix between those endpoints. |
| `pilot15-g078-secondary` | 13 / 11 | Late score-sign flip and 7.7% late PV survival. |

The exact CaptureHistory implementation is preserved in
`tools/measurements/output/sa-03-capture-history-b415f5a/meta/candidate.patch`.
The rejected qsearch rule was
`stand_pat + captured_value + margin <= alpha_before_move` for margins 200, 300,
and 400 among eligible SEE-nonnegative NonPV captures, with tactical exemptions.
The rejected game-clock predictor was
`T_d + m * (T_d - T_(d-1)) >= allocated_time` for `m` in `{1, 2, 4}`.

Do not retry the rejected candidate shapes without materially new evidence.
The suite is small, and its clock measurements are single-threaded on one
machine. It establishes offline safety and efficiency evidence, not Elo.
Explicit `movetime` remains a hard-duration request; the clock result rejects
only the tested game-clock predictors. NonPV qsearch predominantly uses null
windows, so an improving move normally appears as a beta cutoff rather than an
alpha raise; zero observed alpha raises is not evidence that pruning is safe.
Aggregate counters also combine iterative-deepening attempts and aspiration
retries, and some counters overlap. Require mechanism-specific eligibility and
counterfactual denominators before treating a raw count as candidate evidence.

## Opening a search task

New tasks use sequential `SW-XX` IDs. Keep exactly one task active and do not
maintain a speculative mechanism wishlist. Open a task only for either:

- the same mechanism implicated repeatably in at least two independent cases;
  or
- one deterministic, objectively verifiable correctness failure such as an
  illegal move, missed mate, or material error.

Before collection, record the hypothesis, baseline, cases, missing measurement
or denominator, eligibility gate, sole permitted candidate shape, rejection
gate, and artifact path. Diagnostics must be behavior-neutral. Extend
`latrunculi-measure` or search instrumentation only for a named missing
measurement.

For a trajectory ending at depth `D`, run every depth `1..D` in its own fresh
process; rows are cumulative, and baseline and candidate use the same `D`.
First solution depth is the first objective match. Stable solution depth is the
first match whose complete suffix through `D` also matches. “Late” is
`D-2..D`. PV survival is `LCP / min(PV lengths)`; exclude mate transitions from
centipawn jump and sign-flip statistics, collapse consecutive equal roots
before counting A-B-A, and use nearest-rank p90.

## Candidate gates

Every behavior candidate must satisfy all of these:

1. Change one mechanism only and reproduce the 6,068,328-node baseline before
   the edit.
2. Pass three exact fresh-process repetitions over the retained fourteen-case
   suite at 524,288 nodes, 32 MiB Hash, and one thread.
3. Lose no objective solution, delay no stable solution, worsen no mate, and
   increase no objective nodes-to-stable-solution by more than 5%.
4. Do not increase aggregate late root changes, sign flips, A-B-A counts,
   late-jump p90, or zero-prefix transitions; do not reduce median PV survival;
   and add no full-trajectory or late A-B-A to an objective case.
5. Improve fixed-depth geometric-mean nodes by at least 1%, with at least two
   independent cases improving by 2%; alternatively, gain one useful ply in two
   cases without increasing geometric-mean nodes.
6. Add an explicit throughput or wall-time guard whenever the mechanism adds
   per-node work.
7. Pass focused tests, complete `release-dev` and `release-stats` suites,
   ASan/UBSan, the preserved SEARCH-001 matrix and SA-02 trajectory regression,
   and two repeatable candidate benchmarks. Record a legitimate candidate
   fingerprint instead of forcing the baseline value.

An offline-qualified candidate stops for review and explicit approval before
commit, push, or paired OpenBench validation. Only an approved retained
candidate becomes the operational baseline; `470a3d7` remains the immutable
comparison anchor.

## Evidence and disposition

The completed evidence roots are:

- `search-001-470a3d7/`;
- `sa-02-470a3d7/`;
- `sa-03-470a3d7/` and `sa-03-capture-history-b415f5a/`;
- `sa-04-470a3d7/`;
- `sa-05-470a3d7/` and `sa-05-verifier-25bb133/`; and
- `sa-07-470a3d7/`.

They are ignored directories beneath `tools/measurements/output/`, with exact
provenance in `meta/`, raw process output in `raw/`, and aggregation code and
tables in `derived/`. Treat them as immutable and do not mix future output into
them. Some sealed historical harnesses pin the deleted `docs/search-audit.md`;
use their frozen outputs and manifests as evidence, or retrieve that document
from commit `8c0d46d` for an exact forensic replay. New harnesses coordinate
against this document.

Put new evidence in
`tools/measurements/output/sw-XX-<operational-baseline>/`, with provenance in
`meta/`, untouched process output in `raw/`, and ignored aggregation material
in `derived/`. The suffix is the abbreviated retained search-behavior commit;
`meta/` pins the full executable and tool revision plus any candidate patch.

A rejected experiment is a useful result. Preserve its referenced evidence and
patch, then remove behavior code, temporary diagnostics or toggles, redundant
tests, and exact candidate-only build output before opening another task.
Distill only the decision-relevant result here.

Use this compact record for each task:

```text
SW-XX — title [active|done|rejected|skipped]
Hypothesis:
Baseline:
Candidate:
Measurements:
Result:
Artifact path:
Disposition:
```

## Next entry

There is no active experiment. The next evidence intake should:

1. add a small source-pinned objective set beyond the current simple mate and
   immediate-material guards; and
2. retain recurring positions from new self-play or release failures with the
   FEN, side to move, historical sequence, and selection reason.

Activate the first `SW-XX` task only when that evidence satisfies the entry
gate and identifies one mechanism to diagnose.
