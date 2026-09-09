# Search Development

This is the source of truth for `SW-XX` search experiments. It retains the
completed audit findings that constrain future work, defines the experiment
workflow, and tracks the active queue. `SW-XX` experiments are not duplicated
in `docs/roadmap.md`.

## Objective

Improve useful depth and convergence stability through better selectivity
without sacrificing correctness, move quality, or wall-clock performance.
Reported depth alone is not an optimization target.

## Baselines and suite

Update these two operational fields together after an approved retained
candidate:

- Operational search-behavior baseline:
  `6767e7447bfa285bef1daeea30d62f8a69367c42` (`6767e74`).
- Deterministic baseline benchmark: 6,068,328 nodes.

These references remain fixed during ordinary experiments:

- Immutable comparison anchor: `470a3d75c31a13e5f791dc0ee2476c6974be346d`
  (`470a3d7`).
- Regression foundation: `search_measurement_v3`, the retained cases in the
  measurement guide, and the sealed artifacts beneath
  `tools/measurements/output/`.

When the operational baseline advances, reuse the accepted candidate's
validated dashboard and trajectory rows as the next preserved baseline. Rerun
them only when the workload or measurement definition changes.

The [measurement guide](../tools/measurements/README.md) lists the retained case
IDs and command interface. The objective predicates are unique `h8h1#`, either
`d3c2` or `d3c3` forcing mate in three plies, and unique maximum material gain
`d1e2` capturing the loose rook.

## Working terms

- **Reproducibility:** fresh-process, one-thread fixed-node repetitions agree
  exactly in completed depth, score, actual nodes, best move, and PV; timing and
  NPS are excluded.
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

## Experiment lifecycle

Tasks use sequential `SW-XX` IDs. At most one task is active at a time.

| State | Meaning |
|---|---|
| `pending` | Queued but not started. |
| `active` | In progress or waiting at an approval boundary. |
| `done` | Retained after all validation required by the task. |
| `rejected` | Tested but not retained. |
| `skipped` | Closed without testing a candidate. |

Each experiment has up to three stages:

1. **Offline:** implement, test, measure, and update the task record.
2. **Publish:** when explicitly authorized, commit and push the reviewed
   result. A behavior candidate proceeding to OpenBench must be published from
   a task branch named `sw-XX-<slug>`.
3. **External validation:** submit an OpenBench workload when explicitly
   authorized. Once submitted, read-only monitoring and result retrieval need
   no further approval.

Keep a published OpenBench candidate branch unmerged while external validation
is in progress. Its published or tested revision is immutable: never amend,
reset, force-push, or otherwise rewrite it. If the candidate is retained,
integrate the tested patch only when authorized and record both the tested
revision and the integration revision when their hashes differ. If integration
changes behavior-affecting content, treat it as a new revision and rerun the
affected validation. If the candidate is rejected, publish its disposition and
artifact reference in an authorized follow-up commit; then abandon and, when
authorized, delete the unmerged task branch. Remove an already integrated
candidate with a new authorized commit, not by rewriting history.

An active request or `/goal` may conditionally authorize multiple named stages;
otherwise stop at the next boundary. Completing or rejecting one task does not
authorize activating the next.

A task may test a small established search mechanism or tune one existing
parameter when it has a concrete hypothesis and a reversible candidate; it no
longer requires the audit to isolate that mechanism first. Change one mechanism
per candidate, dispose of it before opening the next task, and use paired games
rather than the audit suite to decide playing strength.

Before implementation, record the hypothesis, operational baseline, sole
candidate shape, offline checks, task-specific offline stop conditions,
artifact path, and intended external-validation path. A stop condition may be
`N/A` with a reason. Define any task-specific use of terms such as `material`
or `severe` before measuring. Run only the predeclared checks, and add
diagnostics only for a named decision that existing output cannot support. Do
not add cases, counters, or diagnostics afterward merely to rescue, reject, or
fully explain an otherwise decided candidate; record an unexpected signal for
later work. Do not turn a parameter experiment into an instrumentation project.
Freeze the task's `Baseline` field when it becomes active; later
operational-baseline updates do not rewrite historical records.

Search results may legitimately change. Baseline/candidate agreement in score,
move, PV, nodes, completed depth, or convergence metrics is not required. What
must remain exact is fresh-process reproducibility within the candidate under
fixed-node, one-thread conditions.

## Offline safety gates

Every behavior candidate must:

1. Pin the operational baseline and change one mechanism only.
2. Pass focused tests and the complete configured CTest suites for the
   `release-dev` and `debug-asan-ubsan` presets. Build `release-stats` and run a
   focused search smoke test; run its complete suite only when the candidate or
   measurement path changes search statistics or instrumentation.
3. Produce three identical candidate signatures for every retained case. Each
   run uses a fresh `release-dev` process, one thread, 32 MiB Hash, a 524,288
   node limit, and one internal repetition. Exact fields are completed depth,
   score, actual nodes, best move, and PV; timing and NPS are excluded. The
   signature need not match the baseline.
4. Preserve legal search and protocol behavior and lose no pinned mate or
   material objective. A worse mate result is a hard failure.
5. Produce the same candidate benchmark fingerprint twice. Record a legitimate
   new fingerprint rather than forcing 6,068,328 nodes.
6. Avoid a material throughput regression unless the candidate has a clear
   compensating search-quality benefit. Any added hot-path work requires an
   explicit wall-time or NPS comparison.

A violation of gates 1-5 is a common hard failure. Gate 6 becomes a hard
failure only at the task's predeclared threshold.

Run offline work as a funnel, ordering the predeclared checks from cheapest to
most expensive. Stop once a rejection condition fires; partial evidence is
sufficient for rejection, while an infrastructure failure leaves the task
active and incomplete. Complete the broad suites only for a candidate still
eligible for retention or publication.

Use the triplicated fixed-node rows as the common all-case advisory dashboard
and verify the objective guards at their pinned depths. Run one fresh
fixed-depth candidate search for every retained case only when the task claims
selectivity or efficiency, or when a stop condition uses node efficiency. Use
the case horizons in
`tools/measurements/output/sa-02-470a3d7/derived/resolved-horizons.tsv` and
compare nodes with the preserved baseline.

Reconstruct complete fresh-process depth trajectories only when the candidate
targets iterative deepening, convergence, or solution depth, or when the task
predeclares a lightweight-dashboard trigger. Limit them to the listed
convergence sentinels and objective guards unless that trigger explicitly names
another case. Report only the applicable panels. Modest regressions and changed
search results do not reject a candidate by themselves. Stop offline only for
a common hard failure or a predeclared task-specific stop condition. The suite
is a smoke-test dashboard, not a strength oracle.

For a trajectory ending at depth `D`, run every depth `1..D` in its own fresh
process; rows are cumulative, and baseline and candidate use the same `D`.
First solution depth is the first objective match. Stable solution depth is the
first match whose complete suffix through `D` also matches. “Late” is
`D-2..D`. PV survival is `LCP / min(PV lengths)`; exclude mate transitions from
centipawn jump and sign-flip statistics, collapse consecutive equal roots
before counting A-B-A, and use nearest-rank p90.

A candidate intended to preserve behavior may finish without OpenBench only
when every required baseline/candidate search signature agrees exactly and its
predeclared repeatable throughput gate passes. Any signature change makes it a
behavior candidate and requires the strength workflow.

## Strength decision

For a behavior candidate that passes the offline safety gates and has no severe
advisory red flag, test the exact published `sw-XX-<slug>` revision against the
pre-change revision with one paired STC SPRT profile predeclared from the
[OpenBench guide](openbench.md). An upper-bound result completes that test and
qualifies the candidate for approved integration unless confirmation is
required. Require a separate confirmation only when the task predeclared it,
when candidate-selection risk or the change's scope justifies it, or when the
result conflicts with other evidence; confirmation requires separate
authorization.
Only an approved candidate that has passed every required test and been
integrated becomes the next operational baseline. Dispose of a lower-bound
candidate under the branch policy above. Record an inconclusive result without
calling it accepted, and keep the task active until it receives a disposition.

Record the test ID, SPRT profile, both revisions, benchmark fingerprints,
OpenBench revision, decision, and PGN artifact. The immutable `470a3d7`
comparison anchor remains unchanged as operational baselines advance.

## Evidence and disposition

The sealed audit evidence roots are:

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

Do not append ordinary `SW-XX` artifact roots to the sealed list. Each task
record owns its current artifact path.

Put new evidence in
`tools/measurements/output/sw-XX-<operational-baseline>/`, with provenance in
`meta/`, untouched process output in `raw/`, and ignored aggregation material
in `derived/`. The suffix is the abbreviated retained search-behavior commit;
`meta/` pins the full executable and tool revision plus any candidate patch.

A rejected experiment is a useful result. Preserve its referenced evidence and
patch, then remove behavior code, temporary diagnostics or toggles, redundant
tests, and exact candidate-only build output before opening another task.
Distill only the decision-relevant result here.

After a task reaches a terminal state, retain its ID, result, artifact path,
and durable disposition. Fold any reusable lesson into the findings table and
remove repeated plans or procedural detail; Git history retains the full
working record.

Use this compact record for each task:

```text
SW-XX — title [pending|active|done|rejected|skipped]
Hypothesis:
Baseline:
Candidate:
Measurements:
Offline stop conditions:
External validation:
Result:
Artifact path:
Disposition:
```

## Active and queued experiments

### SW-01 — Narrow the aspiration window [active]

- Hypothesis: narrowing the initial root aspiration window from 50 to 32 cp
  saves enough work on successful searches to outweigh additional retries.
- Baseline: `6767e7447bfa285bef1daeea30d62f8a69367c42`, benchmark 6,068,328
  nodes.
- Candidate: change only `AspirationWindow` from 50 to 32. Add no diagnostics or
  per-node work.
- Measurements: not started. Run the common gates, the retained-suite
  fixed-depth comparison, and the predeclared convergence-sentinel trajectories.
- Offline stop conditions: reject for any common hard failure, a candidate to
  baseline fixed-depth geometric-mean node ratio above 1.03 across the retained
  suite, or a late score-sign flip or A-B-A absent from baseline in at least two
  convergence sentinels. An NPS threshold is `N/A` because this constant-only
  candidate adds no per-node work.
- External validation: if offline-qualified, stop for publish approval and then
  obtain authorization for the incremental SPRT profile in the OpenBench guide.
- Result: pending.
- Artifact path: `tools/measurements/output/sw-01-6767e74/`.
- Disposition: active; documentation only, with no implementation or tests yet.

### SW-02 — Precompute LMR reductions [pending]

Replace hot-loop logarithms with a precomputed reduction table without
intentionally changing search behavior. Require exact baseline/candidate search
signatures and a predeclared repeatable throughput improvement; keep this
separate from LMR parameter tuning. It may finish after offline validation
without OpenBench. If any required signature changes, reclassify it as a
behavior candidate before proceeding.

### SW-03 — Tune quiet LMR selectivity [pending]

Test one conservative quiet-only step, initially divisor 2.5 to 2.4. Leave
noisy moves, the fourth-move threshold, and history handling unchanged. Do not
retry the previously rejected high-history protection or revert the earlier
fourth-move stabilization as part of this task.

After these experiments, select at most one small next mechanism from current
code and game evidence. Do not open it as a task until the preceding candidates
have dispositions. Continue retaining recurring self-play or release failures
as contextual positions, but do not require a new diagnostic audit before a
reversible production experiment.
