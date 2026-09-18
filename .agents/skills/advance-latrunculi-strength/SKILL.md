---
name: advance-latrunculi-strength
description: Advance stable-ID Latrunculi strength work in docs/strength.md through offline experiments, candidate selection, publication, paired testing, integration, and baseline refresh. Use when asked to plan, execute, review, revalidate, commit, publish, test, integrate, or record a strength task.
---

# Advance Latrunculi Strength

`docs/strength.md` owns the operational baseline, task state, task-specific rules, and results;
domain guides own mandatory default panels. Work on one active experiment at a time and follow the
authorization policy. Read only the relevant domain guide: `docs/search.md` for search work and
`tools/tuning/workflow.md` for linear HCE fitting.

## Start an Experiment

1. Inspect the worktree, operational baseline, relevant code, tests, measurements, and history.
   Check that borrowed ideas fit this engine. Preserve unrelated work and archived evidence. Do
   not edit `docs/roadmap.md` unless requested.
2. Move the task from the pending queue to **Active experiment** and complete its record before
   implementation, including its evidence class, task HEAD, artifact directory, Screen,
   Qualification, and separate reject and qualify rules. Record the initial worktree state in the
   artifact manifest. For an exact-tree performance task, complete its `Mechanism` field: name the
   cost removed, reduced, or moved off the critical path; check whether equivalent or offsetting
   work erases the benefit; and explain why the opportunity can plausibly clear the timing
   threshold.
3. For a fully specified candidate, omit `Variants`. Otherwise predeclare one kind of single-factor
   variant and its limit. A normal upper bound is one initial attempt, one implementation repair,
   and one variant. Keep the hypothesis, evidence class, variant range, checks, and pass/fail rules
   unchanged after results arrive.
4. Implement the first attempt. Before Screen, write the exact `Change`, record the candidate diff,
   and hash result-affecting inputs and measured binaries. Log each attempt as
   `attempt | change | result | decision`. A different kind of change or decision rule needs a new
   task ID.

If the request or active `/goal` authorizes several named tasks or a task count, run them one at a
time from the operational baseline. Otherwise stop after this task ends.

## Run the Offline Loop

Use the same response to a failure in either phase:

- Rerun an unchanged candidate only when setup or data collection failed.
- If the code does not implement the written `Change`, repair it once.
- Try a behavior variant only when `Variants` permits it and budget remains.
- Otherwise reject the candidate.

Count only a frozen hard gate as a failure. Interpret reproducibility and diagnostics under the
applicable domain guide.

Log any repair or variant, update `Change` when behavior changes, refresh the candidate diff and
hashes, and restart Screen. Do not change the rules after seeing a valid result. If later
methodology review invalidates a measurement-system gate, follow the amendment rule in
`docs/strength.md`.

### Screen

Run the focused checks and the applicable domain guide's Screen panel without substituting a
different workload. Add instrumentation only when current output cannot answer a question already
in the task. Crashes, illegal or malformed output, protocol errors, and failures of the domain's
reproducibility rule count against the candidate. Do not rescue a failed hypothesis by reclassifying
it after seeing results.

If Screen passes, stop only when the request explicitly asked for Screen alone. Otherwise continue
to Qualification.

### Qualification

Run the relevant complete release suite, the domain guide's Qualification panel, and the task's
remaining risk-specific checks. Follow the domain guide's order, avoid duplicate coverage, and
leave Qualification unresolved if required conditions are unavailable. Build `release-stats` only
when counters are needed. Profiling, performance counters, and disassembly are optional
diagnostics, not default gates.

Run ASan/UBSan only when the change creates a concrete risk involving storage, indexing, bounds,
ownership, lifetime, parsing, recursion, or core engine state. Run TSan only for shared state or
worker lifecycle. Record commands and results when sanitizers run. If a concrete listed risk exists
but the applicable sanitizer is omitted, record why.

Investigate conflicting checks before deciding; disagreement alone is not a chess-correctness
failure. Qualify only when every written hard requirement passes. Any repair or behavior change
restarts Screen and all of Qualification.

## Record the Result

- **Rejected:** preserve the evidence, move the task to the ledger, remove candidate behavior and
  temporary support, and restore the baseline. Remove only candidate-specific build output; never
  use broad `git clean` or delete archived evidence.
- **Stopped/incomplete:** preserve the evidence, keep the candidate out of the qualified table,
  remove candidate behavior, and restore the baseline. Handle methodology corrections only under
  the amendment rule in `docs/strength.md`.
- **Qualified:** move the task to the qualified table and record the comparable evidence required
  for candidate selection in `docs/strength.md`. Save an immutable commit when authorized;
  otherwise save a final patch and binary hashes. Restore the baseline before other work and never
  stack unintegrated candidates.

Use the task's usual artifact directory,
`tools/measurements/output/<task-id>-<baseline>/`, unless its domain already has another layout.
Keep the frozen predeclaration, raw evidence, one short final manifest, and a patch plus hashes when
no immutable commit represents the candidate. Analysis is optional; avoid duplicate result records
or retained runner scripts unless they add evidence. Then inspect the final status, update
`docs/strength.md`, and report the result, decisive evidence, checks run, workspace state, and next
action requiring approval.

Continue to another authorized task only after restoring the baseline. Apply the parked-candidate
limit in `docs/strength.md`.

## Publish or Test When Authorized

Offline work does not authorize commits, pushes, games, OpenBench changes, or integration. Before
Qualification, do not commit or create a candidate branch.

For an authorized qualified-candidate publication commit, create a lowercase `<task-id>-<slug>`
branch from the baseline against which the candidate qualified and commit only that candidate.
For explicitly authorized documentation, bookkeeping, or integration commits, use the authorized
branch and commit plan. Publishing, testing, and integration must also be explicitly authorized.
One request or active `/goal` may authorize several stages; complete that scope without asking
again. Verify the published revision. Once pushed or submitted for testing, do not amend, rebase,
or force-push it.

Apply the selection and stale-candidate rules in `docs/strength.md`. Initial Qualification uses the
full checks above; selected stale candidates rerun only evidence affected by the newer baseline.

Before paired testing, read `docs/openbench.md`, record the hypothesis and termination profile, and
verify the revisions and settings. Run one match at a time, confirm it started, and return control
without polling. If explicitly asked to use local `fastchess-ob`, preserve binary, runner, book,
settings, seed, log, PGN, pentanomial, and result data.

An inconclusive external result remains qualified unless the user closes it. Move rejected or
integrated work to the ledger. For approved integration, prefer a squash merge and confirm it
contains exactly the tested change on the approved baseline. Record the tested and integration
commits, then update the operational baseline. Never merge a rejected or closed-inconclusive
candidate. If the baseline changes during testing, keep the tested branch unchanged and create a
new revision only when revalidation requires one.

## Delegate Only When Useful

The primary agent normally does the work and owns task state, interpretation, Git actions, and
external authorization. Delegate only independent work that materially improves speed or quality.

- At most one delegated development agent may write. Give it one active task and the outcomes it
  may record. The primary must not edit concurrently or run other CPU-heavy measurements.
- A selector is read-only. Use one only when the test slot is free and at least two candidates
  appear applicable, the parked-candidate limit needs review, or the user asks. It may inspect
  evidence and Git history and recommend one candidate or none. It must not edit files or task
  state, run measurements, change Git, submit games, or integrate code.
