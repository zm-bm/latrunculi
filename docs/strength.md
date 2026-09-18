# Strength Development

This file tracks the operational baseline, experiment queue, current candidates, and recent
results. Durable findings belong in domain guides such as [Search Knowledge](search.md).

## Operational baseline

Update these values only after an approved candidate is integrated:

| Field | Current value |
|---|---|
| Operational engine baseline | `8a44474e75371809f5d93e9925d5035780db427c` (`8a44474`) |
| OpenBench compatibility fingerprint | 5,101,317 nodes |
| Cached search corpus baseline | `tools/measurements/output/search-baseline-8a44474/` |

Refresh the cached search corpus when search behavior, its workload, or measurement meaning
changes. Use its deterministic signatures and nodes for comparisons, but use contemporaneous paired
runs for timing.

## Qualified candidates

These candidates passed offline testing. **Status and next step** records whether each is testing,
selected next, parked, or lower priority.

| ID | Candidate | Offline result | Status and next step | Artifacts |
|---|---|---|---|---|
| SW-18 | Negative-history depth-1 LMP after eight moves | Node ratio 0.9843; exact candidate repeats; objective checks passed; trajectory diagnostics recorded | Qualified against `c6eb554`; parked and stale after SW-20 integration; requalify only if selected | `sw-18-c6eb554/` |

## Workflow

Preserve existing `SW-XX` IDs; use `EI-XXX` for every new experiment. Keep each task in one place:

- **Pending queue:** ordered ideas; the first entry is next.
- **Active experiment:** the one task currently being developed or tested offline.
- **Qualified candidates:** candidates that passed offline testing and await selection or another
  decision. Qualified or externally tested candidates do not count as active.
- **Experiment ledger:** completed, rejected, stopped/incomplete, or skipped work.

Before implementation, move the task to **Active experiment** and complete this record:

```text
<ID> — title
Evidence class: <tree-changing | exact-tree | domain-specific>
Task HEAD: <full revision before implementation>
Variants (optional): <allowed single-factor family and attempt or time limit>
Change: <candidate and expected benefit; exact before Screen>
Mechanism (exact-tree performance only): <cost removed, reduced, or moved off the critical path; offsetting-work check; why the opportunity can clear delta>
Baseline: <revision and relevant differences>
Artifacts: <directory fixed before implementation>
Screen: <domain default plus focused checks>; reject if <specific result occurs>
Qualification: <complete suite, reproducibility, applicable timing, and risk checks>;
qualify if <all required results>
```

Freeze the hypothesis, allowed variant family, attempt budget, checks, and pass/fail rules before
collecting results. Freeze the exact `Change` before Screen. Fully specified tasks omit `Variants`.
Screen is the earliest valid rejection panel, not necessarily a small workload. Domain guides own
default panels and metric meanings; the active record owns candidate-specific thresholds,
additions, and predeclared deviations. Qualification verifies reproducibility and adds complete and
risk-specific evidence rather than automatically enlarging the efficacy sample. A failed
hypothesis cannot be rescued after results arrive by reclassifying the candidate; that requires a
new task ID.

Follow the applicable domain guide's reproducibility rules. Within Qualification, run cheap
decisive checks before expensive or environment-sensitive work.

Preserve one immutable predeclaration, raw evidence, a candidate patch and hashes when needed, and
one concise final manifest. Avoid duplicate result summaries and retained runner scripts unless
they add evidence. If later methodology review invalidates a measurement-system gate without
changing the candidate, hypothesis, or candidate acceptance rule, preserve the frozen declaration
and raw output, record an explicit amendment, and resume the same ID only with user approval. Rerun
only affected evidence; other rule changes require a new task ID.

Apply strengthened workflow defaults prospectively. Existing qualified or externally testing
candidates retain the contract under which they qualified unless requalification is explicitly
authorized; never interrupt a running external test merely because this workflow changes.

The [strength skill](../.agents/skills/advance-latrunculi-strength/SKILL.md) owns execution and
cleanup. Use [Search Knowledge](search.md) for search evidence, the
[measurement guide](../tools/measurements/README.md) for commands, the
[HCE tuning workflow](../tools/tuning/workflow.md) for linear evaluation fitting, and the
[OpenBench guide](openbench.md) for paired testing.

## Candidate Selection

Qualified candidates remain separate from the operational baseline. Keep at most two parked
candidates that require games; before adding a third, select one, close one, or obtain approval to
exceed the limit. Work intended to preserve behavior may continue when the limit is full; it does
not count toward the limit if Qualification proves exact preservation.

Choose one candidate or none. When candidates compete for a test slot, record a short selection
case from evidence already collected: baseline freshness; mechanism and the uncertainty games would
resolve; applicable efficacy and timing evidence; objective failures or predeclared, corroborated
consistency concerns; implementation risk; and overlap with previously tested candidates. Prefer
complete, comparable, current evidence and a test likely to resolve useful uncertainty. Do not run
extra offline work merely to make a case look broader. Offline node and speed gains are not Elo
estimates, and no metric is an independent vote. Do not assign a numerical selection score;
marginal candidates may stay parked.

Behavior changes require paired games; proven behavior-preserving changes may skip them. Run one
paired test at a time. A running test does not block offline work from the operational baseline. If
that baseline changes, requalify only the selected stale candidate and only the evidence the change
could affect.

## Authorization

Offline execution covers the task record, allowed variants, Screen, Qualification, evidence,
documentation, and cleanup. Unless the request says Screen only, continue through Qualification.
Offline execution does not authorize commits, pushes, local games, starting, changing, or stopping
OpenBench tests, or integration.

Each external stage must be explicitly authorized. One request or active goal may authorize
several stages; complete that scope without asking again between stages.

A request or active goal may authorize a named list or number of tasks. Restore the operational
baseline after each task and stop when that scope ends. Selecting an older candidate does not by
itself authorize requalification. Integration always requires explicit approval.

## Pending queue

### SW-11 — Prune severe depth-1 SEE-losing captures

After one legal move at depth-1 NonPV nodes, skip only non-TT, nonpromotion, nonchecking captures
with `board.see(move) < -eval::pawn.mg`. Exempt in-check and mate-window nodes; leave qsearch and
picker SEE bands unchanged. Repeat timing because the rule may add hot-path SEE work.

### SW-13 — Prefetch child transposition-table clusters

Prefetch child-key TT clusters without changing TT layout, policy, or search signatures. Require a
repeatable same-core throughput gain and reject it offline if signatures or timing fail. Use
OpenBench only if behavior unexpectedly changes.

### SW-14 — Enable release link-time optimization

Enable supported CMake interprocedural optimization for production and OpenBench builds without
architecture flags or sanitizer/debug changes. Require exact signatures, GCC and Clang builds,
and repeatable same-core throughput; reject on portability, signature, or timing failure.

### SW-15 — Remove atomic work from per-node accounting

Use worker-owned counting with race-free periodic and exact final publication. Preserve polling,
UCI progress, final counts, and bounded node-limit overshoot. Require fixed-depth signatures,
focused lifecycle/limit checks, repeated 1/2/4-thread timing, and TSan.

### SW-16 — Reduce clearly bad-history quiets further

Add one reduction ply only to already-LMR-eligible NonPV quiets that are nonchecking,
nonpromotion, nonkiller, and below one predeclared negative-history threshold. Preserve the base
formula and bounds; test one threshold, not a sweep. This differs from rejected blanket-divisor
and high-history-protection shapes.

### SW-17 — Make null-move reduction adaptive

Keep current eligibility, material guard, TT veto, and SW-04 behavior. Replace only the fixed
3/4-ply reduction with one bounded depth/static-surplus formula; add no eligibility gate or
verification search. Require focused correctness and convergence diagnostics, plus paired
validation if it passes.

## Experiment ledger

Full records remain in the cited ignored artifacts and Git history. Artifact paths below are
relative to `tools/measurements/output/`.

| ID | Candidate | Evidence | Result | Artifacts |
|---|---|---|---|---|
| SW-01 | Aspiration window 50 -> 32 cp | Node ratio 1.1227; median 1.1512; 9/11 cases used more nodes | Rejected; retain 50 cp | `sw-01-6767e74/` |
| SW-02 | Exact lookup table for existing LMR formula | Exact tree; median NPS -1.767% across seven pairs | Rejected; retain formula | `sw-02-cef892a/` |
| SW-03 | Quiet LMR divisor 2.5 -> 2.4 | Offline ratio 0.9736; test #21 stopped at 8,018 games, LLR +0.9037 | Done, inconclusive; not integrated | `sw-03-72d2c88/` |
| SW-04 | Require `static_eval >= beta` for null move | Focused descendant-null regression failed twice | Rejected; retain eligibility | `sw-04-08e5c2f/` |
| SW-05 | Guarded reverse futility at NonPV depths 1–3 | Offline ratio 0.7855; test #22 passed at 2,966 games, +31.01 +/-9.19 Elo | Done and integrated | `sw-05-02a9537/` |
| SW-06 | Confirm reduced PV fail-highs at full depth | Offline ratio 0.9949; test #23 stopped at 8,676 games, LLR +1.1808 | Done, inconclusive; not integrated | `sw-06-1b2b8fc/` |
| SW-07 | Preserve later checking quiets after futility | Nodes +5.39%; pilot stable-window depth 17 -> 14 | Historical stop under the retired convergence gate; not re-evaluated | `sw-07-b8a5335/` |
| SW-08 | Filter only nonchecking quiets after futility | Nodes +5.34%, benchmark +10.02%; test #25 strength-neutral/inconclusive | Done; retained by correctness policy | `sw-08-6c01040/` |
| SW-09 | Extend reverse futility to depth 4 | Ratio 0.9956; test #26 stopped at 4,332 games, LLR +0.464 | Done, inconclusive; not integrated | `sw-09-c6eb554/` |
| SW-10 | Order quiet checks first at futility nodes | Screen passed at `R_node_g` 0.9978 and `R_node_total` 0.9973; timing and sanitizers not run | Stopped under the retired single-sentinel gate; Qualification incomplete and exact shape not closed | `sw-10-c6eb554-v2/` |
| SW-12 | Unconditional depth-1 LMP after eight moves | Ratio 0.8271; NonPV/PV score 76/114 | Rejected; exact shape closed | `sw-12-c6eb554/` |
| SW-19 | Move SW-18 threshold after six moves | Ratio 0.9844; no improvement over SW-18 | Rejected; do not try after four | `sw-19-c6eb554/` |
| SW-20 | Negative-history depth-2 LMP after twelve moves | Ratio 0.9513; test #27 passed at 23,214 games, LLR +2.9601, +4.89 +/- 3.30 Elo | Done and integrated as `8a44474`; tested commit `f2c77b5` | `sw-20-c6eb554/` |
| SW-21 | Reuse SW-20 history/check work | Exact tree; corpus timing 0.94% faster; benchmark tied | Rejected; no robust throughput gain | `sw-21-f2c77b5/` |
| EI-001 | Revalidate SW-21 under direct paired timing | Exact tree; six-pair medians 1.0034 overall, 1.0129 BC, 0.9980 CB; 3/6 wins | Rejected; all three aggregations missed the 0.9925 timing gate | `ei-001-8a44474/` |
| SW-22 | Reuse SW-20 picker metadata | Exact tree; timing order-dependent; benchmark tied | Rejected; no robust throughput gain | `sw-22-f2c77b5/` |
| SW-23 | `-64` history tier for depth-2 moves 11–12 | No corpus or benchmark change | Rejected; rule did not fire materially | `sw-23-f2c77b5/` |
| SW-24 | Move SW-20 threshold after ten moves | Ratio 1.0043; two cases drove the largest node increases | Rejected; exact SW-20 restored | `sw-24-f2c77b5/` |
| SW-25 | Move SW-20 threshold after eleven moves | Ratio 1.0035; the same two cases drove node increases | Rejected; boundary path closed | `sw-25-f2c77b5/` |

Keep detailed evidence in artifacts. Before removing an old ledger row, move any lasting conclusion
to the relevant domain guide.
