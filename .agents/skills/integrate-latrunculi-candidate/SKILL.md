---
name: integrate-latrunculi-candidate
description: Integrate one explicitly approved, reviewed Latrunculi candidate revision and refresh the operational baseline. Use only when the user asks to integrate the recorded commit for a tree-preserving offline pass or an OpenBench-accepted candidate.
---

# Integrate a Latrunculi Candidate

Use `docs/engine-development.md` for current candidate state and search identity rules. Read the
candidate evidence and any other applicable domain rules. Integration always requires an explicit
user request approving the candidate revision that was surfaced for review.

## Verify and Integrate

1. Require either a tree-preserving offline pass or an accepted OpenBench result, plus the recorded
   local or published candidate branch and full commit SHA. If that review identity is absent or
   differs from the revision presented to the user, stop without integrating.
2. Confirm the candidate commit's parent baseline, exact tree difference, retained patch, tests,
   and risk evidence. Never substitute a reconstructed or modified candidate.
3. Preserve unrelated work. Squash only the approved candidate commit's change onto the approved
   current baseline. Stop and return stale or mismatched work to `test-latrunculi-candidate`.
4. Build and run the required complete tests, benchmark fingerprint, and domain identity checks.
   For search work, require complete corpus signatures to match the retained candidate.
5. Create the local engine integration commit. Refresh the cached baseline evidence, update the
   board with the engine commit's short SHA, move the candidate to **Recent results**, and create a
   second local bookkeeping commit that names the engine commit.

Never rewrite the reviewed or tested revision, integrate a mismatch, push, run new games, or begin
another experiment. Report the approved candidate SHA, both local commits, verification, baseline
evidence, and final worktree state.
