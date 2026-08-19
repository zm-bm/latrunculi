# Engine Roadmap

This is the authoritative development backlog for Latrunculi. The
[architecture overview](architecture.md) describes the current implementation;
this document contains only remaining work.

The current goal is Latrunculi 1.0: a stable, well-tested public release with
documented playing strength. Development and automated tuning will continue
through later versions.

**Now** is the active, ordered workstream. **Next** contains substantial work
that follows from it. **Later** is an informal backlog rather than a commitment.
Before implementation, revalidate each identified task against the current
source and produce an implementation-ready plan. Remove completed tasks instead
of maintaining a historical log.

## Now

The immediate workstream deploys durable strength testing, hardens the engine,
and mathematically tunes the existing handcrafted evaluation before publishing
Latrunculi 1.0. These tasks should be executed in order.

### Strength-validation policy

Use the self-hosted OpenBench instance for retained strength claims. Compare
against the pre-change revision with paired, color-reversed games, a pinned
opening suite, one thread per engine, and 32 MB Hash. Start with a `10+0.1` time
control, resign adjudication at 400 cp for three moves, and draw adjudication
after move 40 with eight evaluations within 10 cp; version later changes. Use
normalized-Elo SPRT bounds `[0, 5]` with `alpha = beta = 0.05` to screen
candidates and `[0, 3]` to confirm a retained batch. Record the OpenBench test
ID, both revisions, configuration revision, decision, and PGN location.

### OPS-001 — Deploy the OpenBench workstation

Move the validated OpenBench workflow to the always-on private workstation.

- Deploy the OpenBench fork at revision `1ede996` with a dedicated Python 3.11
  virtual environment.
- Migrate the local database and PGN archive, and keep credentials in an
  untracked host environment file.
- Run the server and worker as restartable services with private network access
  and an explicit worker resource limit.
- Back up the database and PGN archive, and allow books and engine builds to be
  regenerated from their pinned sources.
- Verify restart recovery and complete a paired two-game Latrunculi smoke test
  using the pinned UHO opening suite.

Completion requires persistent server state, automatic worker reconnection,
successful benchmark validation, and complete match results after a service or
machine restart.

### REL-001 — Harden the engine for release

Add repeatable stress testing for correctness, concurrency, and long-running
engine stability.

- Run the complete suite under AddressSanitizer and UndefinedBehaviorSanitizer,
  with ThreadSanitizer as a separate configuration.
- Exercise randomized legal positions, move round trips, evaluation, and short
  searches with deterministic seeds and reproducible failure inputs.
- Run a substantial self-play soak through OpenBench and reject crashes, hangs,
  illegal moves, protocol failures, and incomplete games.
- Preserve the seed, FEN, PGN, command, and diagnostic output for every failure.
- Keep performance thresholds and playing-strength conclusions outside this
  task.

Completion requires clean sanitizer runs, reproducible randomized stress
coverage, and a completed self-play soak without engine failures.

### MATH-001 — Export raw features and construct tuning datasets

The current evaluation trace records weighted contributions. Mathematical
tuning instead needs parameter-independent feature coefficients and reliable
WDL labels.

- Define a deterministic, versioned schema for the linear HCE features,
  including MG/EG coefficients, phase and scaling context, side to move, game
  result, and source identity.
- Prove that exported coefficients reconstruct the supported production
  evaluation before fitting parameters.
- Build a large, deduplicated position dataset and split it by game into
  training, validation, and untouched held-out sets.
- Filter malformed games, terminal positions, recognized dead positions, and
  other samples unsuitable for static-evaluation labels.
- Record sampling, balancing, quiet-position policy, seeds, schema version, and
  filtering decisions. Preserve a dedicated endgame validation slice.
- Keep exporter code, dataset construction, reproducible configuration, and
  compact fixtures under `bench/`; keep generated data under an ignored
  `bench/workspace/` directory.

Completion requires deterministic export, exact feature reconstruction,
schema validation, duplicate and split-leak detection, and useful distribution
reports. Production evaluation and search behavior must remain unchanged.

### MATH-002 — Tune the linear handcrafted evaluation

Tune the existing linear and phase-coupled feature groups reproducibly before
adding more evaluation knowledge.

- Fit the evaluation-to-WDL logistic scale explicitly.
- Tune coherent groups such as material, PSQTs, mobility, pawn structure, and
  piece bonuses with deterministic seeds and configuration.
- Keep the middlegame pawn fixed at 100 centipawns as the score-scale anchor.
- Apply bounds, regularization, symmetry constraints, or staged groups to
  control correlated and implausible coefficients.
- Report training, validation, held-out, and endgame-slice loss separately.
- Produce a reviewable parameter patch or artifact rather than mutating source
  constants opaquely.

Retained parameters must improve held-out prediction loss, preserve evaluation
invariants, remain structurally plausible, and pass focused tests, component
measurements, fixed-depth search review, and the OpenBench workflow. Prediction
loss alone is not evidence of playing strength.

### RELEASE-001 — Publish Latrunculi 1.0

Prepare the first public release after the initial handcrafted-evaluation
tuning pass.

- Require completion of OPS-001, REL-001, MATH-001, and MATH-002.
- Verify supported GCC and Clang release builds and the complete test suite.
- Record the deterministic benchmark, component measurements, and
  representative OpenBench results.
- Confirm UCI behavior, usage documentation, version output, and release
  packaging.
- Tag and publish a reproducible source release and engine binary.

Latrunculi 1.0 focuses on a reliable, tuned handcrafted engine with documented
playing-strength results. NNUE, tablebases, and further strength development
belong to later releases.

## Next

### END-001 — Audit endgame residuals

After linear tuning, analyze the largest held-out endgame errors and compare
them with exact tablebase WDL/DTZ results where available. Separate missing
general features from exact material rules, draw scaling, search horizon, and
tablebase-covered play. Produce an evidence report by material class and
failure type; a production change is not required.

### END-002 — Add justified endgame mechanisms

Plan individual mechanisms only for repeated, explainable failures found by
END-001. Candidates may include draw scaling, wrong-bishop rook-pawn handling,
passed-pawn race context, mop-up guidance, or a small exact bitbase. Each change
requires activation and counterexample tests, held-out or tablebase evidence,
retraining of affected linear parameters, and paired match validation.

### MATH-003 — Tune nonlinear and search-coupled behavior

Tune king-danger conversion, phase, scaling, and tempo separately from the
linear fit. Treat aspiration, razoring, futility, SEE-dependent policy, and
other search thresholds as search tuning even when they share score units.
Require constrained, reproducible optimization, formula-boundary tests,
fixed-depth search review, and OpenBench evidence.

### TB-001 — Add optional Syzygy support

Add optional WDL and DTZ probing without bundling tablebase files. Define UCI
configuration, unavailable-path behavior, supported positions, probe depth,
fifty-move handling, root move selection, and multi-threaded access. Preserve
ordinary search when tablebases are disabled and validate correctness,
performance, and equal-access matches.

## Later

- Design an NNUE backend after the HCE tuning pipeline is operational. Preserve
  the HCE as a readable reference and account for shared immutable networks,
  per-worker accumulator state, and Board make/unmake synchronization.
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
