---
name: explore-latrunculi-change
description: Explore a supplied ENG-XXX issue or lead and turn it into one retained candidate, a null result, or a precise unresolved question. Use for profiling, temporary prototypes, or mechanism investigation before formal offline testing.
---

# Explore a Latrunculi Change

Resolve paths from the Latrunculi checkout. Read the current [development board](../../../docs/engine-development.md), [exploration evidence](references/exploration-evidence.md), and the shared [candidate identity](../../references/candidate-rules.md#candidate-identity) and [retention](../../references/candidate-rules.md#retention) rules. Consult [evidence reuse](../../references/candidate-rules.md#evidence-reuse) when revisiting prior work. If the work calls for linear HCE fitting, record `tune-latrunculi-evaluation` as the next action and stop exploration. Apply the shared [measurement rules](../../references/measurement-rules.md) to local checks.

## Explore

1. Read the supplied `ENG-XXX` issue or lead and its linked evidence for the question, scope, and next test. Treat old recipes as provisional.
2. Inspect the baseline, worktree, relevant code, tests, tools, and history. Keep exploratory code in an isolated worktree based on the operational baseline.
3. Use causal probes or bounded variant comparisons within the requested hypothesis. Stop when the question is answered or the next useful step is unavailable; do not broaden the task merely to produce a candidate.
4. Remove temporary instrumentation and unselected variants before retaining a candidate.

## Hand off

Summarize compared variants together and scope negative conclusions to the tested evidence. Preserve the supplied ID and record the baseline, tested scope, outcome, and reason for stopping on the board.

- **Candidate:** Retain one local candidate branch and commit under the shared identity rules. Move it to **Candidates for review** with its claim, branch, candidate and base revisions, evidence, and proposed [acceptance criteria](../../references/candidate-rules.md#acceptance). Record [review-latrunculi-candidate](../review-latrunculi-candidate/SKILL.md) as the next action, including existing cheap-check evidence. Record any nonstandard mechanism check, expected speed tradeoff, or extra risk test. A tree-preserving claim needs a code- or build-level equivalence argument.
- **Null:** Keep a compact **Recent results** entry with its tested scope and stopping reason.
- **Unresolved:** Keep the entry in **Issues and leads** with the exact next test or resume condition.

Before removing temporary worktrees, preserve any diagnostic code and inputs needed for the recorded next test.

Report the outcome and workspace state. Stop after exploration; do not begin review automatically. Do not run the formal offline pass, push, run games, access OpenBench, or integrate.
