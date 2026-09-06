# Search Audit

SEARCH-001 audited the current search before any pruning change. The audit did
not isolate a search mechanism that satisfies the roadmap evidence gate, so it
does not propose a search-behavior follow-up. Three of the four primary pilot
moves were no longer selected at the representative fixed-node budget. The one
remaining primary case changed score, but not move, at the larger budget and
did not share a distinguishing counter profile with the controls.

## Baseline and provenance

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

## Suite and protocol

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

## Results

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

## Conclusion and limitations

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
