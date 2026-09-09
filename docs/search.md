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
- Operational baseline: `6767e7447bfa285bef1daeea30d62f8a69367c42`
  (`6767e74`); accepted search and evaluation behavior still matches the anchor.
- Deterministic baseline benchmark: 6,068,328 nodes.
- Regression foundation: `search_measurement_v3`, eleven original positions,
  three independently verified objective guards, and the sealed artifacts
  beneath `tools/measurements/output/`.
- Current status: `SW-01` is active. Its documentation boundary is established;
  implementation and testing have not started.

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

## Production experiment loop

New tasks use sequential `SW-XX` IDs, with exactly one active at a time. A task
may test a small established search mechanism or tune one existing parameter
when it has a concrete hypothesis and a reversible candidate; it no longer
requires the diagnostic audit to isolate that mechanism first. Change one
mechanism per candidate, dispose of it before opening the next task, and use
paired games rather than the audit suite to decide playing strength.

Before implementation, record the hypothesis, operational baseline, sole
candidate shape, offline checks, artifact path, and intended OpenBench test.
Add diagnostics only for a named decision that existing output cannot support.
Do not turn a parameter experiment into an instrumentation project.

Search results may legitimately change. Baseline/candidate agreement in score,
move, PV, nodes, completed depth, or convergence metrics is not required. What
must remain exact is fresh-process reproducibility within the candidate under
fixed-node, one-thread conditions.

## Offline safety gates

Every behavior candidate must:

1. Pin the operational baseline and change one mechanism only.
2. Pass focused tests, complete `release-dev` and `release-stats` suites, and
   ASan/UBSan.
3. Produce three identical fresh-process, one-thread fixed-node signatures for
   the candidate; the signature need not match the baseline.
4. Preserve legal search and protocol behavior and lose no pinned mate or
   material objective. A worse mate result is a hard failure.
5. Produce the same candidate benchmark fingerprint twice. Record a legitimate
   new fingerprint rather than forcing 6,068,328 nodes.
6. Avoid a material throughput regression unless the candidate has a clear
   compensating search-quality benefit. Any added hot-path work requires an
   explicit wall-time or NPS comparison.

Run the retained fourteen-case suite once as an advisory baseline/candidate
report. Record fixed-depth nodes, fixed-node completed and useful depth, root
move, score, PV, and the established late-convergence metrics. Modest
regressions and changed search results do not reject a candidate by themselves.
Stop offline only for a correctness failure, candidate nondeterminism, a
material throughput loss, or broad and severe convergence collapse. The suite
is a smoke-test dashboard, not a strength oracle.

For a trajectory ending at depth `D`, run every depth `1..D` in its own fresh
process; rows are cumulative, and baseline and candidate use the same `D`.
First solution depth is the first objective match. Stable solution depth is the
first match whose complete suffix through `D` also matches. “Late” is
`D-2..D`. PV survival is `LCP / min(PV lengths)`; exclude mate transitions from
centipawn jump and sign-flip statistics, collapse consecutive equal roots
before counting A-B-A, and use nearest-rank p90.

## Strength decision

When the offline safety gates pass and the advisory report has no severe red
flag, stop for approval to commit and push the candidate. Screen it against the
pre-change revision with the standard paired OpenBench STC workload and
normalized-Elo SPRT `[0, 5]`. A candidate that reaches the upper bound proceeds
to `[0, 3]` confirmation; only an approved confirmed candidate becomes the next
operational baseline. A lower-bound candidate is rejected and removed. Record
inconclusive results without calling the candidate accepted.

Record the test ID, both revisions, benchmark fingerprints, OpenBench revision,
decision, and PGN artifact. The immutable `470a3d7` comparison anchor remains
unchanged as operational baselines advance.

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

## Active and queued experiments

### SW-01 — Narrow the aspiration window [active]

- Hypothesis: narrowing the initial root aspiration window from 50 to 32 cp
  saves enough work on successful searches to outweigh additional retries.
- Baseline: `6767e7447bfa285bef1daeea30d62f8a69367c42`, benchmark 6,068,328
  nodes.
- Candidate: change only `AspirationWindow` from 50 to 32. Add no diagnostics or
  per-node work.
- Measurements: not started. Run the offline safety gates and one advisory audit
  report, then request approval for the standard `[0, 5]` OpenBench screen.
- Result: pending.
- Artifact path: `tools/measurements/output/sw-01-6767e74/`.
- Disposition: active; documentation only, with no implementation or tests yet.

### SW-02 — Precompute LMR reductions [pending]

Replace hot-loop logarithms with a precomputed reduction table without
intentionally changing search behavior. Require exact baseline/candidate search
signatures and a repeatable throughput improvement; keep this separate from LMR
parameter tuning.

### SW-03 — Tune quiet LMR selectivity [pending]

Test one conservative quiet-only step, initially divisor 2.5 to 2.4. Leave
noisy moves, the fourth-move threshold, and history handling unchanged. Do not
retry the previously rejected high-history protection or revert the earlier
fourth-move stabilization as part of this task.

After these experiments, select at most one small next mechanism—such as
reverse futility pruning, shallow late-move pruning, or dynamic null-move
reduction—from current code and game evidence. Do not open it as a task until
the preceding candidates have dispositions. Continue retaining recurring
self-play or release failures as contextual positions, but do not require a new
diagnostic audit before a reversible production experiment.
