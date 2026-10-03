---
name: explore-latrunculi-change
description: Explore one bounded idea for improving Latrunculi's playing strength and turn it into one retained candidate, a null result, or a precise unresolved question. Use for rough ideas, an issue or lead, profiling questions, temporary prototypes, or mechanism investigation before formal offline testing.
---

# Explore a Latrunculi Change

Resolve paths from the Latrunculi checkout. Read the current [development board](../../../docs/engine-development.md), [exploration evidence](references/exploration-evidence.md), and [candidate revision rules](references/candidate-revisions.md). Route linear HCE fitting to `tune-latrunculi-evaluation`. Work on one local CPU-sensitive task at a time.

## Explore

1. Use the requested idea or existing ID. If asked for the next item, choose from **Issues and leads** using current evidence. Treat old recipes as provisional; leave casual exploration unnumbered.
2. Inspect the baseline, worktree, relevant code, tests, tools, and history. Keep exploratory code in an isolated worktree based on the operational baseline.
3. Use causal probes or bounded variant comparisons within the requested hypothesis. Stop when the question is answered or the next useful step is unavailable; do not broaden the task merely to produce a candidate.
4. Remove temporary instrumentation and unselected variants before retaining a candidate.

## Finish

Summarize compared variants together and scope negative conclusions to the tested evidence. Record the baseline, tested scope, outcome, and reason for stopping on the board.

- **Candidate:** Keep its ID or assign the next unused `ENG-XXX`. Retain one local candidate branch and commit under the [candidate revision rules](references/candidate-revisions.md). Move it to **Candidates for local testing** with its claim, branch, candidate and base revisions, evidence, and next action. Record any nonstandard mechanism check, expected speed tradeoff, or extra risk test. A tree-preserving claim needs a code- or build-level equivalence argument.
- **Null:** Keep a compact **Recent results** entry with its tested scope and stopping reason.
- **Unresolved:** Keep or assign an ID in **Issues and leads** with the exact next test or resume condition.

Remove task-owned temporary worktrees and preserve unrelated work. Report the outcome and workspace state. Create only the retained local candidate commit; leave board and evidence bookkeeping uncommitted. Do not run the formal offline pass, push, run games, access OpenBench, or integrate.
