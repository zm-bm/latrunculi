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
