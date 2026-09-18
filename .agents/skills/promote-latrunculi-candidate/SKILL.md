---
name: promote-latrunculi-candidate
description: Promote a qualified Latrunculi playing-strength candidate recorded in docs/playing-strength.md through selection, immutable publication, paired testing, result recording, approved integration, and baseline refresh. Use for qualified-candidate selection, commit or push preparation, OpenBench work, external-result handling, or integration.
---

# Promote a Latrunculi Candidate

`docs/playing-strength.md` owns candidate state, authorization, external status, results, and the
operational baseline. `docs/openbench.md` owns stable paired-testing settings and termination
policy. Start only from a candidate recorded as qualified; route rough ideas to
`$explore-latrunculi-candidates` and unqualified or stale offline work to
`$qualify-latrunculi-candidate`.

## Select and Check Freshness

1. Inspect the qualified table, operational baseline, candidate artifacts, Git history, and
   worktree. Preserve unrelated work and immutable tested revisions.
2. Apply the selection and parked-candidate rules in `docs/playing-strength.md`. Choose one candidate or
   none from existing evidence; do not run extra offline work merely to strengthen its case.
3. If the baseline changed after Qualification, identify only the evidence that could be affected
   and hand that bounded revalidation to `$qualify-latrunculi-candidate`. Do not publish or test a
   stale candidate first.
4. Verify that every targeted panel required by the candidate passed for the exact qualified
   revision. Return missing, ambiguous, or stale required coverage to
   `$qualify-latrunculi-candidate` before publication or paired testing.

## Require Authorization

Selection and read-only review do not authorize mutation. Require explicit authorization for each
applicable stage: local publication (branch plus candidate commit), push, local paired games or
OpenBench submission or mutation, later status monitoring or terminal retrieval, and integration.
Submission authorization includes its immediate read-only preflight and post-submit identity check,
but not later monitoring. One request or active goal may authorize several named stages; complete
only that scope.

## Publish an Immutable Candidate

For authorized publication, create a lowercase `<task-id>-<slug>` branch from the baseline against
which the candidate qualified. Apply and commit only the retained candidate patch, verify its hash
and benchmark identity, then push only when authorized. If the intended immutable branch or commit
already exists, verify its baseline, patch, hash, and benchmark identity and resume instead of
recreating it. Once pushed or submitted for testing, never amend, rebase, force-push, or otherwise
rewrite that revision.

## Run Paired Testing

Behavior-changing candidates require paired games. Skip them only when the retained Qualification
evidence meets `docs/playing-strength.md`'s behavior-preservation rule; matching signatures alone
are insufficient. Before submission, read `docs/openbench.md`, record the hypothesis and termination
profile, and verify both immutable revisions and every setting. As the authorized submission
preflight, confirm the OpenBench slot is free and that no test with the same base and candidate
revisions, settings, and termination profile is already submitted or recorded. Run one test at a
time. After submission, fetch once to confirm its identity and running state, record one canonical
test ID and URL, and return control without recurring polling.

If explicitly authorized to use local `fastchess-ob`, preserve the binaries, runner, book,
settings, seed, log, PGN, pentanomial, and result data.

## Record the External Result

Record a terminal result once against the canonical test ID; verify an existing record rather than
duplicating it when resuming.

- **Accepted:** record the terminal games, LLR, Elo interval, decision, revisions, settings, and PGN
  location. Integration still requires explicit approval.
- **Rejected:** preserve the same evidence, move the candidate to the ledger, and never integrate
  it.
- **Inconclusive or infrastructure-interrupted:** preserve the evidence and keep the candidate
  qualified unless the user closes it. Do not reinterpret it as a pass or failure.

If the result motivates a materially different proposal, record the finding and route that new
question to `$explore-latrunculi-candidates`; do not rewrite the tested candidate.

## Integrate When Approved

Prefer a squash integration and confirm it contains exactly the tested change on the approved
baseline. Rebuild and run the required tests, benchmark fingerprint, and applicable domain identity
checks. For search work, require the integrated corpus signatures to match the retained qualified or
tested candidate before refreshing the cached baseline. Preserve one concise integration record
linking the baseline, candidate, integration revision, patch and binary hashes, checks, signatures,
and any terminal external evidence. Only then update the operational baseline and ledger in
`docs/playing-strength.md`. Keep the tested revision immutable and stop after the authorized
integration scope.

The primary agent owns selection, Git actions, authorization checks, external actions, and final
state.
