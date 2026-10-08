---
name: integrate-latrunculi-candidate
description: Merge an explicitly approved Latrunculi candidate PR after its required acceptance checks, then refresh the operational baseline. Use when the user requests integration of the recorded revision.
---

# Integrate a Latrunculi Candidate

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), shared [candidate identity](../../references/candidate-rules.md#candidate-identity), [evidence reuse](../../references/candidate-rules.md#evidence-reuse), [approval and scope](../../references/candidate-rules.md#approval-and-scope), and [retention](../../references/candidate-rules.md#retention) rules, [offline checks](../test-latrunculi-candidate/references/offline-checks.md), and applicable [game evidence](../manage-latrunculi-openbench/references/workloads-and-decisions.md#evidence-and-candidate-decision).

## Prepare

Require a **Ready for integration** candidate, its PR, approval of its current head, and an explicit integration request. Approval may be on the hosting platform or in conversation. If publication remains, record the review skill's publication step as the next action and stop before integration; do not invoke review automatically or silently use a different integration path.

Verify the PR head, current target revision, complete diff, and required [acceptance evidence](../../references/candidate-rules.md#acceptance). Assess baseline advancement under the shared evidence rules; prepare the proposed combined result in an isolated worktree. If adaptation changes reviewed code, retain it on the candidate branch, record affected review/tests as the next action, and stop before merging until the revision has applicable evidence and approval.

## Integrate

1. Build the proposed result. Run the complete Release suite, one full standard corpus and fingerprint, and relevant regressions. Require signatures and fingerprint to match the accepted candidate or its separately validated combined revision. Reuse sanitizer and timing evidence only when relevant source, build inputs, settings, and environment remain applicable; rerun checks for changed inputs or unresolved risks.
2. Recheck the approved head and target immediately before merging. If either moved, reassess before proceeding. Squash-merge the approved PR with a short, single-line engine commit message and no body, guarding against an unexpected head change. Do not bypass repository merge protections.
3. Read back the merge result, synchronize local `main` without discarding unrelated work, and verify merged source/build inputs match the checked result. If they differ, do not declare the baseline refreshed; validate the actual result and report the discrepancy. Do not automatically revert or force-push.
4. Refresh operational baseline evidence and record the integration commit in **Recent results**. Assess waiting candidates against the new baseline and record outstanding review/tests without blanket invalidation.

## Hand off

Report the approved PR/head, integration commit, verification and reused evidence, baseline location, and final worktree state. Stop after integration. Do not start games, another experiment, or branch/report pruning automatically. If merging is unavailable, retain the checked result and report the remaining action without silently substituting a local integration commit.
