---
name: advance-latrunculi-roadmap
description: Advance stable-ID work items in Latrunculi's docs/roadmap.md. Use when asked to select, investigate, revalidate, plan, implement, review, verify, complete, commit, push, or externally validate a roadmap item.
---

# Advance the Latrunculi Roadmap

Use `docs/roadmap.md` as the authoritative non-search backlog and work on one
coherent item. Use `$advance-latrunculi-search` for `SW-XX` experiments;
do not duplicate work between the coordinating documents.

## Establish Scope

1. Read the complete selected item, its context, and dependencies.
2. Inspect current source, tests, build configuration, history, and worktree.
   Read `docs/architecture.md` when ownership matters and consult local
   reference engines when useful.
3. Treat roadmap findings as hypotheses. Revalidate them against this
   implementation and authoritative specifications; reference behavior cannot
   override protocol, chess-rule, or project requirements.
4. Resolve material ambiguity from project evidence or ask the user. Preserve
   unrelated work and modify `docs/search.md` only for an explicitly requested
   cross-workstream transfer or cleanup.

## Follow the Requested Action

- For planning, return an implementation-ready plan without editing files or
  creating another plan document.
- For implementation, establish the plan from current evidence and proceed
  unless a consequential policy decision remains unresolved.
- Reviews, explanations, and prioritization are read-only.
- Commit or push only when explicitly authorized.
- Submit, modify, stop, or delete external validation only when explicitly
  authorized. Before OpenBench submission, read `docs/openbench.md` and
  verify the pushed immutable revisions, settings, and OpenBench-enforced
  termination budget; a missing or unenforced cap blocks submission.
  Read-only monitoring and retrieval need no further approval after submission.
- Never rewrite a revision used for external validation; record later results
  or cleanup in a new authorized commit.
- A request or active `/goal` may authorize several named stages. Otherwise
  each stage needs separate authorization; if no action is named, plan only.

Keep durable product direction in `docs/roadmap.md` and task-specific plans in
chat. For an audit, predeclare bounded questions, inputs, sample limits, stops,
and one completion artifact. Reuse existing tools, accept a null result, and do
not expand diagnostics merely to produce a finding.

## Implement and Finish

1. Limit changes to the selected item and necessary tests, documentation, and
   build integration. Follow live conventions and preserve unrelated work.
2. Prefer the simplest natural design, judged by readability and ergonomics
   rather than line count. Choose visibility for natural owners and callers;
   add abstractions, interfaces, and tests only for distinct risk.
3. Use correctness tests for behavior, `latrunculi-measure` for component
   performance, and OpenBench for playing-strength claims when required.
4. Run focused checks first, then broader tests and sanitizers, formatting,
   measurements, or external validation in proportion to risk.
5. Before finishing, remove redundant support code and tests; inspect the final
   diff, scope, formatting, and stale references.
6. Remove a genuinely completed item from `docs/roadmap.md` and adjust
   dependencies concisely; Git history records completion. Leave incomplete or
   planning-only items intact.
7. When authorized, create one focused commit including the roadmap update
   unless the user requests another boundary.
8. Completing one item does not authorize starting another unless the request
   or active `/goal` includes it.

Report the outcome, important decisions, verification, remaining risks, and
either the resulting commit or a concise suggested message.
