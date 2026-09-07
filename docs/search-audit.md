# Search Audit

## Immutable SEARCH-001 baseline

SEARCH-001 audited the current search before any pruning change. The audit did
not isolate a search mechanism that satisfies the roadmap evidence gate, so it
does not propose a search-behavior follow-up. Three of the four primary pilot
moves were no longer selected at the representative fixed-node budget. The one
remaining primary case changed score, but not move, at the larger budget and
did not share a distinguishing counter profile with the controls.

### Baseline and provenance

- Baseline: `470a3d75c31a13e5f791dc0ee2476c6974be346d`
  (`v1.0.0-7-g470a3d7`), with a clean worktree before measurement.
- System: Linux 7.0.11 x86-64, Intel Core i7-11800H, Clang 18.1.3, and CMake
  3.28.3.
- Builds: the existing `release-dev` and `release-stats` presets. Both used one
  search thread, cold TT and heuristics, and 32 MiB Hash unless stated.
- The search sources are unchanged from the 1.0 pilot revision. The intervening
  engine change is the joint-hce-004 HCE parameter update in
  `src/eval/parameters.hpp`, so the pilot positions are inputs for a current-HEAD
  audit rather than an attempt to recreate the old binaries.
- Raw artifacts, commands, inputs, binary hashes, and environment metadata are
  under `tools/measurements/output/search-001-470a3d7/` (gitignored).

The retained OpenBench archives have these properties:

| Test | Release record | Retained archive | SHA-256 |
|---|---:|---:|---|
| 14 | 206 games | 209 games | `be4c2cb1a8d17828d17038552ce7c52683ffb174daddc58043a894b46a0fb569` |
| 15 | 206 games | 84 games | `2ddab28621001a06b85d45a9b5f171ffd3e181aabe79f9ef8dd119dedc30cdff` |
| 18 | 208 games | 209 games | `d3ee62516f7c5a6c3dfe30221ebd0eabcb1182fbd75307726bb5a8dafe6eb2fd` |

All retained games are unique and parse cleanly. The live archive totals do not
match the release record; this is recorded as a provenance limitation rather
than repaired here. Each selected FEN occurs exactly once in its pinned member
and game.

### Suite and protocol

The suite keeps `startpos` and Arasan 20 positions 01, 08, 16, 21, and 30 as
controls. Four primary cases cover abrupt and gradual losses from the
competitive 4ku and Weiss pilots. One Willow case is secondary because that
pilot was more mismatched.

| Case | Source | Historical Latrunculi move and reply |
|---|---|---|
| `pilot14-g171-abrupt` | Test 14, `14.16.1.pgn.bz2`, game 171, Round 87 | `g3g4`, `e7e3` |
| `pilot18-g154-abrupt` | Test 18, `18.21.1.pgn.bz2`, game 154, Round 78 | `f8h8`, `g3g6` |
| `pilot14-g061-gradual` | Test 14, `14.16.1.pgn.bz2`, game 61, Round 29 | `f7f6`, `g3h1` |
| `pilot18-g093-gradual` | Test 18, `18.21.1.pgn.bz2`, game 93, Round 48 | `f7f6`, `e1g1` |
| `pilot15-g078-secondary` | Test 15, `15.20.145.pgn.bz2`, game 78, Round 41 | `g5e5`, `d7h3` |

Every case/repetition ran in a fresh process with one internal repetition. The
`release-dev` matrix used depth 1; 65,536 nodes; 524,288 nodes at 1, 32, and
256 MiB Hash with three repetitions each; 4,194,304 nodes; and five 250 ms
runs. `release-stats` collected the existing search report at 524,288 nodes.
Additional 4,194,304-node statistics were collected when the best move changed
or the score shifted by at least 100 centipawns between the two node budgets.
No alternate-Hash statistics were needed because 32 and 256 MiB produced the
same results.

Fixed-node signatures comprise completed depth, score, actual nodes, best move,
and PV. Time and NPS are excluded. Static scores are normalized to the original
Latrunculi side at the root, after its historical move, and after the opponent
reply.

### Results

All 33 fixed-node triplicate groups were exact. Every fixed-node run stopped at
the requested node count. The `release-dev` and `release-stats` signatures also
matched for all 11 cases at 524,288 nodes and for all six conditional larger
runs.

The 32 and 256 MiB signatures were identical in every case. The 1 MiB stress
condition changed five complete signatures, but only
`pilot14-g171-abrupt` changed its root move. This is collision-stress behavior,
not evidence of a production Hash defect. Within each case, all five fixed-time
runs completed the same depth with the same score, move, and PV. Actual work
ranged from 421,888 to 802,816 nodes in 250.132 to 252.113 ms.

Search scores below are centipawns from Latrunculi's root perspective. Each
search cell is `move / score / completed depth`. Static sequences are
`root -> after Latrunculi move -> after reply`.

| Pilot case | Static sequence | 65,536 nodes | 524,288 nodes | 4,194,304 nodes | Classification |
|---|---:|---|---|---|---|
| `pilot14-g171-abrupt` | +116 -> +47 -> -18 | `f5f1 / +92 / 7` | `f5f1 / +30 / 11` | `f5f4 / -31 / 14` | Not reproduced |
| `pilot18-g154-abrupt` | +31 -> +43 -> -156 | `f8h8 / -9 / 7` | `e7d7 / -52 / 9` | `e7d7 / -169 / 13` | Not reproduced; historical move only at low budget |
| `pilot14-g061-gradual` | +126 -> +81 -> +288 | `f7f5 / +42 / 7` | `g8h7 / +29 / 10` | `g8h7 / +20 / 14` | Not reproduced |
| `pilot18-g093-gradual` | +115 -> +60 -> +17 | `f7f6 / +20 / 8` | `f7f6 / +12 / 12` | `f7f6 / -116 / 16` | Mixed/unresolved |
| `pilot15-g078-secondary` | -48 -> +48 -> +19 | `g5e5 / +78 / 8` | `g5e5 / +17 / 11` | `g3g4 / -122 / 13` | Search-budget-sensitive, secondary only |

The forced continuation after `f8h8` changes the static score from +43 to -156,
and the representative search avoids that move. The other historical opponent
scores are useful selection evidence but are neither a common evaluation scale
nor an objective oracle. In particular, the static direction after the known
reply in the gradual and Willow cases cannot by itself justify an evaluation
task.

The 524,288-node statistics overlap the controls. Percentages reconstructed
from the report's one-decimal per-ply output are approximate; TT values are
node-weighted summaries of the displayed per-ply rates, not probe-weighted
global rates.

| Signal | Six controls | Four primary pilots |
|---|---:|---:|
| Qsearch share | 54.4-74.6% | 68.9-80.0% |
| Deepest reported cumulative EBF | 1.0-1.1 | 1.0-1.2 |
| First-move cutoffs / mean cutoff index | 92.8-96.0% / 1.10-1.16 | 91.6-95.4% / 1.17-1.27 |
| Main TT displayed hit / hit-cut rate | 48.8-72.1% / 28.9-46.7% | 36.1-56.8% / 25.8-35.4% |
| Qsearch TT displayed hit / hit-cut rate | 17.4-39.1% / 89.6-96.0% | 11.8-18.6% / 90.1-97.7% |
| Null-move cutoff rate | 40.2-100.0% | 39.7-56.8% |
| Razor cutoff rate | 84.1-98.9% | 83.2-92.4% |
| LMR re-search rate | 0.12-0.55% | 0.23-0.65% |
| Aspiration re-searches / 1,000 nodes | 0.006-0.032 | 0.000-0.006 |
| PVS re-searches / 1,000 nodes | 0.223-1.053 | 0.235-1.116 |
| Futility skips / 1,000 nodes | 7.8-34.3 | 7.6-15.7 |

The primary cases trend toward more qsearch work and lower displayed TT hit
rates, but the ranges overlap controls and do not identify a shared causal code
path. Cutoff ordering, aspiration, PVS, null move, razoring, futility, and LMR
likewise provide no repeated pilot-only signal. The conditional larger reports
do not change that conclusion.

### Conclusion and limitations

No candidate from SEARCH-001 meets the required gate of a repeatable signal in
two primary cases, or one primary case plus a relevant control, with counters
that isolate the same mechanism. Reported depth alone is not a defect, the
standard-to-roomy Hash comparison is stable, and the fixed-time runs do not
indicate time-management instability. No search follow-up is added to the
roadmap.

This is a small diagnostic suite, not a strength test. The statistics aggregate
all iterative-deepening attempts and aspiration retries. Cutoff order combines
alpha-beta and qsearch nodes, PVS and LMR re-searches overlap, and futility has
no eligibility denominator. Static comparisons include the alternating tempo
term. These limits, plus the engine and score-scale differences in the pilot
PGNs, prevent stronger causal claims without new evidence.

## Continuing search work

This section coordinates incremental search work without reopening or changing
the SEARCH-001 record above. The objective is to improve useful depth and
convergence stability through better selectivity without sacrificing move
quality or correctness. Reported depth alone is not an optimization target.
Search work is not tracked in `docs/roadmap.md`.

### Definitions

- **Reproducibility:** identical fresh-process, one-thread fixed-node runs agree
  exactly in completed depth, score, actual nodes, best move, PV, and applicable
  counters. Timing and NPS are excluded.
- **Convergence stability:** later accepted depths settle rather than repeatedly
  changing root move, centipawn score sign, or PV.
- **Useful depth:** completed depth is credited only while the objective-quality
  requirements hold and the final three accepted depths satisfy the applicable
  stability gate.
- **Selectivity:** fewer nodes reach the same accepted depth or objective
  solution, or greater useful depth is reached at the same node budget.
- **Objective quality:** a source-pinned mate or material criterion verified
  independently of pilot scores. Pilot positions and Arasan best-move labels
  remain contextual evidence, not correctness oracles.

“Solution” below means an objective case satisfying its pinned predicate. For
pilots and Arasan controls, the analogous observation is **reference-move
agreement**, not a solution.

### Operating rules and workspace hygiene

- The immutable comparison anchor is
  `470a3d75c31a13e5f791dc0ee2476c6974be346d` (`470a3d7`). The coordinating-tool
  foundation is `ded3fca89155577061cb234324f0e7e626f94c5e` (`ded3fca`), which
  matches `origin/main` at the start of SA-01 and has no `src/search` or
  `src/eval` differences from the anchor.
- Task states are `active`, `pending`, `done`, `rejected`, and `skipped`, with
  the normal transition `pending -> active -> done|rejected|skipped`. Exactly
  one task is `active` while the workstream is open. An offline-qualified
  experiment awaiting approval remains active and blocks the next task. The
  same rule applies when a diagnostic gate supports a behavior candidate: stop
  for explicit approval before editing behavior. `done` includes a completed
  diagnostic that supports no candidate; `rejected` means a tested behavior
  candidate failed its gate; `skipped` means the task was not run because its
  prerequisite or eligibility gate was absent.
- Preserve `docs/search-audit.md`; preserve `docs/roadmap.md` with SEARCH-001
  absent and no successor; and preserve `tools/measurements/search.cpp`, its
  README, and `tools/measurements/output/search-001-470a3d7/`. The completed
  evidence directory is immutable; do not mix later output into it.
- Preserve `build/release-dev` and `build/release-stats` while work continues.
  At the SA-01 boundary, both reproduce the required 6,068,328-node benchmark.
- The ignored executables under `tools/measurements/output/baselines/` are
  unreferenced, but exact reconstruction was not proved for any of them. Keep
  `latrunculi-checkpoint-a-c8c69a2`, `latrunculi-eval-goal-d5c50e6`,
  `latrunculi-infra003-smoke`, and `latrunculi-pawn-002-before`. Remove no other
  ignored or user-owned artifact.
- Put future evidence in the sibling directory
  `tools/measurements/output/sa-XX-<operational-baseline>/`, with provenance in
  `meta/`, untouched process output in `raw/`, and ignored aggregation material
  in `derived/`. The suffix names the search-behavior baseline (`470a3d7`
  initially); `meta/` also pins the full executable/tool revision (`ded3fca`
  initially), candidate revision or patch, source and binary hashes, compiler
  and build details, commands, and initial status.
- Run one case per fresh process with one search thread, cold TT and heuristics,
  and one internal repetition. Reuse `latrunculi-measure` and the existing
  search instrumentation; extend them only for a named missing measurement or
  denominator.
- Diagnostic code must not change release decisions. A deliberate verifier
  that searches an otherwise omitted line contaminates that run; use only its
  paired verification result and remove it before candidate measurement.
- Change one behavior mechanism at a time. After a rejected experiment or a
  diagnostic with no candidate, retain referenced raw evidence and any
  candidate patch, then remove behavior code, temporary toggles or diagnostics,
  redundant candidate tests, and exact candidate-only build output before the
  next task. Remove task-specific passive counters after disposition unless a
  reviewed boundary explicitly retains them for the next task.
- Before activating the next task, remove task-specific tracked code or stop for
  approval to commit the retained measurement and documentation foundation.
  Do not carry an unpinned mixture of task changes across that boundary.
- Do not change search behavior during diagnostic tasks. Do not commit, push,
  or start OpenBench without explicit approval. An offline-qualified candidate
  remains active and stops for approval to commit, push, and run paired
  OpenBench validation; only an approved retained candidate becomes the next
  operational baseline.

### Measurement and acceptance contract

For a trajectory ending at a case-specific, predeclared depth `D`, run every
depth `1..D` in its own fresh process. The row at depth `d` is cumulative;
iteration nodes are `nodes[d] - nodes[d-1]`. A baseline and candidate use the
same `D` for each case. If a candidate completes an extra fixed-node depth,
extend both fixed-depth ladders before crediting it.

First solution depth is the first depth satisfying an objective predicate.
Stable solution depth is the first satisfying depth whose entire suffix through
`D` also satisfies it. An objective case must satisfy its expected result at
each of the final three depths; its scores and PVs need not be identical.

For each case, useful depth is the greatest measured `d >= 3` for which depths
`d-2..d` have one root move, one score class (negative, zero, or positive
centipawns, or the same mate direction), and a nonzero PV LCP across both
adjacent transitions. An objective case must also satisfy its predicate at all
three depths, and no case receives useful-depth credit if the candidate fails a
suite-wide objective-quality gate.

“Late” covers depths `D-2`, `D-1`, and `D`. Record adjacent centipawn score
jumps, root changes, score-sign flips, and PV longest-common-prefix (LCP) in
plies and as `LCP / min(PV lengths)`. The denominator is therefore the
shorter of the two PVs. Exclude mate-score transitions from centipawn jump and
sign-flip statistics and report them separately. Collapse consecutive equal
root moves before counting A-B-A oscillations. For the full trajectory,
run-length encode root moves at depths `1..D` and count every consecutive
`A, B, A` triple where `A != B`. Late A-B-A is one exactly when
`root[D-2] == root[D] != root[D-1]`, otherwise zero; the common stability gate
uses the late count. Use nearest-rank p90.

Unless a task states otherwise, the SA-03 through SA-05 passive diagnostic
matrix is `release-stats`, 524,288 nodes, 32 MiB Hash, one thread, one case per
fresh process, and one internal and external repetition over the original
eleven cases plus the three objective guards. “Complete suite” means those
fourteen cases. Pair each stats row with the same `release-dev` command and
require its non-timing search signature to match. Counters aggregate all
iterative-deepening attempts and aspiration retries. Eligibility gates use the
original eleven cases; suite-wide safety and veto checks use all fourteen. Do
not pool counts across external repetitions, reruns, or debug collections.

Every behavior candidate must pass all of these gates:

1. Pass focused tests, the complete `release-dev` and `release-stats` CTest
   suites, and ASan/UBSan.
2. For the original eleven cases and the three objective guards at 524,288
   nodes and 32 MiB Hash, agree exactly across three fresh-process, one-thread
   repetitions under the reproducibility definition. A run terminating before
   the node cap uses its repeatable actual node count.
3. Lose no objective solution, reach no objective stable solution later, find
   no worse mate, and increase no objective case's nodes to stable solution by
   more than 5%. A worse mate means a missed or incorrect mate result or a
   longer mate distance.
4. Across the complete comparison suite, do not increase aggregate late root
   changes, sign flips, or A-B-A counts; add no full-trajectory or late A-B-A
   to an objective case; do not increase late score-jump p90; do not reduce
   median PV survival; and do not increase zero-prefix transitions.
5. Pass one selectivity test:
   - At each case's common predeclared comparison depth `D`, the geometric mean
     over the per-case candidate/baseline cumulative-node ratios is at most
     0.99, with at least two independent cases at or below 0.98; or
   - useful fixed-node depth improves by at least one ply in two independent
     cases while that geometric-mean node ratio is at most 1.00.
6. Rerun the original SEARCH-001 matrix's eleven cases against its preserved
   raw baseline, including dev/stats non-timing signature agreement, and rerun
   the SA-02 trajectories and objective guards. Compare normalized semantic
   fields and ignore only the `search_measurement_v2`/`v3` format-label change.
7. Before the first behavior edit, reproduce the anchor's 6,068,328-node
   benchmark. Run a candidate benchmark twice, require repeatability, and
   record its new fingerprint rather than requiring the anchor value.

A candidate that passes every offline gate remains active and stops for review
and explicit approval of paired OpenBench validation.

### Task queue

| Task | State | Scope |
|---|---|---|
| `SA-01` | `done` | Consolidate the foundation and workspace hygiene. |
| `SA-02` | `done` | Establish objective quality and convergence baselines. |
| `SA-03` | `rejected` | Audit capture ordering and run at most one gated experiment. |
| `SA-04` | `done` | Audit qsearch selectivity and run at most one gated experiment. |
| `SA-05` | `done` | Audit history-aware LMR and run at most one gated experiment. |
| `SA-06` | `active` | Rebaseline and choose the next mechanism from evidence. |

#### SA-01 — Foundation and hygiene

Preserve the audit above as immutable evidence, establish this coordination
contract, and make no search-behavior change. At the clean pre-edit boundary,
HEAD and `origin/main` were `ded3fca`, its search and evaluation sources matched
`470a3d7`, both retained builds reproduced 6,068,328 benchmark nodes, and the
protected SEARCH-001 tree contained 693 regular files. No stale executable met
the deletion gate, so none was removed.

- Hypothesis: a pinned contract and clean boundary make later experiments
  comparable and individually reversible.
- Baseline: `470a3d7` evidence anchor and `ded3fca` coordinating-tool foundation.
- Candidate: this one-file documentation change only.
- Measurements: status and source equality, protected-artifact inventory,
  stale-binary provenance, and both retained benchmark fingerprints.
- Result: the protected scope is intact; no cleanup candidate is both proved
  unreferenced and exactly reconstructible.
- Artifact path: `tools/measurements/output/search-001-470a3d7/` remains the
  immutable evidence reference; SA-01 creates no new raw measurement directory.
- Disposition: `done`; the one-file documentation boundary was reviewed and
  explicitly approved, and SA-02 is active but not yet executed.

SA-01 closes with only `docs/search-audit.md` modified. No SA-02 source change,
artifact directory, or measurement belongs to this commit.

#### SA-02 — Objective quality and convergence baseline

Add three cases to the existing selector without changing its other options or
TSV columns, and label the workload `search_measurement_v3`. Do not add `--fen`
or a progress tracer.

| Case | Objective predicate | Pinned source blob |
|---|---|---|
| `objective-mate-1` | any legal mate in one; expected `h8h1` | `tests/search/root_search.test.cpp` at `8cb9b0ca17b3181eed32faa754d1da05ed67df03` |
| `objective-mate-2` | legal mate in two by depth 3; root move unrestricted | `tests/uci/engine_search.test.cpp` at `313c13ad42dd7f3f2d8b7183c84c831651e63a38` |
| `objective-rook-capture` | maximal immediate material gain; expected `d1e2` capturing the loose rook | `tests/search/quiescence.test.cpp` at `84716f75b2d5ea49d87f15e1972b4829aa616d6d` |

Before measurement, verify the predicates independently of engine search:
enumerate every legal root to prove `h8h1` is the unique mate in one (otherwise
accept any verified mate in one), exhaustively prove a legal mate within three
plies for `objective-mate-2`, and enumerate immediate material outcomes to
prove `d1e2` is the unique maximum (otherwise accept any verified equal-or-better
root outcome). Pin the verifier command or source and its output with the SA-02
evidence.

Pin the Arasan EPD blob
`93cbe97d9ee40790eafc984e59cbce3c02a5d7ea` and record contextual labels
`01:g2g4`, `08:e5e6`, `16:d2e2`, `21:d5f6`, and `30:e5h8`.

All SA-02 fixed-depth, fixed-node, and objective collection uses `release-dev`,
32 MiB Hash, one thread, one case per fresh process, and one internal and
external repetition; each trajectory row and 33,554,432-node extension is
therefore collected once, while the clock profiles retain their stated five
external repetitions. Run the 33,554,432-node extension once for
`pilot14-g171-abrupt`, `pilot18-g093-gradual`, and `arasan20-21`. Reconstruct
fresh-process depths through each original case's 4,194,304-node completed
depth, except those three cases, which continue through their 33,554,432-node
completed depth. Run each
objective case through depth 8. Predeclare case predicates, horizons, and the
quality, stability, and selectivity summaries before collection, and preserve
all output under `tools/measurements/output/sa-02-470a3d7/`.

The immutable SEARCH-001 `meta/verify-cli.sh` assumes eleven
`search_measurement_v2` rows. Copy and adapt its validation logic under the
SA-02 artifact tree; do not edit the baseline verifier.

Reproduce the preliminary observation that 250 ms searches spent 5.2–43.2% of
their nodes, median 28.7%, after the last completed iteration. Across the
original eleven cases, run five fresh repetitions of each clock profile:

| Clock control | Expected allocation |
|---|---:|
| `wtime=btime=3000`, `movestogo=30` | 50 ms |
| `wtime=btime=10000`, `winc=binc=50`, `movestogo=40` | 250 ms |
| `wtime=btime=30000`, `movestogo=30` | 950 ms |

Pair every timed result with the fixed-depth cumulative nodes at its last
completed depth `d` and calculate
`(timed_nodes - fixed_depth_nodes[d]) / timed_nodes`; reject a pairing if the
difference is negative or the accepted depth-`d` signature disagrees. Explicit
`movetime` remains a hard-duration request. Clock management becomes eligible
for SA-06 only if at least two profiles have both a suite median of at least
20% and at least six cases individually at or above 20%.

If a clock run completes beyond its case's current trajectory horizon, extend
that case's fresh-process fixed-depth trajectory through the observed depth
before pairing it; never extrapolate cumulative nodes.

Collect these clock profiles through raw UCI rather than expanding the
`latrunculi-measure search` schema. An ignored driver under `derived/` starts a
fresh `release-dev` process per case and repetition, sets Threads to 1 and Hash
to 32 MiB, sends `ucinewgame`, `isready`, the pinned position, and the exact
`go` clock fields, waits for `bestmove`, and only then sends `quit`. Preserve
each stdout and stderr transcript and derive the completed-depth and node fields
from the final accepted `info depth` line immediately preceding `bestmove`.

##### SA-02 experiment record

- Hypothesis: objective guards and fresh-depth trajectories can make later
  selectivity experiments falsifiable, while longer node budgets and real UCI
  clocks can distinguish late convergence from unfinished-iteration work.
- Baseline: search and evaluation at `470a3d7`, coordinated from `f1aed84` with
  the `ded3fca` measurement foundation. The preserved SEARCH-001 4,194,304-node
  rows are the semantic cross-check.
- Candidate: no search candidate. The measurement-only change adds the three
  objective cases and changes the workload label to `search_measurement_v3`;
  options and TSV columns are unchanged.
- Measurements: an external exhaustive predicate check, three 33,554,432-node
  extensions, 201 fresh-process depth rows, 55 preserved explicit-movetime
  pairings, 165 fresh raw-UCI clock pairings, and the adapted CLI/build/test
  checks.
- Result: all objective final windows pass; every fixed-limit and timed
  accepted signature cross-check passes; the convergence baseline retains four
  cases below their horizon in useful depth; and all three game-clock profiles
  meet the predeclared SA-06 eligibility threshold. No search mechanism is
  changed or causally identified.
- Artifact path: `tools/measurements/output/sa-02-470a3d7/` (gitignored), with
  frozen declarations and provenance in `meta/`, untouched invocation output in
  `raw/`, and reproducible aggregation in `derived/`.
- Disposition: `done`; diagnostic collection and review are complete, and the
  retained v3 measurement and documentation foundation was explicitly approved
  as the workstream boundary. SA-03 is active but has not been executed.

The external verifier used Stockfish only for legal-move enumeration and check
status through `d` and `go perft 1`, not for alpha-beta search. It made 1,338
queries and reproduced byte-identical output. The verified mate-in-two roots
are both accepted rather than inventing a single expected move.

| Objective case | Independent predicate | First / stable depth | Nodes to first / stable | Depth-8 result | Useful depth |
|---|---|---:|---:|---|---:|
| `objective-mate-1` | Unique `h8h1#` | 1 / 1 | 46 / 46 | `h8h1 / +M1` | 8 |
| `objective-mate-2` | `d3c2` or `d3c3` forces mate in 3 plies | 3 / 3 | 767 / 767 | `d3c3 / +M2` | 8 |
| `objective-rook-capture` | Unique maximum `d1e2`, winning a rook | 1 / 1 | 29 / 29 | `d1e2 / +1915` | 8 |

All eleven immutable v2 4,194,304-node accepted signatures match the v3
fresh-depth row at the reported completed depth. Each 33,554,432-node accepted
signature likewise matches its independently run final depth. SEARCH-001's
fixed-node triplicates remain the reproducibility evidence; SA-02 intentionally
collects each new fixed-work cell once.

`D / U` below is the case-specific trajectory horizon and resulting useful
depth. Nodes are cumulative at `D` followed by the nodes in iteration `D`.
`R / S / A` counts late root changes, centipawn sign flips, and A-B-A
oscillations.

| Case | D / U | Nodes at D / iteration D | Final move / score | Late R / S / A | Late PV survival |
|---|---:|---:|---|---:|---:|
| `startpos` | 14 / 11 | 3,367,732 / 1,193,115 | `e2e4 / +74` | 1 / 0 / 0 | 15.4% |
| `arasan20-01` | 15 / 15 | 2,946,219 / 886,943 | `e4a8 / -79` | 0 / 0 / 0 | 42.3% |
| `arasan20-08` | 14 / 14 | 3,236,811 / 1,660,343 | `h6h5 / -387` | 0 / 0 / 0 | 34.9% |
| `arasan20-16` | 16 / 14 | 3,698,850 / 1,524,475 | `f1e1 / +168` | 2 / 0 / 1 | 0.0% |
| `arasan20-21` | 21 / 21 | 30,607,242 / 14,912,303 | `d5c3 / +234` | 0 / 0 / 0 | 38.8% |
| `arasan20-30` | 19 / 19 | 3,395,887 / 761,379 | `g5e6 / +694` | 0 / 0 / 0 | 60.6% |
| `pilot14-g171-abrupt` | 18 / 17 | 33,287,910 / 23,569,099 | `h4h5 / -74` | 1 / 0 / 0 | 34.4% |
| `pilot18-g154-abrupt` | 13 / 13 | 2,955,468 / 543,734 | `e7d7 / -169` | 0 / 0 / 0 | 83.3% |
| `pilot14-g061-gradual` | 14 / 14 | 2,985,482 / 1,310,751 | `g8h7 / +20` | 0 / 0 / 0 | 55.8% |
| `pilot18-g093-gradual` | 20 / 20 | 21,863,741 / 8,818,596 | `f7f6 / -159` | 0 / 0 / 0 | 84.2% |
| `pilot15-g078-secondary` | 13 / 11 | 3,397,528 / 597,231 | `g3g4 / -122` | 1 / 1 / 0 | 7.7% |

The four `U < D` entries identify the deepest earlier stable three-depth
window; their final three depths do not satisfy the stability gate.

Across all fourteen cases, the late baseline is five root changes, one sign
flip, one A-B-A, a 46-centipawn nearest-rank p90 jump, 61.5% median PV survival,
and five zero-prefix transitions. The A-B-A is the transient
`f1e1 -> d2e2 -> f1e1` at Arasan 16. Zero-prefix and root-change counts are not
independent here because every validated PV begins with its reported root move.
The three objective cases have no late root change, sign flip, A-B-A, or
zero-prefix transition.

The longer fixed-node runs add four completed plies apiece. Only the abrupt
pilot changes root move at the endpoint; it therefore remains a useful
convergence stress case, not a correctness oracle.

| Case | 4,194,304 nodes | 33,554,432 nodes | PV LCP | Unfinished share at 33M |
|---|---|---|---:|---:|
| `pilot14-g171-abrupt` | `d14 f5f4 / -31` | `d18 h4h5 / -74` | 0 / 14 | 0.8% |
| `pilot18-g093-gradual` | `d16 f7f6 / -116` | `d20 f7f6 / -159` | 13 / 16 | 34.8% |
| `arasan20-21` | `d17 d5c3 / +266` | `d21 d5c3 / +234` | 5 / 17 | 8.8% |

The contextual Arasan labels are never stable through a horizon. Only Arasan
16 at depth 15 and Arasan 30 at depths 2 and 7 match their labels; the other
three controls never match. These observations carry no correctness weight.

All 220 timed rows pair exactly with their fresh-depth score, move, and PV, and
none required a horizon extension. Fractions below summarize the median of five
runs per case; the range is across those eleven case medians. This reproduces
the preliminary explicit-movetime observation exactly on that basis. The wider
all-run range for explicit movetime is 3.7-45.4% and remains in the derived
table. In 67 of the 165 game-clock transcripts, an interim `info` line printed
a greater depth than the final retained result; the analysis uses only the
final accepted line immediately before `bestmove`.

| Control | Observed duration | Case-median unfinished range | Suite median | Cases >= 20% | SA-06 clock gate |
|---|---:|---:|---:|---:|---|
| Explicit `movetime 250` | 250.1-252.1 ms | 5.2-43.2% | 28.7% | 7 | Not applicable; hard request |
| 50 ms game clock | 50-52 ms | 0.5-90.6% | 24.5% | 6 | Pass |
| 250 ms game clock | 250-252 ms | 3.7-42.3% | 24.2% | 6 | Pass |
| 950 ms game clock | 950-953 ms | 0.5-79.7% | 25.7% | 7 | Pass |

The three passing profiles make clock management eligible for evidence review
in SA-06. They show repeated work after the last completed iteration, but do not
show that stopping earlier would improve strength or justify changing soft or
hard limits. The suite remains deliberately small, single-threaded, and tied to
one machine; objective guards cover only two mates and one immediate material
win. Those limits, and the contextual status of pilots and Arasan labels,
remain part of every later candidate decision.

#### SA-03 — Capture-ordering audit and one possible experiment

CaptureHistory exists as tested scaffolding but has no production consumer.
First extend stats with global counters keyed by search context, inferred
stage, move kind, noisy ordinal bucket, and outcome. Classify outcomes as
fail-low, alpha raise, or beta cutoff relative to the pre-move alpha. Search
context is main or qsearch, with PV or NonPV as a separate dimension. Infer the
relevant stage stats-only as TT, evasion, promotion, exact-SEE-good, or
exact-SEE-bad, and account for negative-SEE qsearch TT captures separately. Do
not add Picker state or release-build SEE work.

A noisy ordinal is the one-based count of searched legal moves in the same
`(context, node type, inferred stage, move kind)` at the current node and resets
for every node and stage. TT moves, evasions, promotions, en passant, illegal
moves, and moves in the other exact-SEE band do not consume an ordinary
capture's ordinal. Use buckets `{1, 2, 3-4, 5-8, 9+}` and report cells
separately; a per-case eligibility rate may sum their successes and attempts,
but ordinals remain stage-local. The late-success rate is ordinal-2-or-later
alpha raises and cutoffs divided by all searched ordinal-2-or-later attempts in
that population.

The eligibility gate requires two distinct original cases each to show at least
32 ordinal-2-or-later ordinary-capture successes and a late-success rate of at
least 2%. A success is an alpha raise or beta cutoff. If the gate passes, keep
SA-03 `active`, record the finding, and stop for explicit approval before a
behavior edit. Only then may one candidate add CaptureHistory ownership and
lifecycle, reward capture cutoffs, and penalize earlier failed captures. Use it
only to order ordinary captures within the existing SEE-good and SEE-bad score
bands; leave the promotion band untouched, and never reclassify or prune a
move. Before behavior code, the experiment record must pin ownership and
lifecycle, main/qsearch consumer and update populations, reward and malus
scales, aging and clearing, and score scaling. Determine promotion and exact-SEE
stage membership before applying history: stage identity and inter-stage
precedence are primary, and history is only a secondary within-stage key or a
bounded offset proved unable to cross a boundary. Focused tests at minimum and
maximum history must prove no move crosses the promotion, SEE-good, or SEE-bad
bands; adding the raw history value without that proof is forbidden. Alongside
the buckets, retain the exact ordinal sum for successful captures. “Improved
successful-capture ordinal” means a strictly lower per-case mean at the same
fixed-node budget; require it in at least two cases in addition to the common
gates. If the opportunity gate fails, finish SA-03 `done` with no candidate. If
a tested candidate fails, remove it and mark SA-03 `rejected`.

SA-03 passive result at the `6507724` coordinating revision:

| Case | Late attempts | Successes | Rate | Mean success ordinal |
|---|---:|---:|---:|---:|
| `startpos` | 19,697 | 1,649 | 8.37% | 2.089 |
| `arasan20-01` | 58,116 | 2,369 | 4.08% | 2.168 |
| `arasan20-08` | 96,422 | 5,733 | 5.95% | 2.263 |
| `arasan20-16` | 5,301 | 396 | 7.47% | 2.131 |
| `arasan20-21` | 6,239 | 673 | 10.79% | 2.027 |
| `arasan20-30` | 22,755 | 1,610 | 7.08% | 2.086 |
| `pilot14-g171-abrupt` | 33,731 | 1,957 | 5.80% | 2.115 |
| `pilot18-g154-abrupt` | 85,857 | 5,006 | 5.83% | 2.234 |
| `pilot14-g061-gradual` | 70,481 | 1,993 | 2.83% | 2.193 |
| `pilot18-g093-gradual` | 49,572 | 5,323 | 10.74% | 2.087 |
| `pilot15-g078-secondary` | 56,427 | 2,665 | 4.72% | 2.195 |
| **Original-suite aggregate** | **504,598** | **29,374** | **5.82%** | **2.171** |

All eleven original cases pass both opportunity thresholds, rather than only
the required two. Of the aggregate successes, 58 are alpha raises and 29,316
are beta cutoffs. Exact-SEE-good captures account for 28,888 successes from
401,467 late attempts (7.20%); exact-SEE-bad captures account for 486 from
103,131 (0.47%). This demonstrates repeated within-stage ordering opportunity,
not that CaptureHistory will necessarily exploit it or improve strength.

The paired 14-case matrix preserved exact non-timing dev/stats signatures and
all three objective predicates. For the eleven SEARCH-001 cases, the current
v3 signatures match the immutable v2 rows and the pre-existing diagnostic text
is byte-identical; 297 negative-SEE qsearch TT attempts were separately
accounted. Focused tests, both complete release CTest suites, and direct
ASan/UBSan unit and randomized-stress runs passed. LeakSanitizer itself is not
usable under the ptrace execution environment, so the direct sanitizer runs
disabled leak detection. The pre-behavior release-dev benchmark remains
6,068,328 nodes.

- Hypothesis: searched late ordinary captures succeed often enough across
  independent cases to justify one bounded within-stage ordering experiment.
- Baseline: immutable search behavior `470a3d7`, with coordinating and
  measurement revision `6507724`.
- Candidate: none yet; this pass adds stats-only diagnostics and does not use
  CaptureHistory or alter release search decisions.
- Measurements: one paired fresh-process dev/stats run for each of fourteen
  cases at 524,288 nodes, 32 MiB Hash, one thread, plus the frozen opportunity,
  signature, legacy-counter, objective, test, sanitizer, and benchmark checks.
- Result: all eleven original cases pass the predeclared opportunity gate; the
  passive diagnostic is behavior-neutral on every compared signature.
- Artifact path: `tools/measurements/output/sa-03-470a3d7/`.
- Disposition at the passive boundary: `active`; retain the passive diagnostic
  and stop for explicit approval before pinning or editing the single permitted
  CaptureHistory behavior candidate. SA-04 was pending.

##### SA-03 CaptureHistory candidate

The approved candidate was pinned before behavior code at passive boundary
`b415f5a`. Each worker's `ordering::State` owns and ages one CaptureHistory and
clears it with the worker's other search heuristics. Main search and qsearch
consume `history / 2` only for non-TT ordinary captures at nodes that are not
in check, after exact SEE has selected the existing good or bad capture band.
Promotions, en passant, TT priority, and evasions remain unaffected. Only main
alpha-beta updates history: reward an eligible capture cutoff with the existing
depth-squared bounded-gravity update and penalize, at half that update, up to
32 earlier eligible true fail-lows. A true fail-low is
`value <= alpha_before_move`; an alpha raise is not penalized. Qsearch is a
read-only consumer. The classification uses the move's final returned result
after any existing LMR re-search; independently verifying reduced fail-lows is
reserved for SA-05.

The offset is bounded to `[-512, 512]`. Thus a bad capture remains below the
`2^22` good-capture boundary, while the least-valued good capture remains at
least `2^22 + 7 * 100 - 512 = 2^22 + 188`. The promotion score remains `2^28`,
far above any legal ordinary-capture payload plus the offset. Focused tests
exercised both saturated signs at these boundaries.

In addition to the common gates, alternate five baseline/candidate
fresh-process fixed-depth pairs for `startpos` at depth 14, `arasan20-21` at
depth 21, and `pilot14-g171-abrupt` at depth 18. The geometric mean of their
per-case median candidate/baseline NPS ratios must be at least 0.98. If the
candidate qualifies through fixed-depth node reduction, its corresponding
geometric-mean wall-time ratio must also be below 1.00. This is a local
throughput guard, not playing-strength evidence.

The candidate's successful-capture ordinal gate pools every alpha raise and
beta cutoff for ordinary captures in exact-SEE-good and exact-SEE-bad cells,
across both search contexts, both node types, and all ordinal buckets including
ordinal 1. Compare exact ordinal sums and success counts by cross
multiplication. The late-only means above remain the passive opportunity
measurement and are not an alternate candidate-retention metric.

- Hypothesis: bounded capture-history feedback improves successful-capture
  ordinal and reduces work to stable depth without weakening convergence or
  objective solutions.
- Baseline: immutable search anchor `470a3d7`; passive diagnostic and
  operational boundary `b415f5a`.
- Candidate: one per-worker CaptureHistory, read in main/qsearch within the
  existing ordinary-capture SEE bands and trained only by main-search capture
  cutoffs and preceding true fail-lows, with the fixed scales above.
- Measurements: staged focused tests, paired 524,288-node diagnostic screen,
  terminal-depth selectivity screen, and throughput check. Survivor-only full
  trajectories, three-repeat fixed-node matrix, complete release and sanitizer
  suites, and SEARCH-001 rerun were not run after the throughput rejection.
- Result: focused release tests passed. The paired screen preserved all three
  objective guards and exact dev/stats signatures; successful-capture ordinal
  improved in four original cases. Fixed-depth route 1 also passed, with a
  0.9716 geometric-mean candidate/baseline node ratio and seven cases at or
  below 0.98. Results were heterogeneous: start position used 1.3973 times the
  nodes at depth 14 and `pilot14-g061-gradual` used 1.7593 times the nodes at
  depth 14. The NPS guard passed at 0.9916, but the required wall-time guard
  failed at 1.0646; start position had a 1.4129 median time ratio. The
  deterministic candidate benchmark was repeatable at 7,782,835 nodes. That
  benchmark check ran after the paired screen rather than in its predeclared
  order, with no intervening candidate or binary change. A premature CTest
  invocation of the complete suite is retained as a non-gating preflight
  failure: the candidate-only stress target had not yet been built, and the
  suite was not rerun after the later rejection.
- Artifact path:
  `tools/measurements/output/sa-03-capture-history-b415f5a/`.
- Disposition: `rejected`; the mixed node reductions did not translate into
  the required end-to-end time improvement. The frozen candidate patch and
  evidence remain, while behavior code, candidate-only tests, and candidate
  build output were removed before activating SA-04.

#### SA-04 — Qsearch shadow audit and one possible pruning experiment

Qsearch already excludes ordinary exact-SEE-negative captures; this task does
not propose adding that existing rule. At qsearch nodes that are not in check,
add stats-only shadow data for searched legal ordinary captures with exact SEE
at least zero. Capture the node's original `stand_pat`, beta, and, for each
move, `alpha_before_move` after stand pat and any earlier searched moves; use
`eval::piece(captured_piece_type).mg` as `captured_value`. At margins 200, 300,
and 400 centipawns, record total eligible moves, hypothetical skips satisfying
`stand_pat + captured_value + margin <= alpha_before_move`, and outcomes within
that skipped population. Exempt TT moves, promotions, en passant, moves that
give check, and mate windows.

Classify the searched result mutually exclusively as fail-low when
`value <= alpha_before_move`, alpha raise when
`alpha_before_move < value < beta`, or cutoff when `value >= beta`. Record PV
entry independently when the current qsearch calls `pv->update` with that move;
it may coincide with a raise or cutoff. Separate PV observations from the
candidate-eligible NonPV population; the first behavior trial exempts every PV
node.

Choose the largest predeclared margin that produces at least 100 hypothetical
skips in the NonPV eligibility population in each of two distinct original
cases, with zero hypothetically skipped alpha raises or cutoffs there across the
complete suite. The separately reported PV shadow population is a conservative
veto: any would-be skipped PV entry also disqualifies the margin even though PV
nodes remain exempt from behavior. If no margin qualifies, finish SA-04 `done`
without behavior code. If one qualifies, keep SA-04 `active`, record the
finding, and stop for explicit approval before behavior code. Only then test
exactly one NonPV rule: prune when
`stand_pat + captured_value + selected_margin <= alpha_before_move`. Retain it
only under the common objective-quality, stability, and selectivity gates;
mark a failing tested candidate `rejected` and remove it.

SA-04 observed the complete fourteen-case matrix at the `1a9e026` coordinating
boundary. All eleven original cases exceeded the opportunity threshold at
every margin, but every margin failed the suite-wide safety veto:

| Margin | Original cases with >=100 skips | Original NonPV skips | Complete NonPV fail-lows | Complete NonPV cutoffs | Complete PV entries |
|---:|---:|---:|---:|---:|---:|
| 200 | 11 | 643,965 | 640,940 | 3,200 | 9 |
| 300 | 11 | 581,471 | 580,869 | 732 | 1 |
| 400 | 11 | 524,006 | 523,906 | 206 | 0 |

There were no NonPV alpha raises. That is not evidence of safety: these
NonPV qsearch calls predominantly use a null window, so a move that improves
the bound appears as a beta cutoff. Even the most conservative predeclared
margin would therefore have discarded 206 moves that actually cut off. The
PV veto clears only at 400 centipawns, while the NonPV cutoff veto does not
clear at any margin.

All fourteen current dev/stats signatures match each other and the sealed
SA-03 rows, the retained diagnostic prefix is byte-identical, and all three
objective guards pass. Focused tests, both complete release CTest suites, and
the full ASan/UBSan suite pass. The preset sanitizer invocation is retained as
an infrastructure-only failure because it forces LeakSanitizer on under the
ptrace environment; the equivalent test tree passes with leak detection
disabled and both sanitizers set to halt on errors. Both builds reproduce the
6,068,328-node benchmark.

- Hypothesis: a conservative material-margin rule can omit a repeated class of
  qsearch captures that never improve the bound or enter the PV.
- Baseline: immutable search behavior `470a3d7`; coordinating boundary
  `1a9e026` with the retained SA-03 diagnostics.
- Candidate: none. A stats-only shadow measured 200, 300, and 400 centipawn
  margins without changing search decisions.
- Measurements: one paired fresh-process dev/stats run for each of fourteen
  cases at 524,288 nodes, 32 MiB Hash, and one thread, plus sealed-signature,
  diagnostic-prefix, objective, test, sanitizer, and benchmark checks.
- Result: opportunity is broad, but all three margins hypothetically discard
  real NonPV beta cutoffs; no margin passes the predeclared safety gate.
- Artifact path: `tools/measurements/output/sa-04-470a3d7/`.
- Disposition: `done`; no behavior candidate is permitted. The task-specific
  shadow counters and focused tests are removed before SA-05 collection; the
  referenced raw evidence remains.

#### SA-05 — History-aware LMR audit and one possible experiment

The current LMR formula considers depth, move count, node type, check state,
promotion, quiet status, and killer status, but not history.
Split LMR attempts by node type, move class, depth, reduction bucket
`{1, 2, 3+}`, and the combined quiet-ordering score returned by
`State::quiet_score()`. Quiet and continuation history each saturate at 1024,
so the reachable combined range is `[-2048, 2048]`; use buckets
`{<=-1024, -1023..-1, 0, 1..1023, >=1024}`. For each attempt, capture
`alpha_before_move`, `beta`, and the quiet score before `board.make()`. Classify
the reduced result as fail-low (`value <= alpha_before_move`) or alpha raise.
For the existing full-depth re-search after a reduced alpha raise, classify the
final result as refuted (`value <= alpha_before_move`), confirmed alpha raise
(`alpha_before_move < value < beta`), or cutoff (`value >= beta`). Unresearched
high-history reduced fail-lows form the verifier population.

Run a temporary stats-only verifier only if passive counters show at least 64
high-history reduced fail-lows in each of two distinct original cases. The
passive run selects every 64th deterministic occurrence, capped at 32 per case.
Replay each selected occurrence in its own fresh process with a fixed-depth
horizon of `min(completed_depth + 1, max_depth)` and no node or time cap that
can interrupt the pair. Follow the baseline path without earlier verification,
activate exactly one verifier at the selected occurrence, suppress verifier
recursion and subtree sampling, and search the same move at unreduced depth
with the original null window and captured `alpha_before_move`. A result at or
below that alpha confirms the fail-low; a result above it is a false reduced
fail-low. The null-window verifier does not subdivide false fail-lows into alpha
raises and beta cutoffs. Treat the replay as contaminated, retain only its
paired outcome, and discard the remainder of its counters. Remove the verifier
before candidate measurement.

Permit a candidate only if false reduced fail-lows occur in at least two
distinct original cases. If that gate passes, keep SA-05 `active`, record the
finding, and stop for explicit approval before behavior code. The sole
permitted candidate shape is to reduce strong quiet moves with combined
history at least 1024 by one ply less, floored at zero; never increase
reductions elsewhere. If the evidence gate fails, finish SA-05 `done` with no
candidate. Apply the common gates to a tested candidate; if it fails, remove it
completely and mark SA-05 `rejected`.

##### Passive audit boundary — 2026-09-07

The behavior-neutral 524,288-node matrix found repeated high-history reduced
fail-lows in ten of the eleven original cases. The counts below are the
completed quiet, `history >= 1024` reduced fail-lows; verifier selections use
every 64th occurrence and are capped at 32 per case.

| Case | Fail-lows | Selected occurrences | Passive gate |
|---|---:|---:|---|
| `startpos` | 1,423 | 22 | pass |
| `arasan20-01` | 713 | 11 | pass |
| `arasan20-08` | 26 | 0 | fail |
| `arasan20-16` | 187 | 2 | pass |
| `arasan20-21` | 3,256 | 32 | pass |
| `arasan20-30` | 501 | 7 | pass |
| `pilot14-g171-abrupt` | 562 | 8 | pass |
| `pilot18-g154-abrupt` | 212 | 3 | pass |
| `pilot14-g061-gradual` | 610 | 9 | pass |
| `pilot18-g093-gradual` | 512 | 8 | pass |
| `pilot15-g078-secondary` | 380 | 5 | pass |

The two mate guards produced no high-history fail-lows and the material guard
produced two; objective cases do not satisfy the two-case gate. All fourteen
dev/stats signatures match each other and the sealed SA-04 signatures, the
legacy stats prefix is byte-identical to SA-03, and all objective guards pass.
The matrix recorded 12 interrupted reduced searches and two interrupted
re-searches in their explicit interruption buckets. Focused and complete
release tests, ASan/UBSan, and both 6,068,328-node benchmark anchors pass.

- Hypothesis: strong quiet moves may be reduced often enough for history-aware
  LMR protection to matter.
- Baseline: immutable search behavior `470a3d7`; coordinating boundary
  `03fee82` with retained SA-03 diagnostics.
- Candidate: none at this passive boundary; only stats and focused tests are
  added.
- Measurements: one paired fresh-process dev/stats run for each of fourteen
  cases at 524,288 nodes, 32 MiB Hash, and one thread.
- Result: the repeated-opportunity gate passes in ten original cases; this
  permits the temporary verifier but is not evidence that any reduction is
  incorrect or that the candidate will improve search.
- Artifact path: `tools/measurements/output/sa-05-470a3d7/`.
- Disposition: `active`; seal this passive boundary, then verify the selected
  fail-lows before considering behavior code.

##### Temporary verifier result — 2026-09-07

The verifier ran from passive boundary `25bb133` using temporary, stats-only
commit `7aa0f46`. Each of the 107 frozen occurrences was replayed in a fresh
process at its case's fixed-depth horizon, which ranged from depth 10 through
16. Every active sample prefix matched its control exactly, every target was
reached once, and every unreduced null-window replay remained at or below the
captured alpha:

| Eligible original cases | Selected occurrences | Confirmed fail-lows | False fail-lows | Cases with a false fail-low |
|---:|---:|---:|---:|---:|
| 10 | 107 | 107 | 0 | 0 |

The collection also preserved 20 dev/stats controls and eight invalid-interface
probes. All ten control signature pairs match. A separate 14-process
verification boundary passed the focused and complete dev/stats suites, the
ptrace-safe ASan/UBSan suite, both 6,068,328-node benchmarks, and clean-diff
checks. The sealed artifact contains 540 raw matrix files and 56 raw
verification files; its manifests validate the raw and derived partitions,
frozen harness, verifier patch, and all six earlier protected evidence trees.

- Hypothesis: some high-history quiet moves classified as reduced fail-lows
  would exceed the original alpha if searched at unreduced depth.
- Baseline: immutable search behavior `470a3d7`; retained passive boundary
  `25bb133`.
- Candidate: no behavior candidate. Temporary commit `7aa0f46` adds only the
  isolated, deliberately contaminating verifier and is not part of the
  operational branch.
- Measurements: 107 paired reduced/unreduced searches across ten original
  cases, plus 20 controls, eight interface rejections, and 14 preserved
  verification invocations.
- Result: all 107 reduced fail-lows were independently confirmed; no original
  case produced a false fail-low.
- Artifact path:
  `tools/measurements/output/sa-05-verifier-25bb133/`.
- Disposition: `done`; the required two-case signal is absent, so the sole
  history-aware LMR behavior candidate is not permitted. The sealed verifier
  commit and evidence are retained as provenance, while temporary verifier
  code and task-specific passive counters/tests are absent from the
  operational worktree. SA-06 is active.

#### SA-06 — Rebaseline and select evidence-supported work

After every rejected candidate, preserve referenced raw evidence and the
candidate patch, remove its behavior and temporary material as specified
above, and rebuild the retained operational baseline. After an approved
retained candidate, record its commit and repeatable benchmark fingerprint
while keeping `470a3d7` as the immutable comparison anchor.

Compare accumulated evidence for null-move eligibility, futility denominators,
remaining move ordering, and the clock observation. Add exactly one new SA task
only if a repeated signal in two independent cases or a deterministic
correctness failure identifies it. Otherwise close the workstream without an
unconditional wishlist. `docs/roadmap.md` remains unchanged throughout.

### Experiment record template

- Hypothesis:
- Baseline:
- Candidate:
- Measurements:
- Result:
- Artifact path:
- Disposition:
