---
name: manage-latrunculi-openbench
description: Submit an offline-tested Latrunculi candidate, arrange baseline self-play data, check an OpenBench workload, or stop one when requested. Use for OpenBench interactions, including ad hoc comparisons; submission, data generation, and stopping require explicit requests.
---

# Manage Latrunculi OpenBench

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), [OpenBench guide](../../../docs/openbench.md), [workload and decision rules](references/workloads-and-decisions.md), and shared [candidate identity](../../references/candidate-rules.md#candidate-identity), [evidence reuse](../../references/candidate-rules.md#evidence-reuse), [approval and scope](../../references/candidate-rules.md#approval-and-scope), and [retention](../../references/candidate-rules.md#retention) rules. Concurrent tests are allowed; check for duplicate requested workloads. Perform only requested actions; do not poll or integrate. For “the current test,” require one unambiguous active Latrunculi workload or ask for its ID. Verify identity, revisions, settings, and termination rule before acting.

## Submit a candidate

Require an eligible **Ready for OpenBench** entry with passing offline evidence applicable to the current candidate and baseline, including any recorded evidence-reuse decision. Verify the branch points to the recorded SHA. Assess baseline or candidate changes under the shared evidence rules. If review or checks remain, record the applicable skill as the next action and stop before submission; do not invoke it automatically. Check server access and active or duplicate workloads before pushing.

Push the recorded branch without rewriting its commit. Submit one test using the predeclared strength profile and fetch once to verify its ID, Base and Dev revisions, settings, termination rule, and running state. Restore the original branch. Move the entry to **OpenBench tests** with its URL, revisions, and status; update the associated PR with the workload link and tested revisions.

## Generate tuning data

Require an explicit request for fresh self-play. Pin the baseline and use the [tuning corpus workload](references/workloads-and-decisions.md#tuning-corpus). Check for an existing requested workload first; push only the pinned baseline if needed. Fetch once to verify identity and settings. Keep the tuning item in **Issues and leads** with workload ID, baseline, PGN location, and resume condition. Record `tune-latrunculi-evaluation` as the next action once PGNs are available; do not begin fitting automatically. Do not start a second workload or poll automatically.

## Check or stop

Fetch a named test once. Report running games, score or Elo interval, and LLR where applicable, without putting intermediate snapshots on the board. For a terminal result, retain the [required evidence](references/workloads-and-decisions.md#evidence-and-candidate-decision). Tuning corpus scores, pass/fail flags, and manual stops do not decide a candidate; dataset preparation decides usability. Keep unfinished tuning work in **Issues and leads**, not candidate stages.

Check how the measured revisions relate to the live candidate and baseline. Apply direct results or a recorded evidence-reuse decision under the shared evidence-reuse rules; unresolved applicability does not advance the candidate. Move eligible accepted tests with required confirmation complete to **Ready for integration**, rejections to **Recent results**, and stopped or inconclusive tests to **OpenBench tests** with their next decision. Preserve published test identity when the baseline changes. Update the associated PR with terminal evidence and its applicability to the current head. Report ad hoc comparisons without board or PR edits.

Only an explicit stop request authorizes stopping a workload. If already terminal, report the result. Otherwise stop it, immediately read back the same test, and preserve any boundary decision reached. A pre-boundary stop is inconclusive; in-flight results may still arrive, so the immediate game count is a snapshot. Do not delete or restart tests or stop server or worker services.

Stop after the requested OpenBench operation; do not automatically begin the next workflow stage. Do not change account permissions to complete an operation; report access failures.
