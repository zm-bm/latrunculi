---
name: test-latrunculi-candidate
description: Test one recorded Latrunculi candidate revision against the current operational baseline using cheap checks followed by the complete offline evidence. Use for formal offline testing, retesting a stale candidate, or resuming an interrupted offline test; retain the same passing revision for review.
---

# Test a Latrunculi Candidate Offline

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md), [candidate revision rules](../explore-latrunculi-change/references/candidate-revisions.md), and [offline checks](references/offline-checks.md). Use the [analysis guide](../../../tools/analysis/README.md) or [tuning guide](../../../tools/tuning/README.md) for commands. Work on one local CPU-sensitive task at a time.

## Prepare

1. Require a recorded branch, full candidate and baseline SHAs, intended benefit, tree classification, and claimed effect. Apply the checks for that purpose; record any nonstandard check, expected speed tradeoff, or extra risk test. Return unfinished exploration to **Issues and leads** and `explore-latrunculi-change`.
2. Inspect the current baseline and worktree. Keep pending or stale work in **Candidates for local testing**. Refresh a stale candidate as a new commit on the current baseline, preserving the old revision. If adaptation changes intended behavior, return it to exploration.
3. Review the recorded diff and build the exact candidate SHA in a clean, isolated worktree.

## Test

Run the cheap checks in [offline checks](references/offline-checks.md) first; continue if required checks pass, using numeric targets to decide whether more testing is worthwhile. Then complete correctness, repeatability, Release, applicable risk and sanitizer checks, and paired timing in the specified order. For fitted weights, run `python3 tools/tuning/tune.py verify <fit directory> --engine <recorded-commit binary>` against the retained fit even if tuning already verified it. Use [exploration evidence](../explore-latrunculi-change/references/exploration-evidence.md) to interpret cost and reference comparisons.

Generated measurement files and the comparison summary suffice; do not add a separate command log, manifest, or binary hash. Fix only a mechanical implementation error needed to match the claim. Retain it as a new revision, then restart testing. A change in intended behavior ends this run and returns the item to **Issues and leads** under the same ID. Assign a new ID only if both candidates must remain distinct.

## Finish

For a pass, retain the tested commit and report `git diff <baseline>..<candidate SHA>` and `git show <candidate SHA>` for review. Move tree-changing passes to **Ready for OpenBench** and tree-preserving passes to **Ready for integration**.

Keep incomplete or invalid runs in **Candidates for local testing** with the remaining checks and exact resume condition; move rejections to **Recent results**. For every outcome, retain the revision and a compact result with its stopping stage and reason. Preserve the inputs, corpus TSVs, paired timing files, fingerprint results, and correctness evidence supporting a pass or needed to resume incomplete testing. Remove disposable output; preserve historical artifacts and Git references. Clean up temporary worktrees and preserve unrelated work. Create only refreshed local candidate commits; leave bookkeeping uncommitted. Never push, run games, access OpenBench, or integrate.
