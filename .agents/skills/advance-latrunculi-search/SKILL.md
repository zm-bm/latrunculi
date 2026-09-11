---
name: advance-latrunculi-search
description: Advance stable-ID Latrunculi search experiments coordinated in docs/search.md. Use when asked to select, plan, implement, screen, qualify, review, dispose of, commit, push, submit or monitor OpenBench, or complete an SW-XX search task.
---

# Advance Latrunculi Search

Use `docs/search.md` as the authority for the operational baseline, evidence
panels, workflow, and `SW-XX` tasks. Work on at most one active experiment and
do not duplicate it in `docs/roadmap.md`.

## Establish Scope

1. Read the complete active task and every shared section governing it.
2. Inspect the worktree, operational baseline, relevant source, tests, build
   configuration, measurements, and history. Revalidate borrowed ideas in this
   engine.
3. Record HEAD and worktree state before measuring. Account for every
   behavior-, build-, test-, or measurement-affecting difference.
4. Confirm one hypothesis, one reversible candidate, relevant panels, checks,
   task-specific stops or justified `N/A`, and one artifact root.
5. Before Screen, finish implementation and review, then record and freeze the
   candidate plus result-affecting build inputs. Any later change to either
   requires an updated predeclaration and a fresh Screen.
6. Preserve unrelated work and sealed evidence. Change `docs/roadmap.md` only
   when the user requests a cross-workstream transfer or cleanup.

## Respect Authorization Boundaries

- Planning, review, explanation, and prioritization are read-only. An offline
  request authorizes implementation of the active candidate, its Screen and
  Qualification checks, raw evidence, task updates, and rejection cleanup.
- Commit, push, and OpenBench mutations require explicit authorization in the
  current request or active `/goal`; authorization for one stage does not imply
  another. If several stages are named, execute them in order without asking
  again at each boundary.
- Publish an OpenBench candidate from an unmerged `sw-XX-<slug>` branch after
  verifying its exact diff and revision. Never amend, reset, force-push, or
  otherwise rewrite a published or tested revision.
- Before an authorized submission, read `docs/openbench.md`; use its configured
  Tailscale Serve endpoint and verify the immutable revisions, settings, test
  mode, and termination policy. Read-only status checks and result retrieval
  need no further approval after submission.
- If no action is named, plan only. Keep an offline-qualified task active while
  it awaits its next authorized stage.

## Phase 1: Screen

1. Pin the operational baseline and implement only the predeclared mechanism.
   Add diagnostics only when current output cannot answer a named decision.
2. Run focused correctness checks, one cheapest relevant measurement pass, and
   one benchmark for a search or binary candidate.
3. Use the full tactical corpus for pruning, reductions, move ordering,
   aspiration, and comparable deterministic search behavior or efficiency
   changes. Use task-specific timed, scaling, or protocol panels for clock,
   stopping, threading, Hash, and protocol work instead.
4. Run timing or convergence sentinels only when the mechanism makes them
   relevant. Keep objective, broad-performance, and focused-sentinel evidence
   separate; do not rerun sealed audit matrices unless explicitly required.
5. Apply only the task's predeclared material stop. There is no universal node
   threshold. Stop immediately on rejection; do not run Qualification or add
   post-hoc cases and diagnostics to rescue or condemn the candidate.

Preserve the decisive raw output and compact manifest, remove a rejected
candidate and temporary support, and verify baseline restoration before doing
anything else. Unexpected evidence may inform a later task only after the
current task has a disposition.

## Phase 2: Qualification

Only a Screen survivor proceeds:

1. Run focused tests and the complete configured release suite. Select complete
   sanitizer suites by changed risk: ASan/UBSan for storage, indexing, bounds,
   depth arithmetic, ownership, lifetime, representation, parsing, or core
   board/search/TT state; TSan for concurrency, shared state, or worker
   lifecycle. A scalar tuning change using existing storage and indexing may
   record sanitizers as `N/A` with a reason; when uncertain, run the relevant
   suite. If its runtime cannot operate in the runner, repeat the unchanged
   command in a compatible environment rather than weakening it. Use
   `release-stats` only when instrumentation or counters are needed.
2. Run the second fresh-process copy of each deterministic decision panel and
   require candidate-internal exact agreement in the fields defined by
   `docs/search.md`. Evaluate wall-clock-limited panels as repeated
   distributions instead.
3. Recheck applicable objective, legal, and protocol guards; repeat the
   benchmark; and run repeated timing for clock or stopping changes and when
   hot-path work changed. Record and execute paired variants in their
   predeclared literal serial order. A missing, reordered, overlapping,
   interrupted, wrong-input, or unparseable run invalidates the whole batch; a
   slow valid run does not.
4. Complete only the mechanism-specific checks predeclared by the task.
   Correct, reproducible marginal candidates remain eligible for paired games.

Record a survivor as offline-qualified and leave it active at the next
authorization boundary. A behavior-preserving optimization may finish offline
when its task's exact-signature and throughput requirements pass; a behavior
change requires paired games for strength evidence.

## Publish, Dispose, and Finish

1. For authorized publication or testing, verify the exact candidate revision
   and follow `docs/openbench.md`. Use SPRT for candidate strength by default;
   use fixed games only when a fixed sample is itself the objective, never as a
   substitute for an SPRT resource ceiling.
2. Predeclare the hypotheses and the guide's OpenBench-enforced game budget.
   Less than the default is allowed; more needs explicit user authorization.
   A missing or unenforced cap blocks submission.
3. After submission, perform one status read to confirm the test identity,
   revisions, settings, mode, positive limit, bounds or fixed-game count, and
   running state. Record them, leave the task active, report the handoff, and
   return control. Do not poll, sleep, or keep the turn open while games run.
   Inspect status again only in a later user request or goal continuation. A
   budget-only SPRT finish is inconclusive; further testing needs separate
   justification and authorization.
4. For an unpublished rejection, keep referenced evidence and save a patch only
   when no immutable commit represents the candidate. Remove candidate behavior,
   temporary diagnostics, redundant tests, and exact candidate-only build output;
   never use broad `git clean` or delete sealed evidence.
5. Distinguish rejection from incomplete evidence caused by infrastructure
   failure; leave the latter active.
6. Inspect final diff, status, artifact scope, and formatting. Preserve
   unrelated work, keep authorized commits focused, and do not start a pending
   task merely because the current one received a disposition.

Report the outcome, decisive evidence, verification, disposition, workspace
state, and next approval boundary.
