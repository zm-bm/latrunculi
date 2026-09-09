---
name: advance-latrunculi-search
description: Advance stable-ID Latrunculi search experiments coordinated in docs/search.md. Use when asked to select, plan, implement, measure, review, dispose of, commit, push, submit or monitor OpenBench, or complete an SW-XX search task.
---

# Advance Latrunculi Search

Use `docs/search.md` as the authoritative search-work coordination document.
Work on one active `SW-XX` experiment at a time and follow the live document's
current experiment definition, gates, evidence policy, and disposition rules.
Do not duplicate task definitions or numerical gates in this skill.

## Establish Scope

1. Read the complete active task and every shared section of `docs/search.md`
   that governs its execution.
2. Inspect the working tree, operational baseline, relevant source and tests,
   build configuration, existing measurements, and useful history before
   changing anything.
   Read `docs/architecture.md` when ownership or dependency direction matters,
   and consult locally available reference engines when they provide useful
   evidence. Revalidate any borrowed idea against this engine.
3. Before collecting evidence, record HEAD and working-tree state. Account for
   every change affecting search, evaluation, measurements, tests, or build
   configuration; do not measure an unrecognized behavior-affecting diff.
4. Revalidate the stated hypothesis and candidate against the current code.
   Resolve material ambiguity from project evidence or ask the user when it
   would change the experiment.
5. Preserve unrelated work and sealed evidence. Do not duplicate an experiment
   in `docs/roadmap.md`; modify that document only when the user explicitly
   requests a cross-workstream transfer or coordinating-document cleanup.

## Respect the Requested Stage

- For planning, return an implementation-ready plan without editing files.
- For an offline-only request, implement only the active candidate, run the
  required offline checks and measurements, and update its record. Do not
  commit, push, or start OpenBench.
- For review, explanation, or prioritization, inspect and report without
  changing the repository.
- Commit or push only when explicitly authorized. When an OpenBench candidate
  is published, use an `sw-XX-<slug>` task branch and verify the exact candidate
  diff and revision before each operation.
- Submit, modify, stop, or delete an OpenBench test only when explicitly
  authorized. Read `docs/openbench.md` and verify the pushed immutable
  revisions and test settings before submission. After submission is
  authorized, read-only status checks and result or PGN retrieval require no
  further approval.
- An active user request or active `/goal` may conditionally authorize multiple
  named stages. Otherwise, authorization for one state-changing stage does not
  imply authorization for a later one.
- If no action is named, default to planning.

## Run One Reversible Experiment

1. Pin the operational baseline and confirm the active record defines one
   hypothesis, one candidate shape, one artifact directory, and task-specific
   offline stop conditions.
2. Change one search mechanism only. Add diagnostics only for a decision the
   existing measurement interface cannot support.
3. Run only the predeclared checks, in cheapest decisive order. Stop after a
   rejection condition fires, and do not add post-hoc cases, counters, or
   diagnostics to rescue, condemn, or fully explain a decided candidate. Carry
   an unexpected signal into later work only after disposing of the active task.
4. Use correctness tests for behavior, `latrunculi-measure` for offline search
   evidence, and paired OpenBench games for playing-strength claims, as
   required by `docs/search.md`.
5. Preserve raw output beneath the task-specific ignored artifact directory
   and distill only decision-relevant evidence into `docs/search.md`.
6. Treat any later candidate behavior, build-configuration, or
   measurement-method change as invalidating the affected evidence. Rerun
   those gates and refresh provenance before commit, push, or OpenBench.
7. Keep the task active while it awaits approval. Do not begin a pending task
   until the current task has a recorded disposition.

## Dispose and Finish

1. If an unpublished candidate fails, preserve the referenced evidence and
   patch, remove its behavior code, temporary diagnostics or toggles,
   redundant tests, and candidate-only build output using exact
   candidate-owned paths, then verify that the retained baseline is restored.
   Never use broad `git clean` or delete sealed evidence.
2. Treat every published or externally tested revision as immutable. Never
   amend, reset, force-push, or otherwise rewrite it. For a rejected unmerged
   candidate, record the result before abandoning or deleting its task branch,
   and obtain authorization for branch deletion. Remove a candidate that was
   already integrated with a new authorized commit.
3. If the candidate passes its authorized stage, stop at the next approval
   boundary and report exactly what remains unauthorized.
4. Distinguish a rejected candidate from incomplete evidence caused by a build,
   tool, or infrastructure failure; leave the latter active.
5. Update the task's compact record and state without deleting useful negative
   results or rewriting the immutable audit findings.
6. Recording a disposition does not authorize activating or implementing the
   next task unless the active request or `/goal` includes it.
7. Inspect the final diff, status, artifact scope, and formatting. Confirm that
   no unrelated work or roadmap change entered the result.
8. Keep each authorized commit focused. Do not squash or amend a revision that
   has been published or tested; a later result or disposition may require a
   follow-up documentation commit. Update the operational baseline only after
   the retained candidate and its required validation have been approved.

Report the outcome, key evidence, verification performed, disposition,
workspace state, and the next approval boundary.
