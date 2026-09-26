---
name: submit-latrunculi-openbench
description: Publish and submit one named, offline-tested Latrunculi candidate to OpenBench. Use when the user explicitly asks to send, submit, publish, push, or start an OpenBench test for a tree-changing candidate.
---

# Submit a Latrunculi OpenBench Test

Use `docs/engine-development.md` for current candidate state and `docs/openbench.md` as the
protocol. This skill performs one externally mutating action and then stops.

## Verify

1. Require a named tree-changing candidate whose offline evidence passed against the current
   baseline and whose local candidate branch and commit were recorded for review. Return stale,
   incomplete, or unmaterialized work to `test-latrunculi-candidate`.
2. Inspect the worktree, retained patch, recorded candidate revision, baseline, offline result,
   required risk tests, and existing Git history. Require the candidate commit's parent and tree
   difference to match the tested baseline and patch exactly. Confirm that no other OpenBench test
   is active and no matching test already exists.
3. Fix the Base and Dev revisions, settings, and termination rule before publication.

## Publish and Submit

An explicit request to submit the candidate authorizes pushing the recorded candidate branch,
OpenBench submission, and one immediate read-back. It does not authorize rewriting the candidate,
later checks, or integration.

Verify and push the already-reviewed local candidate commit without recreating it from the patch.
The published commit must be the same immutable candidate identity recorded by offline testing.
Never amend, rebase, force-push, or otherwise rewrite a reviewed, published, or tested revision.

Submit one test using `docs/openbench.md`. Fetch once to verify the canonical test ID, revisions,
settings, and running state. Return to the original branch, record the test reference in
**Candidates** with the short Base and Dev revisions and `Next: check OpenBench #ID`. Default
settings need not be copied to the board. Leave that board edit uncommitted unless the user
separately authorizes a bookkeeping commit.

Do not monitor, stop, replace, interpret a terminal result, integrate, or start another test.
