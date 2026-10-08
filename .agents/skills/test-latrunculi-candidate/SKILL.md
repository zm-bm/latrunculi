---
name: test-latrunculi-candidate
description: Formally test a reviewed Latrunculi candidate, reassess evidence after changes, or resume an incomplete offline pass. Retain the measured revision and update its existing PR without starting games or integration.
---

# Test a Latrunculi Candidate Offline

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), shared [candidate identity](../../references/candidate-rules.md#candidate-identity), [evidence reuse](../../references/candidate-rules.md#evidence-reuse), and [retention](../../references/candidate-rules.md#retention) rules, and [offline checks](references/offline-checks.md). Use the [analysis guide](../../../tools/analysis/README.md) or [tuning guide](../../../tools/tuning/README.md) for commands. Apply the shared [measurement rules](../../references/measurement-rules.md) to local checks.

## Prepare

Require a recorded branch, full candidate/base SHAs, [acceptance criteria](../../references/candidate-rules.md#acceptance), tree classification, and applicable completed quality review. For missing or affected review, record [review-latrunculi-candidate](../review-latrunculi-candidate/SKILL.md) as the next action and stop testing. For unfinished exploration, return the entry to **Issues and leads**, record exploration as the next action, and stop. A local-only candidate does not need a PR to be tested.

Inspect the current baseline and complete candidate diff. If either revision changed, assess evidence applicability under the shared evidence-reuse rules and identify checks still needed. Build the recorded revision in a clean, isolated worktree.

## Test

Run missing cheap checks first, then complete required correctness, repeatability, Release, applicable risk/sanitizer checks, and paired timing in [offline checks](references/offline-checks.md). Reuse still-applicable results explicitly. For fitted weights, verify the compiled candidate against the retained fit; reuse verification only when its relevant inputs remain unchanged.

Use the shared `bench.py` interface for corpus passes, fingerprints and paired comparisons; completeness, legality and repeatability checks are built in. Run the standard comparison after required correctness checks. Add separate profiling only when needed to substantiate the claim, and keep experiment-specific mechanism checks separate. Generated measurements and a compact summary suffice; do not add a separate command log, manifest, or binary hash.

A required implementation fix creates a new revision; record review of the affected code and the checks its impact requires as the next action, then stop this test run. Do not silently test different code under the old identity. For a change in intended behavior, record exploration as the next action and end this run.

## Hand off

For a pass, retain the tested head and report `git diff <baseline>...<candidate SHA>` and `git log <baseline>..<candidate SHA>` for the complete candidate. Update its existing PR with concise results and make it ready for human review after verifying the remote head matches the tested revision. If publication remains, record the review skill's publication step as the next action; do not invoke it automatically or create a second PR.

Move passes with outstanding game requirements to **Ready for OpenBench**; move those with acceptance requirements satisfied to **Ready for integration**. Record any remaining PR publication action for a local-only candidate. Keep incomplete runs in **Candidates for local testing** with their resume condition and rejections in **Recent results**.

Stop after offline testing. Do not push candidate code, run games, access OpenBench, or merge automatically.
