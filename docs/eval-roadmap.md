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

### Completed — TUNE-001 feature-semantics audit

The feature-semantics audit is complete. It corrected queen discovery
detection, strengthened structured color-symmetry and castling-shelter
coverage, and separated isolated from backward pawn penalties. It also
recorded deliberate policies for material authority, bishop-pair activation,
geometric threat counts, and horizontally asymmetric PSQTs.

No confirmed correctness defect or essential invariant-test gap remains open.
The retained feature definitions, supporting evidence, completed commits, and
remaining tuning candidates are documented in
[`eval-feature-audit.md`](eval-feature-audit.md). That document now preserves
the completed rough-tuning and performance evidence.

### Completed — TUNE-002 rough evaluation pass

The bounded rough pass retained passed-pawn scoring, a phase range based on the
full starting non-pawn material, softer generic endgame scaling, and pawn
attacks as a weak-piece threat signal. The middlegame pawn remained the fixed
100-centipawn scale anchor. Reviews of material ratios, tempo, PSQTs, remaining
pawn and piece weights, mobility curves, king safety, and downstream search
thresholds concluded with no change. A latent-mobility experiment was rejected
and reverted.

Three reduced checkpoint matches screened coherent groups. The final standard
match against the archived pre-pass engine scored 930/786/284 over 1,000
opening pairs (66.2%, +116.4 +/- 12.0 Elo, LOS 100%). This supports the
aggregate retained engine at those settings; it does not attribute the result
to an individual feature or replace later mathematical tuning. Detailed task
rationale, commits, snapshots, and match artifacts remain in
[`eval-feature-audit.md`](eval-feature-audit.md).

## Phase 4: Profile-Guided Performance Optimization

### Completed — PERF-001 and PERF-002

Profiling found that the release x86-64 build spent 17.6% of isolated
evaluation cycles in the software `__popcountdi2` helper. The retained change
enables hardware POPCNT for supported GNU/Clang x86-64 builds, with a CMake
option for older targets. Final isolated throughput improved from 260.321 to
185.470 ns/evaluation (-28.8%), or from 3.84 to 5.39 million evaluations per
second (+40.4%). Evaluation snapshots and deterministic fixed-depth search
results remained exact; downstream NPS also improved.

A 1,024-entry worker-local pawn evaluation cache was implemented and measured,
then rejected and fully reverted. Although hit rates ranged from 76.0% to
99.8%, its approximately 72 KiB per-worker cost and additional evaluator
boundary did not produce a repeatable search-speed improvement. Reconsider a
pawn cache only if pawn evaluation grows materially or profiling provides
stronger evidence. Full experiment details remain in
[`eval-feature-audit.md`](eval-feature-audit.md).

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

1. **MATH-001** — export parameter-independent features and construct datasets.
2. **MATH-002** — implement reproducible linear tuning with held-out
   validation.
3. **MATH-003** — tune nonlinear and search-coupled parameters separately.
4. Create a separate NNUE roadmap only after the entry criteria are met.

### Remaining change contract

| Findings | Evaluation values | Search behavior | Required evidence |
|---|---|---|---|
| MATH-001 | No normal-path change | No normal-path change | Feature reconstruction and dataset validation |
| MATH-002 and MATH-003 | Intentionally changes | Intentionally may change | Held-out metrics, coefficient sanity, statistical matches |
