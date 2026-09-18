---
name: explore-latrunculi-candidates
description: Explore bounded ideas for improving Latrunculi's playing strength, coordinated in docs/playing-strength.md, through code inspection, temporary prototypes, and exploratory measurements. Use for rough ideas, profiling questions, mechanism investigations, or candidate discovery that is not ready for frozen Screen and Qualification.
---

# Explore Latrunculi Candidates

`docs/playing-strength.md` owns task state and the operational baseline. Read `docs/search.md` for
search measurement meaning. Linear HCE fitting follows `tools/tuning/workflow.md`; do not invent
search-style panels for it here. Work on one active offline task and one CPU-sensitive measurement
stream at a time.

## Bound the Exploration

1. Inspect the worktree, operational baseline, relevant code, tests, measurements, and history.
   Preserve unrelated work and archived evidence. Do not edit `docs/roadmap.md` unless requested.
2. Move the task to **Active offline work** with `Phase: explore`. Record its bounded question,
   task HEAD, baseline, artifact directory, evidence budget, and stopping conditions before changing
   code or collecting data.
3. Define the three possible outcomes: a candidate proposal, a durable null result, or a precise
   unresolved question. Exploration has no pass/fail gate and must not call a candidate qualified.

If the request already supplies a concrete candidate whose Change, evidence class, mechanism,
thresholds, Screen, and Qualification can be frozen, route it to `$qualify-latrunculi-candidate`.
Route work on an already-qualified candidate to `$promote-latrunculi-candidate`.

## Investigate

- Use the smallest evidence that can answer the recorded question. Temporary instrumentation,
  bounded prototype implementations, and exploratory measurements are allowed within the recorded
  budget.
- Use existing component tools and the relevant domain guide. Treat any Screen-like workload or
  speed result as exploratory evidence, not Qualification.
- Log material attempts and preserve raw output under
  `tools/measurements/output/<task-id>-<baseline>/`. Do not retain one-off runners when existing
  commands suffice.
- Stop when the question is answered, the budget is exhausted, or a required condition is
  unavailable. Do not broaden the question merely to produce a candidate.

Exploration does not authorize formal Screen or Qualification, commits, candidate branches,
pushes, local games, OpenBench access, or integration.

## Finish and Restore

- **Candidate proposal:** record the exact proposed Change, evidence class, mechanism, applicable
  mechanism-aligned efficacy claim, supporting evidence, important risks, prototype patch and hash
  when applicable, and proposed thresholds, Screen, Qualification, and decision rules. For an
  exact-tree proposal, include the code- or build-level semantic-equivalence argument; treat
  matching exploratory signatures as corroboration only. Propose a bounded targeted panel only
  when the candidate changes or relies on behavior outside the cold, one-thread, fixed-depth
  domain default, such as retained state, clock or limit handling, or threading and shared state.
  Use focused mechanism instrumentation, not a targeted panel, for sparse activation. State what
  a targeted panel adds without replacing the default. The proposal remains unqualified until
  independently frozen and run by the qualification skill.
- **Null result:** record what was tested and why the evidence does not support a candidate, then
  move it to the ledger as completed with no candidate.
- **Unresolved:** record the exact missing evidence or unavailable condition and the smallest next
  question, then move it to the ledger as stopped or incomplete.

Remove temporary behavior and instrumentation and restore the operational baseline before any
handoff. Return a candidate proposal to the Pending queue under the same ID with its artifact path
and `Next: qualify`. If Qualification was already authorized, hand the restored task directly to
`$qualify-latrunculi-candidate` instead. Update `docs/playing-strength.md`, distill genuinely
durable findings into the relevant domain guide, and report the outcome, evidence, workspace state,
and any proposed handoff. Do not begin Qualification without authorization.
