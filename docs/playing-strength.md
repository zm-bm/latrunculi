# Playing Strength Development

This file is the sole mutable lifecycle authority for the operational baseline, active offline
work, experiment queue, candidates, external status, authorization, and recent results. Durable
findings belong in domain guides such as [Search Knowledge](search.md).

## Operational baseline

Update these values only after an approved candidate is integrated:

| Field | Current value |
|---|---|
| Operational engine baseline | `8a44474e75371809f5d93e9925d5035780db427c` (`8a44474`, SW-20, OpenBench #27) |
| OpenBench compatibility fingerprint | 5,101,317 nodes |
| Cached search corpus baseline | `tools/measurements/output/search-baseline-8a44474/` |

Refresh the cached search corpus when search behavior, its workload, or measurement meaning
changes. Use its deterministic signatures and nodes for comparisons, but use contemporaneous paired
runs for timing.

## Active offline work

None. When occupied, this section contains exactly one task using the applicable phase record
below.

## Qualified candidates

These candidates passed offline testing and remain live for selection or another decision.

| ID | Candidate | Offline result | Status and next step | Artifacts |
|---|---|---|---|---|
| SW-18 | Negative-history depth-1 LMP after eight moves | Node ratio 0.9843; exact candidate repeats; objective checks passed; trajectory diagnostics recorded | Qualified against `c6eb554`; parked and stale after SW-20 integration; requalify only if selected | `sw-18-c6eb554/` |

## Workflow

Three repo-local skills execute distinct stages of candidate development:

| Stage | Skill | Starts with | Ends with |
|---|---|---|---|
| Explore | [`explore-latrunculi-candidates`](../.agents/skills/explore-latrunculi-candidates/SKILL.md) | A rough idea or bounded question | A candidate proposal, null result, or unresolved question |
| Qualify | [`qualify-latrunculi-candidate`](../.agents/skills/qualify-latrunculi-candidate/SKILL.md) | A concrete candidate with freezeable rules | A qualified, rejected, or unresolved candidate |
| Promote | [`promote-latrunculi-candidate`](../.agents/skills/promote-latrunculi-candidate/SKILL.md) | A qualified candidate | An external result, approved integration, or requalification request |

Preserve existing `SW-XX` IDs; use `EI-XXX` for every new experiment. One ID follows the work
through all applicable stages. Keep each task in one place:

- **Pending queue:** ordered inactive work, including candidate proposals awaiting Qualification.
  Record `Next: qualify` on a returned proposal; the first authorized entry is next.
- **Active offline work:** the one task currently being explored or qualified. Record its phase as
  `explore` or `qualify` and keep only one CPU-sensitive measurement stream active.
- **Qualified candidates:** candidates that passed offline testing and await selection or another
  decision. Qualified or externally tested candidates do not count as active.
- **Experiment ledger:** completed, rejected, stopped/incomplete, or skipped work.

Before Exploration, move the task to **Active offline work**, set `Phase: explore`, and complete
this record:

```text
<ID> — title
Phase: explore
Question: <bounded question>
Task HEAD: <full revision before work>
Baseline: <revision and relevant differences>
Artifacts: <directory fixed before work>
Evidence budget: <attempt, measurement, or time bound>
Stop when: <answer, exhausted budget, or unavailable condition>
```

Exploratory prototypes and measurements do not qualify a candidate. After Exploration, restore
the baseline. Return a candidate proposal to the Pending queue under the same ID with its artifact
path and `Next: qualify`, unless Qualification was already authorized; in that case, move it back
to Active offline work and independently freeze the Qualification record. Move a null result or
unresolved question to the ledger, recording the exact resume condition for unresolved work.

Before Qualification, move or keep the task in **Active offline work**, set `Phase: qualify`, and
complete this record:

```text
<ID> — title
Phase: qualify
Evidence class: <tree-changing | exact-tree | domain-specific>
Task HEAD: <full revision before implementation>
Variants (optional): <allowed single-factor family and attempt or time limit>
Change: <candidate and expected benefit; exact before Screen>
Mechanism: <claimed effect and activation; exact-tree adds equivalence argument, work removed or moved, offsetting-work check, and delta rationale>
Baseline: <revision and relevant differences>
Artifacts: <directory fixed before implementation>
Screen: <domain default plus applicable efficacy metric/threshold, focused checks, and targeted panels>; reject if <specific result occurs>
Qualification: <complete suite, reproducibility, applicable timing, remaining targeted panels, and risk checks>;
qualify if <all required results>
```

Before collecting results, freeze the hypothesis, exact `Change`, allowed variant family, attempt
budget, checks, and pass/fail rules. Fully specified tasks omit `Variants`. Domain guides own
default panels, metric meaning, reproducibility, and targeted-panel triggers; the active record
owns candidate-specific thresholds, additions, and deviations. Screen is the earliest valid
rejection panel. Run cheap decisive checks before environment-sensitive work, and do not rescue a
failed hypothesis by reclassifying it after results arrive; that requires a new task ID.

After a Screen-only pass, restore the baseline and return the task to the Pending queue under the
same ID with `Next: qualify; Screen passed; Qualification pending`. Resume from Qualification only
when the candidate, baseline, inputs, and measurement meaning are unchanged; otherwise restart
Screen. A new candidate that qualifies moves to Qualified candidates; a rejection or unresolved
result moves to the ledger. For stale-candidate revalidation, temporarily move the candidate to
Active offline work and retain its frozen hypothesis and acceptance contract unless an authorized
methodology amendment applies: a pass updates its qualified baseline and artifacts, a valid failed
gate moves it to the ledger, and an unavailable required condition returns it to Qualified
candidates marked stale and revalidation unresolved.

While work is active or qualified, preserve one immutable predeclaration, raw evidence, a candidate
patch and hashes when needed, and one concise final manifest. Avoid duplicate summaries and
one-off runners. If later methodology review invalidates a measurement-system gate without changing
the candidate, hypothesis, or acceptance rule, preserve the declaration and raw output, record an
explicit amendment, and resume the same ID only with user approval. Rerun only affected evidence;
other rule changes require a new task ID.

Apply strengthened defaults prospectively: existing qualified or externally testing candidates
retain their contract unless requalification is authorized, and a workflow change never interrupts
a running external test. Add gates, schemas, or automation only after repeated use demonstrates
stable fields or concrete friction.

Use [Search Knowledge](search.md) for search evidence, the
[measurement guide](../tools/measurements/README.md) for commands, the
[HCE tuning workflow](../tools/tuning/workflow.md) for linear evaluation fitting, and the
[OpenBench guide](openbench.md) for paired testing.

## Candidate Selection

Qualified candidates remain separate from the operational baseline. Keep at most two parked
candidates that require games; before adding a third, select one, close one, or obtain approval to
exceed the limit. Work intended to preserve behavior may continue when the limit is full; it does
not count toward the limit only when Qualification satisfies the applicable domain guide's
exact-behavior proof standard.

Choose one candidate or none. When candidates compete for a test slot, record a short selection
case from evidence already collected: baseline freshness; mechanism and the uncertainty games would
resolve; applicable efficacy and timing evidence; objective failures or predeclared, corroborated
consistency concerns; implementation risk; and overlap with previously tested candidates. Prefer
complete, comparable, current evidence and a test likely to resolve useful uncertainty. Do not run
extra offline work merely to make a case look broader. Offline node and speed gains are not Elo
estimates, and no metric is an independent vote. Do not assign a numerical selection score;
marginal candidates may stay parked.

Behavior changes require paired games. A candidate may skip them only when Qualification satisfies
the applicable domain guide's semantic-equivalence, exact-signature, and test requirements and
every applicable targeted panel; sampled signature agreement alone is insufficient. Run one paired
test at a time. A running test does not block offline work from the operational baseline. If that
baseline changes, requalify only the selected stale candidate and only the evidence the change
could affect.

Keep running, accepted-awaiting-integration, and inconclusive external candidates in Qualified
candidates. Move rejected or integrated candidates to the ledger; an approved integration also
refreshes the operational baseline after the applicable domain integration-identity check passes.

## Authorization

Exploration authorization covers its bounded question, temporary prototypes or instrumentation,
exploratory evidence, documentation, and cleanup. It ends with a candidate proposal, null result,
or unresolved question and does not authorize Qualification.

Qualification authorization covers the frozen task record, allowed variants, Screen,
Qualification, evidence, documentation, and cleanup. Unless the request says Screen only, continue
through Qualification. Neither offline stage authorizes commits, pushes, local games, starting,
changing, or stopping OpenBench tests, or integration.

Require explicit authorization for each applicable mutating or external stage: local publication
(branch plus candidate commit), push, local paired games or OpenBench submission or mutation,
later status monitoring or terminal-result retrieval, and integration. Submission authorization
includes the immediate read-only preflight and post-submit identity check, but not later monitoring.
One request or active goal may authorize several named stages; complete that scope without asking
again between stages.

A request or active goal may authorize a named list or number of tasks. Restore the operational
baseline after each task and stop when that scope ends. Selecting an older candidate does not by
itself authorize requalification. Integration always requires explicit approval.

## Pending queue

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

| ID | Candidate | Evidence | Result | Artifacts |
|---|---|---|---|---|
| SW-11 | Prune severe depth-1 SEE-losing captures | Screen `R_node_g` 0.9883 passed; `R_node_total` 1.0028; complete tests and reproducibility passed; balanced timing ratio 1.0414 with 0/6 wins | Rejected; missed the 1.0100 timing gate | `sw-11-8a44474/` |

Keep only records needed for near-term coordination; distill lasting conclusions into the relevant
domain guide before pruning them. Git history retains removed records. On-disk artifacts are
retained for the operational baseline, active work, and qualified candidates; deleting terminal
artifacts requires a separate authorized cleanup.
