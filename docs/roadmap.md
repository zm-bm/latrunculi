# Engine Roadmap

This is the concise, longer-horizon direction and project backlog for the
engine. Detailed Elo-oriented experiments are coordinated in
[Strength Development](strength.md), while [Search Knowledge](search.md)
retains durable search evidence. The [architecture overview](architecture.md)
describes the current implementation.

Keep directions and larger capabilities here. Put bounded, measurable
investigations and candidates in `strength.md` as `EI-XXX` experiments rather
than duplicating them. Add a **Now** section only while a roadmap project is
active; **Next** is ordered and **Later** is informal. Revalidate items before
work and remove them when complete; Git history records completion.

## Next

### END-001 — Improve endgame play from exact evidence

Use held-out endgame errors and exact tablebase WDL/DTZ results to guide
improvements to general features, material rules, draw scaling, and horizon
handling. When ready, define one bounded `EI-XXX` investigation in
`strength.md` rather than coordinating it here.

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
