# Handcrafted Evaluation Roadmap

## Purpose

This document is the staged roadmap for improving Latrunculi's handcrafted
evaluation (HCE) before considering NNUE. It separates four kinds of work that
must not be conflated:

- **Behavior-preserving organization:** source ownership and interfaces change,
  but every position keeps the same evaluation.
- **Correctness work:** a feature's chess meaning changes because the current
  implementation is demonstrably wrong or internally inconsistent.
- **Strength tuning:** parameters or feature definitions intentionally change
  scores and possibly search behavior.
- **Performance work:** runtime or memory behavior changes while evaluation
  values remain identical unless explicitly approved otherwise.

NNUE is deferred until the HCE is organized, measurable, roughly tuned,
profiled, and mathematically tunable. The HCE should remain useful afterward as
a readable reference, diagnostic implementation, and possible fallback.

## Current Architecture

### Source ownership

- `parameters.hpp` owns material values, piece-square tables, mobility tables,
  feature weights, masks, phase limits, `eval::Phase`, and typed parameter
  lookups.
- `tapered_score.hpp` owns `eval::TaperedScore`; `trace.hpp` owns the evaluation
  term taxonomy and structured trace data.
- `base_terms.hpp` owns the Board-maintained material and piece-square cache.
- `evaluation.hpp/.cpp` provide the public evaluation entry points, while
  `evaluator.hpp` and `evaluator_detail.hpp` contain the private, single-use
  mechanics.
- `trace.hpp/.cpp` define structured evaluation results, while
  `trace_formatter.hpp/.cpp` own their human-readable diagnostic presentation.

All HCE-owned declarations and entry points live in `namespace eval`.
Production callers use only `eval::evaluate(board)` and
`eval::evaluate_trace(board)`; the stateful `Evaluator` cannot be constructed,
copied, moved, or reused outside its trusted implementation and narrow test
seam. `EvalValue` and mate/search sentinels remain engine-wide because they also
represent search scores.

### Evaluation data flow

`Board` owns an `eval::BaseTerms` containing its incrementally maintained
material and piece-square terms. Piece mutations update that state; FEN
loading, copies, ordinary moves, captures, promotions, castling, en passant,
null moves, and complete unmake histories are covered by independent
recomputation checks. The direct `board -> eval` dependency is deliberate and
explicitly HCE-specific rather than disguised as intrinsic Board state.

Each call to `eval::evaluate(board)` constructs a fresh `eval::Evaluator`.
Construction initializes king attacks and evaluation zones. Evaluation then
runs terms in a significant order: pawn and piece terms build attack, mobility,
threat, and king-attacker state that later king, mobility, and threat terms
consume. A refactor must not reorder these operations merely because the final
score is presented as a list of independent terms.

The final white-relative tapered score is scaled in the endgame according to
the stronger side's pawn count, blended by non-pawn-material phase, converted
to side-to-move perspective, and given a tempo bonus.

### Consumers outside `src/eval`

Evaluation affects more than leaf scores:

- Search calls `eval::evaluate(board)` at max ply, for razoring and futility
  static evaluations, for quiescence stand pat, and when resetting the root
  result.
- Search margins are expressed in the same score scale: aspiration 50,
  futility 250/400/550, and razoring 500/900/1800.
- Board SEE uses middlegame material values.
- Noisy-move ordering uses the captured piece's middlegame value.
- Null-move eligibility uses non-pawn material and the rook value.
- UCI's local `eval` console command formats `eval::evaluate_trace(board)` and
  writes the resulting table to the diagnostic stream.

Consequently, changing material values can alter static scores, SEE, move
ordering, pruning eligibility, and the searched tree. Parameter tuning cannot
be validated by evaluator unit tests alone.

### Normal and diagnostic evaluation

Normal and traced evaluation share one ordered implementation selected at
compile time. Normal search has no callback, type-erased state, or runtime trace
branch. A trace records term contributions and each transformation from the
unscaled white-relative tapered score through endgame scaling, blending,
side-to-move conversion, and tempo. Formatting consumes only that structured
value and has no access to evaluator internals.

### Tests and measurement

The evaluation tests are organized by parameters, tapered-score arithmetic,
end-to-end evaluation, and feature mechanics. They cover:

- tapered-score arithmetic and piece-square color symmetry;
- side-to-move and null-move perspective;
- pawn structure, mobility, pins, piece features, threats, shelter, king
  danger, phase, scaling, and tapering;
- trace-to-normal consistency and stable diagnostic formatting;
- independent recomputation of Board's incremental material/PSQT state.

Feature tests prefer structured traces and use a narrow Board-oriented test
access seam only for private geometry or transformation mechanics. Exact
weighted feature assertions remain useful refactor guards, but must not become
permanent correctness requirements once intentional tuning begins.

The benchmark tooling now provides four distinct measurements:

- deterministic evaluation snapshots over a checked-in diagnostic corpus;
- isolated `eval::evaluate()` throughput over preconstructed Boards;
- fixed-depth or movetime UCI search runs with comparable artifacts;
- paired baseline-versus-candidate matches through Cute Chess.

Snapshots protect exact behavior, throughput measures evaluator speed, search
runs expose downstream tree and speed effects, and matches provide strength
evidence. None substitutes for another. Parameter-independent feature export
and tuning datasets remain future work.

## Phase 1: Bounded Organizational Refactor

### Completed — ORG-001 through ORG-005

The bounded organizational phase is complete:

- **ORG-001:** established `eval` namespace and parameter ownership, including
  one authority for material values.
- **ORG-002:** separated normal evaluation, structured tracing, and diagnostic
  formatting without adding runtime trace overhead to search.
- **ORG-003:** moved non-template mechanics out of the implementation header and
  divided tests by responsibility with a narrow access seam.
- **ORG-004:** made Board's evaluation base-term cache explicit and
  independently verifiable across representation and history transitions.
- **ORG-005:** established `evaluation.hpp` as the public boundary, kept the
  stateful evaluator internal, and renamed the Board cache to
  `eval::BaseTerms`.

A final lifecycle cleanup made the stateful evaluator single-use, removed
transient final-result fields, clarified trace-stage names, and narrowed the
formatter header. The complete phase preserved evaluation values, UCI
diagnostic bytes, and deterministic depth-five depth, seldepth, nodes, scores,
principal variations, and best moves across the six-position search suite.

These findings are retained only as historical context. No further broad
organizational work is planned before tuning.

## Phase 2: Measurement and Baselines

### Completed — INFRA-001 through INFRA-003

The measurement phase is complete without changing engine behavior:

- **INFRA-001:** added a checked-in 24-position diagnostic corpus and
  deterministic long-form evaluation snapshots with explicit emit, verify,
  and regenerate commands.
- **INFRA-002:** added a single-threaded, fixed-work `eval::evaluate()`
  throughput benchmark with warmup, repeated samples, deterministic checksums,
  standard run artifacts, and compatible-run comparison.
- **INFRA-003:** added reproducible paired Cute Chess matches against archived
  baseline binaries, with a checksum-pinned external opening suite, smoke and
  standard profiles, raw PGNs/logs, W/D/L, pentanomial counts, and
  candidate-relative confidence reporting.

The commands and artifact contracts are documented in the
[benchmark guide](../bench/README.md). Evaluation snapshots are diagnostic
regression data, not training data or a strength metric. Timed throughput has
no pass/fail threshold. Smoke matches validate orchestration only; strength
claims require adequately sized standard matches whose reported interval
excludes zero in the candidate's favor.

This completed infrastructure is now a prerequisite rather than an open work
item for the tuning and performance findings below.

## Phase 3: Human-Guided Correction and Rough Tuning

These findings intentionally may change evaluations and searched trees. They
must not be bundled with organizational refactors.

### TUNE-001 — Audit feature semantics before changing weights

**Motivation and evidence**

The evaluator contains interacting, order-dependent feature mechanics:
isolated/backward/doubled pawns, mobility zones, pinned movement, weak pieces,
outposts, bishop blockers, rook files, queen discoveries, shelter/storm,
attacker accumulation, nonlinear king danger, phase, and endgame scaling.
Changing weights before validating feature activation risks tuning around a
bug.

**Intended outcome**

- Review each feature definition using traceable paired positions that change
  one concept at a time.
- Add full-evaluation mirror/color invariants and monotonic relationships where
  chess meaning permits them.
- Separate confirmed semantic defects into focused correctness commits with
  direct regression positions.
- Revisit the opening-shape concern recorded in `docs/search-stability.md`
  without using evaluation changes to hide TT/LMR instability.

**Likely components**

Evaluator mechanics, feature-focused corpus positions, trace tests, and search
stability measurements.

**Dependencies and ordering**

Requires INFRA-001/002. Correctness changes should precede parameter tuning and
receive independent match/search evaluation.

**Tests and measurement**

Feature activation/deactivation pairs, symmetry, trace consistency, corpus
diffs, fixed-depth search comparisons, and smoke matches.

**Risks**

Calling a subjective preference a correctness bug, changing several features
at once, or accepting a better-looking trace that weakens play.

**Completion criteria**

Every active feature has documented semantics and representative positions;
confirmed defects are fixed separately; remaining behavior is an intentional
baseline for rough tuning.

### TUNE-002 — Perform a staged human-guided parameter pass

**Motivation and evidence**

The README explicitly labels the engine not yet tuned. Current values include
large hand-selected material ratios, extensive PSQTs, mobility curves, and
nonlinear king-safety parameters. Existing opening evidence suggests some
near-equal priors may be too compressed, but search instability is also a known
confounder.

**Intended outcome**

- Tune coherent parameter groups separately rather than changing the entire
  table at once.
- Recommended order: score scale/material and tempo; phase/endgame scaling;
  PSQTs; pawn/minor/rook/queen features; mobility/threats; king safety last.
- Use structured traces and representative positions to justify broad changes.
- Preserve an explicit baseline for each group and require fixed-depth search
  plus match evidence before retaining it.
- Recalibrate or at least audit SEE, noisy ordering, null-move material guards,
  aspiration, razoring, and futility assumptions whenever the score scale or
  material values change.

**Likely components**

Evaluation parameters and possibly search thresholds whose units depend on the
evaluation scale. Each parameter group should normally be its own focused
change.

**Dependencies and ordering**

After TUNE-001 and all measurement infrastructure. Complete before final
profile-guided optimization so performance work targets the retained feature
set.

**Tests and measurement**

Reviewed corpus deltas, unchanged structural invariants, isolated evaluation
benchmark, fixed-depth search suite, and paired engine matches. Exact tunable
goldens may change only with an intentional reviewed snapshot update.

**Risks**

Overfitting handpicked openings, compensating for search defects, losing score
calibration, or attributing noisy short-match results to a parameter change.

**Completion criteria**

The retained rough parameter set is explainable, has no known semantic defects,
preserves invariants, and performs at least neutrally in adequately sized
baseline matches while improving targeted evaluation behavior.

## Phase 4: Profile-Guided Performance Optimization

### PERF-001 — Optimize the retained HCE hot path with exact score equivalence

**Motivation and evidence**

Potential costs visible in the current implementation include constructing a
fresh evaluator per call, repeatedly querying attacks for threat terms, and
maintaining several derived attack/zone arrays. Source inspection alone does
not establish which cost dominates.

**Intended outcome**

- Profile release builds over both isolated evaluation and representative
  search before selecting optimizations.
- Address measured costs one focused change at a time: repeated attack
  computation, data layout, branch behavior, construction overhead, or
  carefully justified inlining/code-size changes.
- Require exact corpus score equivalence for every performance-only change.
- For deterministic fixed-depth configurations, compare nodes, score, PV, and
  bestmove as an additional guard against hidden behavior changes.
- Preserve readable feature structure; do not turn the HCE into opaque SIMD or
  generated code merely to chase a microbenchmark.

**Likely components**

Evaluator mechanics, benchmark/instrumentation tooling, and possibly Board
query/cache usage. Specific files depend on profiler evidence.

**Dependencies and ordering**

After TUNE-002 stabilizes the rough feature set and INFRA-002 provides reliable
measurement. Individual optimizations remain separate commits.

**Tests and measurement**

Exact score/trace checksum equality, full tests, sanitizer coverage, isolated
throughput distributions, and downstream fixed-depth search comparisons.

**Risks**

Benchmark noise, code-size/cache regressions from excessive inlining,
duplicating derived state, undefined behavior in low-level optimizations, or
making mathematical feature extraction harder.

**Completion criteria**

Each retained optimization has profiler evidence, exact evaluation equivalence,
and a repeatable throughput/search improvement without reducing maintainability
disproportionately.

### PERF-002 — Add a worker-local pawn evaluation hash

**Motivation and evidence**

Pawn structure changes less frequently than complete positions and often
recurs through transpositions. `Evaluator::evaluate_pawns()` currently
recomputes isolated, backward, and doubled-pawn scores on every evaluation.
It also initializes pawn attacks and double attacks that later mobility,
threat, and king-safety terms consume, so caching only its returned score would
be incomplete.

Dedicated pawn caches are an established HCE technique in the reference
engines: Minic uses a per-thread pawn table, Ethereal uses a per-thread
pawn-and-king table, and CPW has a pawn table separate from its search TT.
Latrunculi's `search::Worker` already provides the natural non-shared lifetime
for a small cache without adding synchronization to the evaluation hot path.

**Intended outcome**

- Add a fixed-size, worker-local pawn evaluation table keyed only by the white
  and black pawn placement, not by the complete position key.
- Cache the complete pawn result needed by later evaluation: the tapered pawn
  score for both sides and the derived pawn attack/double-attack state.
- Define explicit replacement, collision-verification, sizing, clearing, and
  search-to-search lifetime policies.
- Pass the cache through the normal search evaluation path while keeping
  standalone and diagnostic evaluation usable without hidden global state.
- Preserve exact evaluation values, traces, fixed-depth search results, and
  thread independence on both cache hits and misses.
- Measure hit rate, probe cost, memory per worker, isolated evaluation
  throughput, and whole-search performance before retaining the design.
- Do not add a dedicated complete-position evaluation hash or repurpose the
  search transposition table as part of this finding.

**Likely components**

A focused pawn-table type under `src/eval`, evaluator input/state boundaries,
`search::Worker` ownership and evaluation call sites, optional instrumentation,
and evaluation/search tests and benchmarks. Key generation may require a
pawn-only key maintained by `Board` or a verified hash derived from the two
pawn bitboards; the implementation plan must compare those choices and protect
make/unmake correctness if Board state is extended.

**Dependencies and ordering**

Requires INFRA-002 for meaningful measurements. The structured trace and
explicit Board evaluation-state boundary are already established; if the plan
selects an incremental pawn key, it must extend the existing independent Board
recomputation coverage. Perform after TUNE-002 stabilizes the pawn feature set.
PERF-001 should first establish profile evidence and may remove unrelated
hot-path costs, but this remains a separate focused change.

**Tests and measurement**

Exercise hit, miss, replacement/collision, and clear/lifetime behavior; compare
cached and uncached scores and structured traces over the corpus; verify that
all pawn-derived attack state is identical; cover Board make/unmake if an
incremental pawn key is selected; run multi-worker sanitizer coverage; require
deterministic fixed-depth score, nodes, PV, and best-move equivalence. Report
cache hit rate and memory per worker alongside isolated and whole-search
benchmark results.

**Risks**

Using the full position key and receiving little reuse, accepting an
unverified collision, restoring a score without its derived attacks, stale
entries after parameter changes, excessive per-worker memory, or introducing
shared-table synchronization that costs more than recomputation.

**Completion criteria**

Every hit is behaviorally indistinguishable from recomputing pawn evaluation;
the table has explicit ownership and collision/lifetime policies; tests cover
the full cached result rather than only its score; memory and hit-rate data are
recorded; and the retained implementation produces a repeatable evaluation or
search-performance improvement without changing chess behavior.

## Phase 5: Mathematical Tuning

### MATH-001 — Export parameter-independent features and construct datasets

**Motivation and evidence**

The evaluator currently multiplies weights directly into `TaperedScore`
results. A weighted debug trace cannot recover the raw sparse coefficients
needed to tune PSQTs and feature weights independently. No labeled WDL dataset
or train/validation split exists.

**Intended outcome**

- Define a deterministic, versioned feature schema covering tunable linear
  middlegame/endgame coefficients and sparse PSQT entries.
- Add an extraction mode/tool that emits raw feature coefficients, phase,
  scaling context, side to move, and source position identity without changing
  normal search evaluation.
- Build a deduplicated labeled position dataset from games.
- Split by game, not individual position, into training, validation, and held-out
  test sets to avoid leakage.
- Keep the small diagnostic corpus separate from training data.
- Record filtering policy, sampling balance, result labels, schema version, and
  reproducible seeds.

**Likely components**

Evaluation feature representation, offline extraction tooling, dataset scripts,
and documentation. Large generated datasets should not be committed blindly.

**Dependencies and ordering**

After rough feature definitions stabilize and preferably after PERF-001 so
extraction can remain clearly separate from the hot path.

**Tests and measurement**

Reconstruct current linear evaluation components from exported features,
validate symmetry and deterministic schemas, detect duplicate/leaked games,
and verify split/result distributions.

**Risks**

Data leakage, biased game sources, inconsistent side-to-move conventions,
silent schema drift, or trying to encode nonlinear king/scale behavior as
ordinary independent linear weights.

**Completion criteria**

Feature export is reproducible and can reconstruct its supported evaluation
terms; datasets and splits are documented, deduplicated, versioned, and held-out
validation remains untouched by fitting.

### MATH-002 — Implement reproducible Texel-style linear tuning

**Motivation and evidence**

Material, PSQT, mobility, and many positional bonuses are suitable for
supervised WDL-error minimization, but their features are correlated and their
middlegame/endgame contributions are phase-coupled.

**Intended outcome**

- Fit the evaluation-to-WDL logistic scaling constant explicitly.
- Optimize selected linear middlegame/endgame parameter groups with a
  deterministic seed and recorded configuration.
- Use bounds, regularization, symmetry constraints, or staged groups to prevent
  implausible coefficients and cancellation between correlated features.
- Report training, validation, and held-out loss separately.
- Generate a reviewable parameter artifact or patch rather than mutating source
  constants opaquely.
- Validate retained candidates in fixed-depth search and engine matches; lower
  prediction loss alone is not sufficient.

**Likely components**

Offline tuner code, feature datasets, parameter serialization/generation, and
evaluation parameter tables.

**Dependencies and ordering**

Requires MATH-001 and INFRA-003. Tune coherent groups incrementally rather than
all parameters in one unreviewable run.

**Tests and measurement**

Synthetic optimizer tests, reproducible runs, held-out WDL loss, coefficient
sanity reports, corpus score deltas, fixed-depth search, and paired matches.

**Risks**

Overfitting, correlated features producing extreme weights, optimizing noisy or
search-biased labels, and accepting statistically insignificant match results.

**Completion criteria**

The tuner is reproducible; selected parameters improve held-out loss; retained
changes preserve structural invariants and have credible match evidence.

### MATH-003 — Tune nonlinear evaluation and search-coupled parameters separately

**Motivation and evidence**

King danger is quadratic in a raw danger score; check danger, phase, endgame
scaling, and tempo are not independent linear feature weights. Evaluation scale
also interacts with aspiration, razoring, futility, SEE, and ordering.

**Intended outcome**

- Keep nonlinear king-safety, phase, scaling, and tempo experiments out of the
  first linear fit.
- Use staged grid/search methods, constrained optimization, or SPSA/self-play
  where supervised linear fitting is not valid.
- Tune search thresholds separately from HCE parameters even when their units
  are coupled.
- Require paired engine matches and statistical confidence for final adoption.

**Likely components**

Nonlinear evaluation formulas/parameters, match tooling, and later separate
search-tuning work.

**Dependencies and ordering**

After MATH-002 establishes a stable linear baseline.

**Tests and measurement**

Formula boundary/monotonicity tests, held-out prediction metrics where
meaningful, search stability comparisons, and statistically evaluated matches.

**Risks**

Large noisy search spaces, compensating parameters, invalid interpolation,
destabilized pruning, and confounding evaluation with search-policy changes.

**Completion criteria**

Every retained nonlinear or search-coupled change has an explicit method,
reproducible configuration, structural tests, and match evidence beyond the
linear baseline.

## Deferred: NNUE

NNUE work begins only after the HCE pipeline above is operational. No current
finding should introduce a speculative virtual evaluator interface or store an
NNUE accumulator/network in `Board`.

The later design must respect these constraints:

- Network weights are immutable shared state owned above individual workers.
- Each search worker owns mutable accumulator stacks and any refresh cache.
- Accumulator push/pop must stay coherent with ordinary moves, captures,
  promotions, castling, en passant, null moves, root installation, Board copies,
  and complete unmake.
- A scalar full-refresh implementation should establish correctness before
  incremental updates or SIMD.
- The HCE remains available as a reference implementation and diagnostic
  comparator while NNUE is developed.
- Network loading, UCI options, fallback policy, and distribution are later
  integration decisions, not part of the HCE refactor.

Entry criteria for an NNUE roadmap are: stable evaluation ownership, structured
tracing, corpus and throughput benchmarks, match infrastructure, a documented
Board/evaluator state boundary, and completed rough/mathematical HCE tuning.

## Remaining Execution Sequence

1. **TUNE-001** — audit and correct feature semantics.
2. **TUNE-002** — perform staged human-guided rough tuning.
3. **PERF-001** — profile and optimize the retained HCE with exact equivalence.
4. **PERF-002** — add and measure a worker-local pawn evaluation hash.
5. **MATH-001** — export raw features and build leakage-resistant datasets.
6. **MATH-002** — tune linear parameters with held-out validation.
7. **MATH-003** — tune nonlinear and search-coupled parameters separately.
8. Create a separate NNUE roadmap only after the entry criteria are met.

### Change contract by phase

| Findings | Evaluation values | Search behavior | Required evidence |
|---|---|---|---|
| TUNE-001 correctness fixes | Intentional only for confirmed defects | May change | Direct regression, corpus diff, search/match checks |
| TUNE-002 | Intentionally changes | Intentionally may change | Trace rationale, corpus diff, fixed-depth suite, matches |
| PERF-001 and PERF-002 | Exact preservation | Exact preservation at fixed depth | Score/trace checksum, profiler/cache metrics, throughput and search benchmark |
| MATH-001 | No normal-path change | No normal-path change | Feature reconstruction and dataset validation |
| MATH-002 and MATH-003 | Intentionally changes | Intentionally may change | Held-out metrics, coefficient sanity, statistical matches |
