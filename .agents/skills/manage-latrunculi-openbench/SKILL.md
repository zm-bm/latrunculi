---
name: manage-latrunculi-openbench
description: Submit an offline-tested Latrunculi candidate, arrange baseline self-play data, check an OpenBench workload, or stop one when requested. Use for OpenBench interactions, including ad hoc comparisons; submission, data generation, and stopping require explicit requests.
---

# Manage Latrunculi OpenBench

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), [OpenBench guide](../../../docs/openbench.md), [workload and decision rules](references/workloads-and-decisions.md), and [candidate revision rules](../explore-latrunculi-change/references/candidate-revisions.md). Keep one OpenBench test active at a time. Perform only requested actions; do not poll or integrate. For “the current test,” require one unambiguous active Latrunculi workload or ask for its ID. Verify identity, revisions, settings, and termination rule before acting.

## Submit a candidate

Require an eligible **Ready for OpenBench** entry with an offline pass against the current baseline. Verify the branch points to the recorded SHA. Return stale or incomplete work to **Candidates for local testing** and `test-latrunculi-candidate`. Check server access and active or duplicate workloads before pushing.

Push the recorded branch without rewriting its commit. Submit one test using the predeclared strength profile and fetch once to verify its ID, Base and Dev revisions, settings, termination rule, and running state. Restore the original branch. Move the entry to **OpenBench tests** with its URL, revisions, and status.

## Generate tuning data

Require an explicit request for fresh self-play. Pin the baseline and use the [tuning corpus workload](references/workloads-and-decisions.md#tuning-corpus). Check for an existing requested workload first; push only the pinned baseline if needed. Fetch once to verify identity and settings. Keep the tuning item in **Issues and leads** with workload ID, baseline, PGN location, and resume condition; hand collected PGNs to `tune-latrunculi-evaluation`. Do not start a second workload or poll automatically.

## Check or stop

Fetch a named test once. Report running games, score or Elo interval, and LLR where applicable, without putting intermediate snapshots on the board. For a terminal result, retain the [required evidence](references/workloads-and-decisions.md#evidence-and-candidate-decision). Tuning corpus scores, pass/fail flags, and manual stops do not decide a candidate; dataset preparation decides usability. Keep unfinished tuning work in **Issues and leads**, not candidate stages.

Apply candidate transitions only if candidate and baseline SHAs match the live board entry and the baseline is current. Move eligible accepted tests with required confirmation complete to **Ready for integration**, rejections to **Recent results**, and stopped or inconclusive tests to **OpenBench tests** with their next decision. Preserve published test identity if its baseline becomes stale. Report ad hoc comparisons without board edits.

Only an explicit stop request authorizes stopping a workload. If already terminal, report the result. Otherwise stop it, immediately read back the same test, and preserve any boundary decision reached. A pre-boundary stop is inconclusive; in-flight results may still arrive, so the immediate game count is a snapshot. Do not delete or restart tests or stop server or worker services.

Leave bookkeeping uncommitted unless authorized. Do not change account permissions to complete an operation; report access failures.
