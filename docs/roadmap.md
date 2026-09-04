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

### RELEASE-001A — Prepare the Latrunculi 1.0 release candidate

Freeze the accepted tuned engine as the first public-release candidate.

- Set the project version to `1.0.0` and require the UCI identity to report it.
- Review the public build, usage, feature, license, and platform documentation.
- Define the minimal reproducible source and binary release artifacts.
- Commit and push the exact candidate revision without tagging it. Any later
  playing-code change requires strength screening and a new release candidate.

### RELEASE-001B — Verify the release candidate

Validate the exact revision prepared by RELEASE-001A and record reproducible
release evidence.

- Verify supported GCC and Clang release builds, the complete test suite, and
  the ASan/UBSan and TSan configurations.
- Verify the UCI handshake, version, documented options, and normal shutdown.
- Record the compiler and hardware, deterministic benchmark, and component
  measurements.
- Complete the 2,000-game
  [OpenBench release stability test](openbench.md#release-stability-test) with
  no crashes, hangs, time losses, illegal moves, protocol failures, or
  incomplete games. Record the revision, OpenBench revision, test ID, and PGN
  location.
- Repeat affected checks if the candidate source changes.

### RELEASE-001C — Establish external strength context

Run a reproducible gauntlet that places the release candidate in a recognizable
playing-strength range. The gauntlet is required, but its score is not a
release gate.

- Select at least three pinned engine versions from one published rating list.
  Use short paired pilots to find opponents that reasonably bracket
  Latrunculi, and record their source or binary provenance.
- Before the final matches, fix the opponents, game budget, settings, and
  analysis method. Use paired openings on the same hardware with one thread,
  32 MiB hash, no tablebases, and the standard OpenBench book, time control,
  and adjudication.
- Run fixed-game matches and record each opponent's version and reference
  rating, the complete settings, game count, WDL, score, and uncertainty.
- Report an approximate performance rating anchored to the named rating list.
  State that ratings from different hardware and testing conditions are not
  directly interchangeable, and require no minimum result for release.

### RELEASE-001D — Publish Latrunculi 1.0

Publish only after RELEASE-001A through RELEASE-001C are complete.

- Confirm that the release revision is the exact revision already verified.
- Write concise release notes with the benchmark, measurements, relevant
  OpenBench results, stability result, and external-strength context.
- Produce the documented source and binary artifacts with checksums.
- Tag the revision as `v1.0.0`, publish the release, and verify the downloaded
  artifacts and reported engine version.

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
