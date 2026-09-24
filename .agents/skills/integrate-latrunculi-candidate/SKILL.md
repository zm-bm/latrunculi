---
name: integrate-latrunculi-candidate
description: Integrate one approved Latrunculi playing-strength candidate and refresh the operational baseline. Use only when the user explicitly asks to integrate a tree-preserving offline pass or an OpenBench-accepted candidate.
---

# Integrate a Latrunculi Candidate

Use `docs/playing-strength.md` for current candidate state and search identity rules. Read the
candidate evidence and any other applicable domain rules. Integration always requires an explicit
user request.

## Verify and Integrate

1. Require either a tree-preserving offline pass or an accepted OpenBench result. Confirm its
   baseline, exact patch or immutable revision, tests, and risk evidence.
2. Preserve unrelated work. Squash only the retained candidate change onto the approved current
   baseline. Stop and return stale or mismatched work to `test-latrunculi-candidate`.
3. Build and run the required complete tests, benchmark fingerprint, and domain identity checks.
   For search work, require complete corpus signatures to match the retained candidate.
4. Create the local engine integration commit. Refresh the cached baseline evidence, update the
   board with the engine commit's short SHA, move the candidate to **Recent results**, and create a
   second local bookkeeping commit that names the engine commit.

Never rewrite the tested revision, integrate a mismatch, push, run new games, or begin another
experiment. Report both local commits, verification, baseline evidence, and final worktree state.
