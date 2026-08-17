---
name: advance-latrunculi-roadmap
description: Advance stable-ID work items in Latrunculi's docs/roadmap.md. Use when asked to select, investigate, revalidate, plan, implement, review, verify, complete, or commit a Latrunculi roadmap item such as BENCH-001, REL-001, or MATH-001.
---

# Advance the Latrunculi Roadmap

Use `docs/roadmap.md` as the authoritative backlog and work on one coherent
roadmap item at a time. Read live project files instead of copying roadmap or
architecture content into this skill.

## Establish Scope

1. Read the complete selected item, its section context, and stated
   dependencies.
2. Read `docs/architecture.md` when subsystem ownership or dependency direction
   matters.
3. Inspect the current source, tests, build configuration, relevant history,
   and working-tree state before drawing conclusions.
4. Treat the roadmap finding as a hypothesis. Revalidate it against the current
   implementation and authoritative external specifications when applicable.
5. Consult the reference engines under `/home/rick/code/chess-engine-refs` when
   they provide useful implementation evidence. Do not let their behavior
   override an explicit protocol, chess-rule, or project policy requirement.
6. Identify ambiguities that would materially change behavior or scope. Ask the
   user only when source evidence and existing project policy do not resolve
   them.

## Follow the Requested Action

- For a planning request, return an implementation-ready plan in chat and do
  not edit files.
- For an implementation request, establish the plan from current evidence and
  proceed without a separate approval pause unless a consequential policy
  decision remains unresolved.
- For a review, explanation, or prioritization request, inspect and report
  without changing the repository.
- If the request names no action, default to planning rather than modifying the
  repository.

Do not create a separate plan document. Keep durable product direction in
`docs/roadmap.md`; keep task-specific implementation plans ephemeral.

## Make Focused Changes

1. Keep the change limited to the selected item and its necessary tests,
   documentation, and build integration.
2. Follow the current namespace, naming, formatting, ownership, and dependency
   conventions visible in the repository.
3. Preserve unrelated working-tree changes and avoid opportunistic cleanup.
4. Prefer the simplest design that satisfies the behavior and leaves clear
   ownership boundaries. Judge simplicity by readability and ergonomics, not
   merely line count or interface size.
5. Choose visibility according to the natural owner and callers. This is a
   private codebase; do not minimize member interfaces for their own sake.
6. Do not add an abstraction, helper, production seam, assertion, or test case
   without a distinct need.
7. Test behavior through its natural domain interface. Add integration
   coverage only for a distinct downstream risk, and avoid command,
   worker-count, or input cross-products without separate risk.
8. Before final verification, remove redundant code and tests. Require every
   retained construct and test case to protect distinct behavior or risk.
9. Use correctness tests for behavior, `latrunculi-measure` for component
   performance, and OpenBench for retained playing-strength claims when the
   item requires them.

## Verify and Finish

1. Run focused tests first, then the broadest relevant configured suite.
2. Run sanitizer, formatting, measurement, deterministic-output, or external
   validation workflows in proportion to the task's risk and completion
   criteria.
3. Inspect the final diff, changed-file scope, formatting, and stale-name
   searches. Confirm no unrelated roadmap work entered the change.
4. When the item is genuinely complete, remove it from `docs/roadmap.md` and
   adjust dependencies or ordering concisely. Do not retain a completion log.
5. Do not remove or rewrite the roadmap item when work is incomplete or only a
   plan was requested.
6. Create one focused commit only when the user explicitly authorizes a commit.
   Include the roadmap update with the completed work unless the user requests
   separate commits.

Report the outcome, important design decisions, verification performed,
remaining risks, and either the resulting commit or a concise suggested commit
message.
