---
name: test-latrunculi-candidate
description: Test one retained Latrunculi playing-strength candidate against the current operational baseline using cheap checks followed by the complete offline evidence. Use for formal offline testing, retesting a stale candidate, or resuming an interrupted offline test.
---

# Test a Latrunculi Candidate Offline

Use `docs/engine-development.md` for current work and search-testing rules. Read the applicable
measurement or evaluation-tuning guide. Work on one local CPU-sensitive task at a time.

## Prepare

1. Require a clear change, `candidate.patch`, tree classification, and claimed effect. Apply the
   domain defaults; require a board note only for an override, nonstandard mechanism check, or extra
   risk test. Return incomplete work to `explore-latrunculi-change`.
2. Inspect the current baseline and worktree. For a stale candidate, test the same claim against the
   current baseline; if adapting it changes behavior, return it to exploration.
3. Apply exactly the retained patch and review the resulting diff.

## Test

Run the domain guide's cheap deterministic checks first. Continue only when they pass, then run the
complete tests, reproducibility checks, timing panel, and each extra risk test required by the
change. Generated measurement files and the comparison summary are sufficient evidence; do not
create a separate command log, manifest, patch hash, or binary hash.

Fix only a mechanical implementation error needed to match the candidate, then restart the test. A
behavior change ends the run and returns the item to exploration under the same ID. Assign a new ID
only when both candidates must remain distinct. Rerun unchanged work only after an identified setup
or collection failure.

## Finish

- **Tree-preserving pass:** restore the baseline and move it to **Candidates** with `Next: integrate`.
- **Tree-changing pass:** restore the baseline and move it to **Candidates** with
  `Next: submit OpenBench`.
- **Rejected:** restore the baseline, move one concise result to **Recent results**, and remove the
  candidate's task-owned patch and raw output unless they are unusually informative. Never remove
  pre-existing historical artifacts.
- **Invalid or incomplete run:** restore the baseline and keep it in **Candidates** with
  `Next: test offline` and the exact resume condition.

For a pass, retain `candidate.patch`, the generated measurement inputs and summary, and one short
result. Never commit, create a candidate branch, push, run games, access OpenBench, or integrate.
