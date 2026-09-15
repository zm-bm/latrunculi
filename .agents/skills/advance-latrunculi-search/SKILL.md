---
name: advance-latrunculi-search
description: Advance stable-ID Latrunculi search experiments coordinated in docs/search.md. Use when asked to select, plan, implement, screen, qualify, park, review, dispose of, commit, push, run local paired games, submit or monitor OpenBench, or complete an SW-XX search task.
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
3. Record task HEAD and worktree state in the artifact manifest. Pin the
   operational search baseline in the active task record, and account for every
   behavior-, build-, test-, or measurement-affecting difference.
4. Confirm one hypothesis, one reversible candidate, relevant panels, checks,
   task-specific stops or justified `N/A`, and one artifact root.
5. Treat the active task record as the predeclaration. Before Screen, finish
   implementation and review, then freeze the candidate, measured binaries, and
   result-affecting inputs. Restart Screen only when one of those changes.
   Documentation, formatting, comments, and test-only edits need targeted
   verification plus confirmation that the measured binary hash is unchanged.
6. Preserve unrelated work and sealed evidence. Change `docs/roadmap.md` only
   when the user requests a cross-workstream transfer or cleanup.

## Respect Authorization Boundaries

- Planning, review, explanation, and prioritization are read-only. An offline
  request authorizes implementation of the active candidate, its Screen and
  Qualification checks, raw evidence, task updates, and rejection cleanup.
- Commit, push, local paired games, and OpenBench mutations require explicit
  authorization in the current request or active `/goal`; authorization for
  one stage does not imply another. If several stages are named, execute them
  in order without asking again at each boundary.
- Publish an OpenBench candidate from an unmerged `sw-XX-<slug>` branch after
  verifying its exact diff and revision. Never amend, reset, force-push, or
  otherwise rewrite a published or tested revision.
- Before an authorized submission, read `docs/openbench.md`; use the private
  endpoint documented there and verify the immutable revisions, settings, test
  mode, and termination policy. Read-only status checks and result retrieval
  need no further approval after submission.
- If no action is named, plan only. A qualified candidate may be parked while a
  later task starts from the operational baseline; it is never an implicit
  baseline for later experiments.

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

1. Run the complete configured release suite. It subsumes focused tests unless
   the candidate changed after Screen, those tests are excluded, or another
   configuration must be checked. Use `release-stats` only when instrumentation
   or counters are needed.
2. Use ASan/UBSan only for concrete risk from storage, indexing expressions,
   bounds logic, ownership, lifetime, representation, parsing,
   recursion/depth arithmetic, or core board/search/TT state. Scalar constants,
   conditions, formulas, and compile-time table contents or limits are `N/A`
   when indexing is unchanged and a focused boundary test covers the new
   limit. Use TSan only for concurrency, shared state, or worker lifecycle.
   Record the concrete risk or `N/A` reason; sanitizers are not a general
   confidence check.
3. For deterministic search-tree candidates, run a second fresh-process
   200-position corpus pass and require exact agreement in the fields defined
   by `docs/search.md`. Evaluate wall-clock-limited panels as repeated
   distributions instead.
4. Run sentinel trajectories once for ordinary pruning, reduction, and ordering
   changes. Repeat complete trajectories only for iterative-deepening,
   aspiration, root, stopping, or clock changes; otherwise repeat only a
   potentially decisive regression.
5. Recheck applicable objective, legal, and protocol guards; run the benchmark
   a second time and require its fingerprint to repeat. Run repeated timing for
   clock or stopping changes and when hot-path work changed. Record and execute
   paired variants in their predeclared serial order. A missing,
   reordered, overlapping, interrupted, wrong-input, or unparseable run
   invalidates the whole batch; a slow valid run does not.
6. Complete only the mechanism-specific checks predeclared by the task.
   Correct, reproducible marginal candidates remain eligible for paired games.

Record a behavior-changing survivor as `qualified`. Park it as an immutable
commit or, when commits are not authorized, a final patch plus binary hashes;
then restore the operational baseline before another task starts. Never stack
an unvalidated candidate. If the operational baseline changes before paired
testing or integration, requalify the parked candidate against it. A
behavior-preserving optimization may finish offline when its exact-signature
and throughput requirements pass.

## Validate, Publish, Dispose, and Finish

1. For explicitly authorized paired strength testing, use OpenBench when
   available or a direct pinned `fastchess-ob` match. Verify the exact candidate
   and baseline first. Follow `docs/openbench.md` for OpenBench. For local games,
   preserve immutable source or a patch and binary hashes, fixed seed,
   runner/book hashes, identical paired settings and adjudication, raw log,
   PGN, pentanomial counts, and the result.
2. Predeclare the hypothesis and termination policy. Use SPRT for candidate
   strength by default and let it run to either LLR boundary. Use fixed games
   only when a fixed sample is itself the objective, with a positive even
   `max_games`.
3. Run one match at a time. Confirm its identity, revisions or binary hashes,
   settings, mode, bounds or fixed-game count, and running state once; record
   them and return control without polling. Inspect status again
   only in a later user request or goal continuation. Interpret upper bound as
   acceptance and lower bound as rejection. A manual stop before either bound
   is inconclusive; an infrastructure failure supplies no decision. Further
   testing needs separate justification and authorization.
4. For an unpublished rejection, keep referenced evidence and save a patch only
   when no immutable commit represents the candidate. Remove candidate behavior,
   temporary diagnostics, redundant tests, and exact candidate-only build output;
   never use broad `git clean` or delete sealed evidence.
5. Distinguish rejection from incomplete evidence caused by infrastructure
   failure; leave the latter active.
6. Future task artifacts need untouched raw output, one compact manifest,
   optional analysis, and a patch only when no immutable commit represents the
   candidate. Do not create separate predeclaration files.
7. Inspect final diff, status, artifact scope, and formatting. Preserve
   unrelated work, keep authorized commits focused, and do not start a pending
   task merely because the current one received a disposition.

Report the outcome, decisive evidence, verification, disposition, workspace
state, and next approval boundary.
