# Engine Roadmap

This is the authoritative development backlog for Latrunculi. The
[architecture overview](architecture.md) describes the current implementation;
this document contains only remaining work.

The current goal is Latrunculi 1.1: improve search quality from the stable,
tuned 1.0 baseline.

**Now** is the active, ordered workstream. **Next** contains substantial work
that follows from it. **Later** is an informal backlog rather than a commitment.
Before implementation, revalidate each identified task against the current
source and produce an implementation-ready plan. Remove completed tasks instead
of maintaining a historical log.

## Now

### SEARCH-001 — Audit search efficiency and selectivity

Determine why Latrunculi reports substantially less search depth before
changing pruning. Across three release pilots, its median reported depth was 12
against 16–18. The closest 206-game match finished 58–90–58; the largest
mismatch was too one-sided to diagnose. In lost games, opponent evaluations
first crossed two pawns around median move 26, and about one-third showed a
two-pawn jump between consecutive opponent evaluations. The failures therefore
include both abrupt tactical swings and gradual deterioration. Depth and
evaluation scales differ between engines, so treat these results as evidence
for investigation, not proof of one defect. Optimize move quality per unit
time, not reported depth.

- Build a reproducible, fresh-process, single-thread suite from competitive
  pilot games and existing search positions. Include both abrupt score swings
  and gradual declines; use heavily mismatched games only as secondary cases.
  Record fixed-time and fixed-node scores, moves, PVs, depths, and nodes. Check
  determinism and Hash sensitivity, and distinguish search errors from
  static-evaluation errors.
- Measure qsearch share, effective branching, cutoff order, TT effectiveness,
  aspiration and PVS re-searches, and the behavior of null move, razoring,
  futility pruning, and LMR with the existing instrumentation.
- Investigate only mechanisms supported by the measurements. Initial candidates
  are SEE or delta pruning in qsearch, capture-history integration,
  history-aware LMR, verified null move and zugzwang handling, and time
  management.
- Turn each justified change into a separate task with correctness tests,
  component measurements, and paired OpenBench validation. Do not increase
  reductions merely to raise reported depth.

## Next

### END-001 — Audit endgame residuals

Analyze the largest held-out endgame errors and compare them with exact
tablebase WDL/DTZ results where available. Separate missing general features
from exact material rules, draw scaling, search horizon, and tablebase-covered
play. Produce an evidence report by material class and failure type; a
production change is not required.

### END-002 — Add justified endgame mechanisms

Plan individual mechanisms only for repeated, explainable failures found by
END-001. Candidates may include draw scaling, wrong-bishop rook-pawn handling,
passed-pawn race context, mop-up guidance, or a small exact bitbase. Each change
requires activation and counterexample tests, held-out or tablebase evidence,
retraining of affected linear parameters, and paired match validation.

### TB-001 — Add optional Syzygy support

Add optional WDL and DTZ probing without bundling tablebase files. Define UCI
configuration, unavailable-path behavior, supported positions, probe depth,
fifty-move handling, root move selection, and multi-threaded access. Preserve
ordinary search when tablebases are disabled and validate correctness,
performance, and equal-access matches.

## Later

- Design an NNUE backend after the HCE baseline is stable. Preserve the HCE as
  a readable reference and account for shared immutable networks, per-worker
  accumulator state, and Board make/unmake synchronization.
- Add UCI capabilities when supported by the corresponding engine feature:
  MultiPV, richer bound and progress reporting, Chess960, and optional strength
  controls.
- Measure multi-thread search scaling and TT/cache contention before changing
  the parallel-search design.
- Add continuous integration for supported GCC and Clang builds, tests,
  ASan/UBSan, and a separate ThreadSanitizer configuration.
- Consider Lichess operation, tournament submission, and broader public testing
  after the 1.0 release.
