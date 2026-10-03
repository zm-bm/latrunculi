---
name: integrate-latrunculi-candidate
description: Integrate one explicitly approved, reviewed Latrunculi candidate revision and refresh the operational baseline. Use only when the user asks to integrate the recorded commit for a tree-preserving offline pass or an OpenBench-accepted candidate.
---

# Integrate a Latrunculi Candidate

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), [candidate revision rules](../explore-latrunculi-change/references/candidate-revisions.md), [offline checks](../test-latrunculi-candidate/references/offline-checks.md), and any [game evidence](../manage-latrunculi-openbench/references/workloads-and-decisions.md#evidence-and-candidate-decision). Require a **Ready for integration** entry and an explicit user request approving its recorded revision.

## Verify and integrate

1. Check the approved candidate SHA, diff, parent baseline, and retained offline evidence against the current baseline. If games were required, verify accepted OpenBench evidence and any required confirmation. Stop on missing, stale, or mismatched evidence; return the candidate to **Candidates for local testing** and `test-latrunculi-candidate`.
2. Preserve unrelated work. Squash only the approved candidate commit's change onto the approved current baseline. Never rewrite the reviewed or tested candidate.
3. Build and run the complete required tests, benchmark fingerprint, and domain identity checks. Before refreshing the baseline, require the integrated build's complete corpus signatures and applicable fingerprint to match the retained candidate.
4. Create the local engine integration commit. Refresh baseline evidence and move the item to **Recent results** with the engine commit SHA. Return other waiting candidates to **Candidates for local testing** as stale. Preserve historical evidence and published test identities. Create a separate bookkeeping commit naming the engine commit.

Do not integrate a mismatch, push, run new games, or begin another experiment. Report the approved candidate SHA, both local commits, verification, baseline evidence, and final worktree state.
