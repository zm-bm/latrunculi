# Candidate rules

## Candidate identity

Keep one active branch and, once published, one PR per candidate. Record full candidate and operational baseline SHAs with retained evidence; the board may use short SHAs. Preserve the experiment ID unless distinct candidates must coexist. Limit the candidate diff to its proposed change and supporting tests or build changes, with bookkeeping separate. Build measured revisions in clean, isolated worktrees.

Follow-up commits may stay on the same branch. Preserve commits supporting active review, measurements, or published tests; do not rewrite them. Incorporate baseline changes through a merge or another history-preserving adaptation, inspect the complete resulting diff, and record the new identities. Untested local history may be tidied without misidentifying earlier results.

## Acceptance

Before formal testing, record separate correctness, speed, and strength criteria, acceptable tradeoffs, and whether games are required.

- **Correctness:** Required regressions and correctness checks must pass. Speed or game results cannot compensate for a correctness failure.
- **Speed:** Support a claimed speedup with repeatable timing. Otherwise report the measured cost without requiring a gain.
- **Strength:** Use game evidence for strength claims. Broad search-policy changes and fitted evaluation normally require strength testing even when motivated by correctness or speed.

Tree classification describes search behavior; it does not alone decide game requirements. A narrow, demonstrated correctness fix may qualify through regressions and local checks without games. Do not revise acceptance criteria merely because results disappoint.

## Evidence reuse

Every measurement retains its actual candidate/base revisions and conditions, including original OpenBench identities and decisions. After a revision or baseline change, assess affected behavior, shared state, source/build inputs, compiler/settings, and relevant environment. Assess correctness, timing, and game evidence separately; record which results still apply, why, and which checks remain.

Mechanical or documentation changes may retain evidence when relevant inputs and behavior remain equivalent. A small diff alone is not proof. Combining engine changes requires local combination checks and assessment of search interactions; nonoverlapping files do not establish independence. Require fresh games when plausible interactions leave strength acceptance unresolved; otherwise record why prior results support acceptance.

Reuse does not relabel old measurements. Baseline advancement triggers reassessment rather than automatic rejection or blanket retesting; it does not by itself require a new OpenBench test or authorize stopping an existing one. Missing or invalid evidence remains unresolved.

## Approval and scope

Approval applies to the recorded current head and may be given on the hosting platform or in conversation. A clear integration request approving that revision supplies approval and merge authorization. Later code changes require review of their impact; passing tests or platform approval alone does not authorize merging.

By default, record the next action and stop at the skill boundary. Naming or linking another skill does not invoke it or authorize its actions. Continue across stages only when the user explicitly requested that continuation; do not ask again for stages or actions already authorized.

## Retention

Leave bookkeeping uncommitted unless requested otherwise. Remove task-owned temporary worktrees at handoff and preserve unrelated work. Keep current-baseline evidence, active candidates and supporting inputs, and useful audits. Integrated or abandoned branches and disposable outputs become eligible for separately requested cleanup; permanent references are unnecessary. Do not prune evidence needed by active work or delete published test records.
