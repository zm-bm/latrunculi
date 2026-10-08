---
name: review-latrunculi-candidate
description: Harden a promising Latrunculi candidate before formal testing or after review feedback. Explicit invocation publishes a draft PR by default; a local-only request retains the reviewed revision without publishing.
---

# Review a Latrunculi Candidate

Review a promising candidate for correctness and long-term maintainability before formal testing. Keep changes within its recorded purpose. By default, push the reviewed branch and open or update a draft PR. When the user requests local-only review, retain the reviewed revision without publishing it. This publication scope applies to explicit invocation; applying the skill to a generic review request does not itself authorize publication.

Resolve paths from the Latrunculi checkout. Read the [development board](../../../docs/engine-development.md) and shared [candidate identity](../../references/candidate-rules.md#candidate-identity), [approval and scope](../../references/candidate-rules.md#approval-and-scope), and [retention](../../references/candidate-rules.md#retention) rules. Use the [analysis guide](../../../tools/analysis/README.md) for checks. Apply the shared [measurement rules](../../references/measurement-rules.md) to local checks.

## Prepare

Identify the branch, candidate/base SHAs, intended behavior, [acceptance criteria](../../references/candidate-rules.md#acceptance), and existing PR or review. Inspect the complete candidate diff and baseline changes in an isolated worktree. Reuse relevant exploration evidence; run missing or affected cheap mechanism checks to establish that the candidate is worth hardening. Do not require a full offline campaign at this stage.

Keep a failed or unresolved mechanism in **Issues and leads** with the finding and next action. If only publication remains from an earlier review, verify its revision and evidence before completing that handoff without unnecessary edits or revalidation.

## Review

Review correctness, structure, names, API boundaries, invariants, comments, formatting, and test maintainability. Refactor where it makes the code easier to maintain; retain a sound implementation when no change is warranted. Explain lasting contracts in code and keep experiment history in evidence. Prefer focused tests over copying diagnostic machinery into the permanent suite.

Stay within the recorded purpose. For a new search idea or changed intended behavior, record exploration as the next action and stop review. Retain worthwhile changes as candidate commits, rerun affected cheap checks, and apply the shared [evidence-reuse rules](../../references/candidate-rules.md#evidence-reuse) to existing review and measurements. Record the final reviewed SHA and remaining risks or validation needs; no-change review also completes this stage.

## Hand off

For the default published workflow, verify the remote and target branch, push the candidate without rewriting tested history, and create or update its draft PR with the problem, behavior, tradeoffs, and concise validation tied to its revisions. Include applicable OpenBench links and keep large raw outputs outside the source diff. Human review may begin while the PR is a draft. Check for an existing PR first. If changes invalidate readiness or approval, return the PR to draft; a no-change publication retry need not downgrade a still-current ready PR. Read back its URL, head, base, and draft/readiness state. Publication failure leaves the local review intact with the unresolved action recorded.

Move the reviewed candidate to **Candidates for local testing** and record [test-latrunculi-candidate](../test-latrunculi-candidate/SKILL.md) as the next action, unless applicable existing evidence already supports a later stage. Record its PR or local-only status. Report changes, reviewed revision, affected evidence, next action, and worktree state. Stop after review; do not start formal offline testing, run games, or merge automatically.
