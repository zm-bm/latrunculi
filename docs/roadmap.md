# Engine Roadmap

This is the authoritative development backlog for Latrunculi. The
[architecture overview](architecture.md) describes the current implementation;
this document contains only remaining work.

**Now** is the active, ordered workstream. **Next** contains substantial work
that follows from it. **Later** is an informal backlog rather than a commitment.
Before implementation, revalidate each identified task against the current
source and produce an implementation-ready plan. Remove completed tasks instead
of maintaining a historical log.

## Now

The immediate workstream completes exact draw handling, establishes repeatable
strength testing, and mathematically tunes the existing handcrafted evaluation.
These tasks should be executed in order.

### RULE-001 — Recognize dead positions by material

`Board::is_draw()` currently recognizes the fifty-move rule and repetition but
not positions in which no legal sequence can produce checkmate.

- Add a conservative, exact material predicate and use it in normal and
  quiescence search.
- Cover king-only and lone-minor cases while avoiding false draws such as
  K+NN versus K, where mate is possible even if it cannot be forced.
- Define promoted-bishop and bishop-square-color behavior from the dead-position
  rule rather than simple piece counts.
- Keep stalemate, repetition, the fifty-move rule, and tablebase results as
  separate concepts.

Completion requires positive and negative rules tests plus identical search
behavior outside newly recognized exact draws. No evaluation parameter changes
belong in this task.

### BENCH-001 — Establish the engine benchmark and OpenBench workflow

Add the deterministic engine benchmark and external workflow needed for
repeatable strength testing.

- Add `latrunculi bench` with a fixed low-depth position suite, deterministic
  aggregate node count, elapsed time, and nodes-per-second output compatible
  with OpenBench.
- Add the minimal OpenBench build entry point while retaining CMake as the
  normal project build system.
- Create a focused top-level `bench/` area for OpenBench configuration and the
  later tuning pipeline. Keep `latrunculi-measure` responsible only for local
  component performance.
- Pin a private OpenBench server and worker on the dedicated workstation,
  including opening-suite identity, engine options, time controls,
  adjudication, and test defaults.
- Complete paired fixed-game and short SPRT smoke tests before relying on the
  service for tuning decisions.

Repeated benchmark runs must produce the same aggregate node count. OpenBench
must build both revisions, normalize worker speed, and complete paired tests
whose engine revisions and configuration are recorded.

#### Strength-validation policy

Use the self-hosted OpenBench instance for retained strength claims. Compare
against the pre-change revision with paired, color-reversed games, a pinned
opening suite, one thread per engine, 32 MB Hash, and versioned time-control and
adjudication settings. Use normalized-Elo SPRT bounds `[0, 5]` with
`alpha = beta = 0.05` to screen candidates and `[0, 3]` to confirm a retained
batch. Record the OpenBench test ID, both revisions, configuration revision,
decision, and PGN location.

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
performance, and equal-access matches. This task may proceed independently
after RULE-001.

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
- Evaluate move-ordering improvements such as complete capture-history
  integration only after the current ordering baseline is measured.
