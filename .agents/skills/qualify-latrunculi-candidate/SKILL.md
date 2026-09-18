---
name: qualify-latrunculi-candidate
description: Qualify a concrete Latrunculi playing-strength candidate recorded in docs/playing-strength.md through frozen Screen and Qualification checks. Use for implementing, screening, qualifying, rejecting, resuming, or revalidating an offline candidate whose Change and decision rules can be fixed before measurement.
---

# Qualify a Latrunculi Candidate

`docs/playing-strength.md` owns task state, the operational baseline, candidate-specific rules, and
results. Read `docs/search.md` for search default panels and measurement meaning. Linear HCE
fitting follows `tools/tuning/workflow.md`; do not invent search-style panels for it here. Work on
one active offline task and one CPU-sensitive measurement stream at a time.

## Freeze the Candidate

1. Inspect the worktree, operational baseline, relevant code, tests, measurements, and history.
   Preserve unrelated work and archived evidence. Do not edit `docs/roadmap.md` unless requested.
2. Independently review any exploration proposal. Require a concrete candidate whose Change,
   evidence class, mechanism, applicable mechanism-aligned efficacy claim, thresholds, Screen,
   Qualification, and reject and qualify rules can be frozen. For exact-tree work, require a code-
   or build-level semantic-equivalence argument and use matching signatures only as corroboration;
   otherwise classify the candidate as tree-changing. Route a rough idea or open investigation to
   `$explore-latrunculi-candidates` rather than inventing missing rules. Pre-freeze exploratory
   measurements cannot satisfy Screen or Qualification.
3. Move or keep the task in **Active offline work** with `Phase: qualify` and complete its record
   before implementation. Record the initial worktree state in the artifact manifest.
4. For a fully specified candidate, omit `Variants`. Otherwise predeclare one single-factor variant
   family and its attempt or time limit. A normal upper bound is one initial attempt, one
   implementation repair, and one variant.
5. Freeze a targeted panel only when the candidate changes or relies on behavior outside the cold,
   one-thread, fixed-depth domain default, such as retained state, clock or limit handling, or
   threading and shared state. Use focused mechanism checks for sparse activation instead. Freeze
   the domain guide's complete panel contract and its phase, and run it alongside, never instead
   of, the applicable domain default.
6. Materialize the first attempt by implementing it or applying its retained patch. Before Screen,
   write the exact `Change`, record the candidate diff, and hash result-affecting inputs and
   measured binaries. Log each attempt as
   `attempt | change | result | decision`.

Keep the hypothesis, evidence class, variant range, checks, and pass/fail rules unchanged after
valid results arrive. Do not rescue a failed hypothesis by reclassifying it after seeing results;
a different kind of change or decision rule needs a new task ID.

## Run the Offline Loop

Use the same response to a failure in either phase:

- Rerun an unchanged candidate only when setup or data collection failed.
- If the code does not implement the written `Change`, repair it once.
- Try a behavior variant only when `Variants` permits it and budget remains.
- Otherwise reject the candidate.

Count only a frozen hard gate as a failure. Log any repair or variant, update the exact `Change`
when an allowed variant changes behavior, refresh the candidate diff and hashes, and restart
Screen. If later methodology review invalidates a measurement-system gate, follow the amendment
rule in `docs/playing-strength.md`.

### Screen

Run the focused checks, any frozen targeted Screen panel, and the applicable domain guide's Screen
panel without substituting another workload. Add instrumentation only when current output cannot
answer a question already in the task. Crashes, illegal or malformed output, protocol errors, and
failures of the domain's reproducibility rule count against the candidate.

If Screen passes, stop only when the request explicitly asked for Screen alone. Otherwise continue
to Qualification.

For a Screen-only request, preserve the frozen record and evidence, restore the operational
baseline, and return the same ID to the Pending queue marked
`Next: qualify; Screen passed; Qualification pending`. Resume at Qualification only when the
candidate, baseline, inputs, and measurement meaning remain unchanged; otherwise restart Screen.

### Qualification

Run the relevant complete release suite, the domain guide's Qualification panel, any frozen
targeted Qualification panel, and the task's remaining risk-specific checks. Follow the domain
guide's order, avoid duplicate coverage, and leave Qualification unresolved if required conditions
are unavailable. Build `release-stats` only when counters are needed; profiling, counters, and
disassembly are optional diagnostics.

Run ASan/UBSan only for a concrete risk involving storage, indexing, bounds, ownership, lifetime,
parsing, recursion, or core engine state. Run TSan only for shared state or worker lifecycle. Record
commands and results when sanitizers run, or why an applicable sanitizer was omitted.

Investigate conflicting checks before deciding; disagreement alone is not a chess-correctness
failure. Qualify only when every frozen hard requirement passes. Any repair or behavior change
restarts Screen and all of Qualification.

## Record the Result

For stale-candidate revalidation, temporarily move the candidate from Qualified candidates to
Active offline work. Retain its frozen hypothesis and acceptance contract unless an authorized
methodology amendment applies. Rerun each frozen targeted panel only when the newer baseline could
affect it; do not invent a replacement panel. A pass updates its qualified baseline and artifacts;
a valid failed gate moves it to the ledger; an unavailable required condition returns it to
Qualified candidates marked stale and revalidation unresolved. Preserve its historical
qualification evidence. This stale-candidate rule overrides the generic unresolved destination
below.

- **Rejected:** preserve the evidence, move the task to the ledger, remove candidate behavior and
  temporary support, and restore the baseline.
- **Stopped or unresolved:** preserve the evidence, move new work to the ledger as stopped or
  incomplete, remove candidate behavior, and restore the baseline.
- **Qualified:** move the task to the qualified table, record the comparable evidence needed for
  later selection, retain the final patch and hashes, and restore the baseline. Never stack an
  unintegrated candidate on later offline work.

Use `tools/measurements/output/<task-id>-<baseline>/` unless the domain already has another layout.
Keep the frozen predeclaration, raw evidence, one concise final manifest, and the candidate patch
and hashes. Remove only candidate-specific build output; never use broad `git clean` or delete
archived evidence.

Offline qualification does not authorize candidate selection, commits, branches, pushes, games,
OpenBench access, or integration. Route a qualified candidate to `$promote-latrunculi-candidate`.
When promotion requests stale-candidate revalidation, rerun only evidence the newer baseline could
affect. Report the result, decisive evidence, checks run, and final workspace state.

The primary agent owns task state, interpretation, and cleanup. Delegate at most one independent
development task when it materially improves the work; do not edit concurrently or run another
CPU-heavy measurement stream.
