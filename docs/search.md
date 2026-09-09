# Search Development

This is the source of truth for `SW-XX` search experiments. It holds the operational baseline,
durable evidence, workflow, and queue; search work is not duplicated in `docs/roadmap.md`.

## Direction and baseline

Improve useful depth and convergence through better selectivity without sacrificing correctness,
move quality, or wall-clock performance. Reported depth alone is not an optimization target.

Update these fields together only after an approved candidate is integrated:

| Operational field | Current value |
|---|---|
| Search-behavior baseline | `6767e7447bfa285bef1daeea30d62f8a69367c42` (`6767e74`) |
| Deterministic benchmark | 6,068,328 nodes |
| Cached corpus baseline | `tools/measurements/output/search-baseline-6767e74/` |

The cached rows use `search_measurement_v3` and `tools/measurements/search.epd`. Reuse a retained
candidate's validated rows; recollect only when behavior, workload, or measurement definitions change.

The [measurement guide](../tools/measurements/README.md) owns the command interface. Keep its evidence
panels distinct:

- focused mate and material tests are objective correctness guards;
- all 200 source-pinned Arasan positions in `tools/measurements/search.epd` are the
  broad tactical performance corpus; their `bm` and `am` labels are context, not
  correctness or strength oracles;
- `tools/measurements/search-sentinels.epd` contains four focused convergence cases for mechanisms
  that can affect convergence; and
- task-specific timed, scaling, or protocol panels cover clock, stopping, threading, Hash, and protocol work.

Fresh-process, one-thread deterministic repeats are reproducible when completed depth, score,
actual nodes, best move, and PV agree exactly; timing and NPS are excluded. For sentinel
trajectories, useful depth is the greatest depth `d >= 3` for which `d-2..d` share a root move
and score class, both adjacent PV transitions have a nonempty common prefix, and objective predicates hold.

## Durable evidence

| Area | Evidence | Decision |
|---|---|---|
| Reproducibility and Hash | All 33 fixed-node triplicate groups were exact, dev/stats signatures agreed, and 32 versus 256 MiB Hash changed no result. | No run-to-run instability or normal-Hash defect was found. |
| Pilot roots | Three of four historical pilot moves were not reproduced at 524,288 nodes, with no shared counter profile. | Pilots provide convergence context, not a correct move or mechanism. |
| Objective guards | Mate in one was stable at depth 1 and 46 nodes; mate in two at depth 3 and 767 nodes; rook capture at depth 1 and 29 nodes. | Keep them as focused tests, not performance rows. |
| Convergence | The 14-case audit had five late root changes, one sign flip, one A-B-A, a 46 cp late-jump p90, 61.5% median PV survival, and five zero-prefix transitions. | Depth-to-depth movement is convergence behavior, not nondeterminism. |
| Capture ordering | Ordinal-2-or-later captures succeeded 29,374 times in 504,598 attempts (5.82%): 7.20% in exact-SEE-good and 0.47% in exact-SEE-bad. CaptureHistory used 0.9716 times the nodes but 1.0646 times the wall time; start position took 1.4129 times as long. | The tested table was rejected and removed; preserve existing SEE bands. |
| Qsearch pruning | Ordinary exact-SEE-negative captures are already excluded. Margins 200, 300, and 400 would have skipped 3,200, 732, and 206 real NonPV cutoffs. | Do not retry adding SEE pruning or those margin rules. |
| History-aware LMR | All 107 sampled high-history reduced fail-lows remained fail-lows at full depth. | Do not retry the tested one-ply protection without new evidence. |
| Game-clock stop | Unfinished iterations used median shares of 24-26%, but all 210 timed searches matched fresh-depth results; tested predictors fired prematurely 21, 65, and 129 times. | The observed waste is not nondeterminism; reject those predictors. |
| Null move and futility | Existing counters lack eligibility and counterfactual-failure denominators. | Neither mechanism is disproved, but the audit did not justify a change. |
| Aspiration window | The 32 cp window passed correctness and reproducibility but regressed the performance cases substantially. | Retain 50 cp; see SW-01. |

The focused convergence sentinels are:

| Case | Horizon / useful depth | Durable signal |
|---|---:|---|
| `startpos` | 14 / 11 | Late root change and 15.4% late PV survival. |
| `arasan20-16` | 16 / 14 | Two late root changes, one A-B-A, and 0% late PV survival. |
| `pilot14-g171-abrupt` | 18 / 17 | Root changed `f5f4 -> h4h5` between 4M and 33M nodes, with no shared PV prefix. |
| `pilot15-g078-secondary` | 13 / 11 | Late score-sign flip and 7.7% late PV survival. |

Their baseline horizons are in
`sa-02-470a3d7/derived/resolved-horizons.tsv` beneath the measurements output.

Do not retry rejected shapes without materially new evidence. The exact CaptureHistory
patch is
`tools/measurements/output/sa-03-capture-history-b415f5a/meta/candidate.patch`.
The rejected qsearch rule was `stand_pat + captured_value + margin <= alpha_before_move`
for margins 200, 300, and 400 among eligible exact-SEE-nonnegative NonPV captures with
tactical exemptions. The rejected clock predictor was
`T_d + m * (T_d - T_(d-1)) >= allocated_time` for `m` in `{1, 2, 4}`. The
high-history LMR trial protected quiet moves with combined history at least 1024.

The corpus is tactical offline evidence, not Elo, and clock evidence came from
one machine and thread. Explicit `movetime` is a hard-duration request. NonPV
qsearch usually exposes improvement as a beta cutoff rather than an alpha
raise, so zero alpha raises alone do not make pruning safe. Aggregate counters
may overlap iterations or aspiration attempts; require mechanism-specific
eligibility and counterfactual denominators.

## Experiment workflow

Tasks use sequential `SW-XX` IDs and states `pending`, `active`, `done`, `rejected`,
and `skipped`. Keep at most one active task; a candidate awaiting approval remains
active. Each task changes one mechanism and records:

```text
SW-XX — title [state]
Hypothesis / candidate:
Baseline / panels / stops:
Result / artifacts:
Disposition / next boundary:
```

Before implementation, pin the operational baseline and predeclare the
candidate, relevant panels, checks, artifact root, and one material stop for
the mechanism—or `N/A` with a reason. Search results may change;
candidate-internal reproducibility and objective correctness are invariants.
Finish implementation and review, then freeze the candidate and result-affecting
build inputs before Screen; record their patch or revision and binary hashes. A
later candidate or build-input change requires an updated predeclaration and a
fresh Screen. Do not add post-hoc cases or diagnostics to rescue or condemn a
decided result.

### Phase 1 — Screen

Run only enough to reject a clear failure:

1. Run focused correctness checks.
2. Run one relevant measurement pass. For pruning, reductions, move ordering,
   aspiration, and comparable deterministic search changes, use the complete
   200-position corpus at depth 10, one thread, 32 MiB Hash, one fresh process,
   and one internal repetition. For clock, stopping, threading, Hash, or
   protocol work, use a task-specific timed, scaling, or protocol panel instead.
3. Run one benchmark for a search or binary candidate.
4. Add timing or sentinel trajectories only when the mechanism makes them
   relevant. Repeated timing belongs in Qualification unless one run already
   establishes the predeclared stop.

Compare a corpus pass with the cached baseline using geometric mean, median,
and improved/regressed case counts. There is no universal node threshold. If a
predeclared stop fires, reject immediately, preserve the evidence, remove the
candidate and temporary support, restore the baseline, and do not run
Qualification. A correct marginal or mixed result remains eligible to advance.

### Phase 2 — Qualification

Only a Screen survivor proceeds:

1. Run focused tests and the complete `release-dev` CTest suite. Run the complete
   `debug-asan-ubsan` suite when a candidate changes storage, indexing, bounds or
   depth arithmetic, ownership, lifetimes, representations, parsing, or
   board/search/TT state. Run `debug-tsan` for concurrency, shared state, or
   worker lifecycle. A scalar constant, threshold, weight, or condition-only
   change using existing storage and indexing may declare sanitizers `N/A` with
   a reason; when uncertain, run the relevant suite. Do not weaken checks to
   accommodate the runner—rerun the unchanged command in a compatible
   environment.
2. Run a second fresh-process copy of every deterministic decision panel. The
   two candidate passes must agree exactly in completed depth, score, actual
   nodes, best move, and PV; exclude timing and NPS. Evaluate wall-clock-limited
   panels through repeated distributions instead.
3. Recheck every applicable mate, material, legal, and protocol guard.
4. Run the benchmark again and require the candidate fingerprint to repeat;
   record a legitimate new value instead of forcing the baseline value.
5. Use repeated timing for clock or stopping changes and for candidates that
   add or remove hot-path work, then complete any other predeclared checks.
   Record and execute paired variants in a predeclared literal serial order.
   A missing, reordered, overlapping, interrupted, wrong-input, or unparseable
   run invalidates the whole batch; rerun it before analysis. A slow valid run
   is not malformed.

A behavior-preserving optimization may finish offline when the task requires
exact baseline signatures and its throughput gate passes. A behavior-changing
candidate that passes both phases is offline-qualified for paired games, even
when its performance dashboard is marginal or mixed.

### Publish and OpenBench

Offline qualification does not authorize publication. Commit, push, and
OpenBench actions proceed only when the current request or active `/goal`
explicitly authorizes them; authorization for one stage does not imply another.
Publish from an unmerged `sw-XX-<slug>` branch and never rewrite a published or
tested revision. Follow the [OpenBench guide](openbench.md), record the test ID,
profile, revisions and benchmark fingerprints, OpenBench revision, decision,
and PGN artifact. An upper-bound result qualifies for approved integration, a
lower-bound result rejects, and an inconclusive result remains active. Only an
approved integrated revision updates the operational baseline table.

## Evidence and cleanup

The immutable audit anchor is
`470a3d75c31a13e5f791dc0ee2476c6974be346d` (`470a3d7`). Sealed roots beneath
`tools/measurements/output/` are `search-001-470a3d7/`, `sa-02-470a3d7/`,
`sa-03-470a3d7/`, `sa-03-capture-history-b415f5a/`, `sa-04-470a3d7/`,
`sa-05-470a3d7/`, `sa-05-verifier-25bb133/`, and `sa-07-470a3d7/`; do not
modify them. Frozen manifests are authoritative. Retrieve the deleted audit
document from commit `8c0d46d` only for forensic replay.

Put new evidence in `tools/measurements/output/sw-XX-<baseline>/`: untouched
output in `raw/`, one compact `meta/` manifest with revisions, hashes, and
commands, and optional analysis in `derived/`. Save a candidate patch only
when no immutable commit represents it. For a rejection, retain referenced
evidence, remove candidate behavior and temporary support, and verify baseline
restoration before opening another task.
Completing one task never authorizes starting the next.

## Active and queued experiments

### SW-01 — Narrow the aspiration window [rejected]

- **Hypothesis / candidate:** Narrow the aspiration window from 50 to 32 cp.
- **Baseline / panels / stops:** `6767e74`; focused objectives,
  reproducibility, convergence, corpus efficiency, and benchmark.
- **Result / artifacts:** Correctness, reproducibility, convergence, and
  benchmark checks passed, but the mixed-case node ratio was 1.0911 and the
  corrected performance-only ratio was 1.1227, with a 1.1512 median and nine
  of eleven cases regressing. Candidate benchmark: 5,522,421 nodes. Broad
  CTest and sanitizer suites were not run after Screen rejection. Evidence:
  `tools/measurements/output/sw-01-6767e74/`.
- **Disposition / next boundary:** Rejected and removed. The 50 cp baseline
  again produced 6,068,328 nodes. Methodology cleanup does not reopen this
  large, repeatable regression; retry 32 cp only with materially new evidence.

### SW-02 — Precompute LMR reductions [rejected]

- **Hypothesis / candidate:** Replace the hot-loop logarithms with one immutable
  reduction table keyed by node type, move class, killer status, depth, and move
  count. Preserve every exemption and the current formula's integer result.
- **Baseline / panels / stops:** `cef892a1266d5d7a924a4e7fad6cad5d276b886e`
  (`cef892a`; search behavior `6767e74`); focused search/objective tests, the
  200-position depth-10 corpus, deterministic benchmark, and paired benchmark
  timing. Reject in Screen on any correctness, corpus-signature, or benchmark-node
  change, or a same-core candidate benchmark at least 5% slower than its immediately
  preceding baseline run. Qualification requires two exact candidate corpus passes
  and seven alternating same-core benchmark pairs after warm-up, with at least 1%
  median paired NPS improvement and at least five candidate wins.
- **Result / artifacts:** Focused and complete release suites passed; the table
  candidate also passed ASan/UBSan before two final assertion-only edits. The
  final release suite passed, and the throughput rejection made another
  sanitizer run unnecessary. Two fresh candidate corpus passes matched each
  other and the cached baseline exactly across all 200 cases (73,576,036 nodes),
  and every benchmark retained the 6,068,328-node fingerprint. Across seven
  alternating same-core pairs, the candidate was faster once; median NPS fell
  1.767% and mean NPS fell 1.252%. Evidence:
  `tools/measurements/output/sw-02-cef892a/`.
- **Disposition / next boundary:** Rejected for failing the predeclared throughput
  gate and removed. Rebuilt release binaries match the preserved baseline byte for
  byte, and the restored benchmark produced 6,068,328 nodes. SW-03 was activated
  separately from the clean `72d2c88` boundary.

### SW-03 — Tune quiet LMR selectivity [active]

- **Hypothesis / candidate:** Change only the quiet LMR divisor from 2.5 to
  2.4; leave noisy moves, the fourth-move threshold, history, PV/killer
  multipliers, exemptions, clamping, and re-search behavior unchanged. Because
  the multipliers follow the divisor, eligible quiet PV and killer moves can
  still receive the candidate's one-ply increase.
- **Baseline / panels / stops:** `72d2c889545dd761738b38dfa2a94cb8ac1fc525`
  (`72d2c88`; search behavior `6767e74`); focused LMR and objective tests, the
  200-position depth-10 corpus, the four sentinel trajectories through their
  recorded horizons, and the deterministic benchmark. Reject in Screen if the
  corpus geometric-mean node ratio is at least 1.05, or at least two sentinels
  lose at least two plies of useful depth. Correctness and candidate-internal
  reproducibility remain invariants. `release-stats`, sanitizers, and repeated
  timing are `N/A` while the candidate remains a scalar-only change using
  existing storage and indexing.
- **Result / artifacts:** Both phases passed. Two candidate corpus passes were
  exact internally: geometric-mean node ratio 0.9736, median 0.9779, with 135
  positions improving and 65 regressing. Useful depths were 11, 14, 18, and 9
  versus baseline 11, 14, 17, and 11, so only one sentinel met the two-ply-loss
  condition. Seven focused tests passed twice, the complete `release-dev` suite
  passed 3/3, and the candidate benchmark repeated at 6,609,227 nodes. Evidence:
  `tools/measurements/output/sw-03-72d2c88/`.
- **Disposition / next boundary:** Offline-qualified and still active. The
  corpus was more selective and the predeclared convergence stop did not fire;
  these are screening results, not strength evidence. Do not retry high-history
  protection or revert the fourth-move stabilization. Publication and paired
  validation are authorized from `sw-03-quiet-lmr`: test the immutable candidate
  against parent `72d2c88` with STC SPRT `[0,3]`, `10+0.1`,
  `UHO_Lichess_4852_v1.epd`, `Threads=1`, `Hash=32`, and standard adjudication.
  Keep the task active until the terminal result; integration remains a separate
  approval boundary.

After these tasks, select at most one small mechanism from current code and
game evidence. Do not open it until the preceding candidate has a disposition;
a new diagnostic audit is not required for a reversible production experiment.
