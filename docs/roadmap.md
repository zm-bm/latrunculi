# Engine Roadmap

This is the authoritative backlog for work not currently managed as an
`SW-XX` search experiment. The [architecture overview](architecture.md)
describes the current implementation, and active search experiments are
coordinated separately in [Search Development](search.md).

**Now** contains at most one active roadmap item, **Next** contains ordered
candidates, and **Later** is an informal backlog. Revalidate items before work
and remove them when complete; Git history records completion.

## Now

There is no active roadmap item.

## Next

### END-001 — Audit endgame residuals

Analyze the largest held-out endgame errors and compare them with exact
tablebase WDL/DTZ results where available. Separate missing general features
from exact material rules, draw scaling, search horizon, and tablebase-covered
play. Produce an evidence report by material class and failure type; a
production change is not required.

### END-002 — Scope justified endgame mechanisms

If END-001 supports production work, replace this placeholder with separate
stable-ID items for the repeated, explainable mechanisms it identifies; do not
implement several mechanisms under this item. Each resulting change requires
activation and counterexample tests, held-out or tablebase evidence, retraining
of affected linear parameters, and paired match validation. Remove this item if
the audit supports no action.

### TB-001 — Add optional Syzygy support

Add optional WDL and DTZ probing without bundling tablebase files. Define UCI
configuration, unavailable-path behavior, supported positions, probe depth,
fifty-move handling, root move selection, and multi-threaded access. Preserve
ordinary search when tablebases are disabled and validate correctness,
performance, and equal-access matches.

## Later

- Treat an NNUE backend as a separate architecture project. Preserve the HCE as
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
  when engine and operational readiness justify public deployment.
