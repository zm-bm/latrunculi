---
name: explore-latrunculi-change
description: Explore one bounded idea for improving Latrunculi's playing strength and turn it into one retained candidate, a null result, or a precise unresolved question. Use for rough ideas, the next queued experiment, profiling questions, temporary prototypes, or mechanism investigation before formal offline testing.
---

# Explore a Latrunculi Change

Use `docs/playing-strength.md` for current work and search-testing rules. Read
`tools/tuning/workflow.md` only for linear HCE fitting. Work on one local CPU-sensitive task at a
time.

## Explore

1. Use the named idea or existing queue ID. Select the first queue item only when the user asks for
   the next one. Leave casual exploration unnumbered.
2. Inspect the baseline, worktree, relevant code, tests, tools, and history.
3. Use the smallest useful inspection, instrumentation, prototype, or exploratory measurement.
   This evidence cannot replace the formal offline test. Stop when the question is answered or the
   next useful step is unavailable; do not broaden the task merely to produce a candidate.
4. Restore the baseline and remove temporary behavior and instrumentation before handing off.

## Finish

- **Candidate:** keep the queue ID or assign the next unused `EI-XXX`, create one artifact directory,
  save `candidate.patch` without a separate hash, keep only useful generated output and a short
  result, and add a **Candidates** row with its change, claimed effect, tree classification, key
  evidence, and `Next: test offline`. Record only a justified default-gate override, nonstandard
  mechanism check, or extra risk test. A tree-preserving claim still needs a code- or build-level
  equivalence argument.
- **Null result:** leave no board record unless it produced a durable lesson worth retaining.
- **Unresolved:** keep or assign an ID and return it to the **Queue** with the exact resume condition.

Report the outcome and workspace state. Do not run the formal offline test, commit, push, run games,
access OpenBench, or integrate.
