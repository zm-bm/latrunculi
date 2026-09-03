# Engine Roadmap

This is the authoritative development backlog for Latrunculi. The
[architecture overview](architecture.md) describes the current implementation;
this document contains only remaining work.

The current goal is Latrunculi 1.0: a stable, well-tested public release with
documented playing strength. Development and tuning will continue through later
versions.

**Now** is the active, ordered workstream. **Next** contains substantial work
that follows from it. **Later** is an informal backlog rather than a commitment.
Before implementation, revalidate each identified task against the current
source and produce an implementation-ready plan. Remove completed tasks instead
of maintaining a historical log.

## Now

### RELEASE-001 — Publish Latrunculi 1.0

Prepare the first stable public release with documented playing strength.

- Complete one joint linear-evaluation experiment using the
  [tuning workflow](../tools/tuning/workflow.md). Keeping the current baseline
  is valid when the selected candidate fails validation or strength testing.
- Require every retained parameter change to pass an OpenBench strength
  screen.
- Verify supported GCC and Clang release builds and the complete test suite.
- Complete the [OpenBench release stability test](openbench.md#release-stability-test).
- Record the deterministic benchmark, component measurements, representative
  OpenBench results, and a match against a named external engine. Record the
  opponent version, settings, game count, and result without requiring a fixed
  Elo threshold.
- Confirm UCI behavior, usage documentation, version output, and release
  packaging.
- Tag and publish a reproducible source release and engine binary.

Latrunculi 1.0 focuses on a reliable, tuned handcrafted engine with documented
playing-strength results. NNUE, tablebases, and further strength development
belong to later releases.

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
- Revalidate potential sources of search instability before treating them as
  current defects:
  - Establish fresh-process, single-thread determinism with identical position,
    limit, Hash, and configuration inputs.
  - Measure Hash-size sensitivity across a fixed suite, comparing nodes, scores,
    root moves, PVs, and TT statistics.
  - Audit aspiration convergence, root PVS and scout windows, re-searches, and
    full-window verification of competitive root moves.
  - Measure LMR reductions and verification searches, especially for good quiet
    moves ordered late by TT or history state.
  - Recheck static opening-evaluation shape independently from search results.
- Revalidate search-strength opportunities after the stability investigations,
  including time management, verified null-move pruning, and zugzwang handling.
- Measure multi-thread search scaling and TT/cache contention before changing
  the parallel-search design.
- Evaluate move-ordering improvements such as complete capture-history
  integration only after the current ordering baseline is measured.
- Add continuous integration for supported GCC and Clang builds, tests,
  ASan/UBSan, and a separate ThreadSanitizer configuration.
- Consider Lichess operation, tournament submission, and broader public testing
  after the 1.0 release.
