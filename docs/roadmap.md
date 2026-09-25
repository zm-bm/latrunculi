# Engine Roadmap

This document records the engine's longer-horizon direction and backlog. **Next** is ordered and
**Later** is informal. Detailed engine experiments and durable search evidence belong in
[Engine Development](engine-development.md), while the
[architecture overview](architecture.md) describes the current implementation.

Revalidate an item before starting it and remove it when complete; Git history records completion.

## Next

### END-001 — Improve endgame play from exact evidence

Use held-out endgame errors and exact tablebase WDL/DTZ results to guide
improvements to general features, material rules, draw scaling, and horizon
handling. When ready, define one bounded `ENG-XXX` investigation in
`engine-development.md` rather than coordinating it here.

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
- Improve multi-thread search scaling and TT/cache behavior before changing
  the parallel-search design.
- Add continuous integration for supported GCC and Clang builds, tests,
  ASan/UBSan, and a separate ThreadSanitizer configuration.
- Consider Lichess operation, tournament submission, and broader public testing
  when engine and operational readiness justify public deployment.
