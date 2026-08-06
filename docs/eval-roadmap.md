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

The evaluation subsystem currently has five files totaling roughly 1,168 lines:

- `eval.hpp` contains piece values, piece-square tables, mobility tables, pawn,
  piece, threat, king-safety, phase, and scaling parameters. The name does not
  describe its actual role as the parameter repository.
- `tapered_score.hpp` defines the global `TaperedScore` value type.
- `types.hpp` defines the global, unscoped `EvalTerm` and `Phase` enums.
- `evaluator.hpp` contains the global `Evaluator`, nearly all evaluation
  implementation, debug score structures, and formatter specializations. At
  roughly 744 lines it mixes the hot evaluator, trace collection, formatting,
  and test access.
- `evaluator.cpp` contains only trace-score updates and `EvaluatorDebug`
  construction.

Only parameters and masks are currently inside `namespace eval`. Evaluation
types, the evaluator, its debug companion, and the free `evaluate()` function
remain global. `EvalValue` and mate/search sentinel values are also global or in
`eval_value`, but those are shared search score types rather than HCE-owned
types and need not move with the evaluator.

### Evaluation data flow

`Board` owns two HCE-specific incremental values:

- material score;
- piece-square bonus score.

`Board::add_piece()`, `remove_piece()`, and `move_piece()` update both values.
FEN loading builds them through those mutation functions; board copies copy
them; make/unmake handles captures, promotions, castling, and en passant by
reusing the same mutations. Null moves leave them unchanged. This is efficient,
but it creates a direct `board -> eval` dependency through `TaperedScore`,
material parameters, and piece-square tables.

Each call to `evaluate(board)` constructs a fresh `Evaluator`. Construction
initializes king attacks and evaluation zones. Evaluation then runs terms in a
significant order: pawn and piece terms build attack, mobility, threat, and
king-attacker state that later king, mobility, and threat terms consume. A
refactor must not reorder these operations merely because the final score is
presented as a list of independent terms.

The final white-relative tapered score is scaled in the endgame according to
the stronger side's pawn count, blended by non-pawn-material phase, converted
to side-to-move perspective, and given a tempo bonus.

### Consumers outside `src/eval`

Evaluation affects more than leaf scores:

- Search calls `evaluate(board)` at max ply, for razoring and futility static
  evaluations, for quiescence stand pat, and when resetting the root result.
- Search margins are expressed in the same score scale: aspiration 50,
  futility 250/400/550, and razoring 500/900/1800.
- Board SEE uses middlegame material values.
- Noisy-move ordering uses the captured piece's middlegame value.
- Null-move eligibility uses non-pawn material and the rook value.
- UCI's local `eval` console command constructs `EvaluatorDebug`, evaluates the
  position, formats its term table, and writes it to the diagnostic stream.

Consequently, changing material values can alter static scores, SEE, move
ordering, pruning eligibility, and the searched tree. Parameter tuning cannot
be validated by evaluator unit tests alone.

### Normal and diagnostic evaluation

`Evaluator` stores an optional `std::function` callback and checks it after each
term even during normal search, where no trace is requested. `EvaluatorDebug`
installs a callback, owns a score tracker, and relies on friendship so its
formatter can inspect evaluator internals. The UCI diagnostic output is human
readable, but there is no stable structured trace suitable for corpus snapshots
or a tuner.

### Tests and measurement

The current 25 evaluation tests provide meaningful coverage of:

- tapered-score arithmetic and piece-square color symmetry;
- side-to-move and null-move perspective;
- pawn structure, mobility, pins, piece features, threats, shelter, king
  danger, phase, scaling, and tapering;
- presence of stable headings in debug output.

However, the 514-line evaluator test uses a production `EvaluatorTest` friend
to call many private methods. Many assertions are exact weighted scores derived
from current constants. Those are useful golden checks for a behavior-preserving
refactor, but they will become maintenance noise if treated as permanent
correctness requirements during tuning. Full-evaluator color/mirror symmetry,
trace-total consistency, and independent recomputation of Board's incremental
material/PSQT state are not directly covered.

The benchmark tooling currently provides:

- a C++ perft benchmark;
- a Python UCI search harness over a six-position default suite drawn from
  Arasan 20, with depth or movetime limits, repeats, manifests, raw logs, and
  baseline/candidate comparisons.

There is no isolated evaluation-throughput benchmark, versioned evaluation
corpus, parameter-independent feature export, or reproducible engine-match
runner with statistical reporting.

## Phase 1: Bounded Organizational Refactor

All findings in this phase must preserve exact scores, trace text unless an
explicit compatibility decision says otherwise, and deterministic fixed-depth
search results.

### ORG-001 — Complete evaluation namespace and file ownership

**Motivation and evidence**

The directory and code namespace disagree. Generic global names such as
`Evaluator`, `TaperedScore`, `EvalTerm`, and `Phase` are owned by evaluation,
while `eval.hpp` is actually a parameter table. Material values are split
between `core/constants.hpp` and thin evaluation wrappers.

**Intended outcome**

- Put HCE-owned types and operations under `namespace eval`.
- Give the parameter repository a descriptive name such as `parameters.hpp`.
- Replace global unscoped evaluation enums with scoped or otherwise
  namespace-owned terms and phases without changing indices or table layout.
- Make external evaluation calls explicit (`eval::evaluate` or the selected
  concrete HCE entry point).
- Keep shared search score types (`EvalValue` and mate sentinels) outside the
  HCE namespace.
- Establish one authoritative material-value definition even though SEE,
  ordering, Board, and evaluation all consume it.

**Likely components**

`src/eval`, material constants in `src/core/constants.hpp`, Board's incremental
score declarations, search and UCI call sites, and evaluation/board/search
tests.

**Dependencies and ordering**

First roadmap change. It should precede file splitting and tuning so later APIs
use final ownership names.

**Tests and measurement**

Run the complete suite, compare a representative position snapshot before and
after, and run identical fixed-depth search benchmarks. Formatting and UCI
diagnostic text should remain unchanged unless separately approved.

**Risks**

Accidentally changing signed piece-square indexing, phase indices, material
authority, formatter specialization names, or the search score scale.

**Completion criteria**

Evaluation-owned declarations are in `eval`; the parameter filename describes
its role; no duplicate material authorities remain; every corpus position and
fixed-depth search result is unchanged.

### ORG-002 — Separate hot evaluation from tracing and formatting

**Motivation and evidence**

The normal search path carries `std::function` callback state and a per-term
branch solely for the local debug command. Trace data and formatters reach
private evaluator state through friendship, and `evaluator.hpp` combines these
concerns.

**Intended outcome**

- Give normal evaluation and diagnostic tracing distinct entry points that
  share evaluation mechanics without imposing type-erased callback overhead on
  search.
- Represent a trace as structured term data with final tapered, scaled,
  side-relative, and tempo-adjusted values.
- Move human-readable formatting outside the hot evaluator implementation.
- Remove formatter access to evaluator internals and retain exact diagnostic
  output for the UCI console command.
- Do not introduce a virtual evaluator hierarchy or an NNUE-shaped abstraction.

**Likely components**

`evaluator.hpp/.cpp`, new trace/report files within `src/eval`, UCI's console
handler, and evaluator/UCI tests.

**Dependencies and ordering**

After ORG-001. The structured trace becomes a prerequisite for INFRA-001 and
later tuning diagnostics.

**Tests and measurement**

Prove normal and traced evaluation return identical values across the corpus;
prove trace terms reconstruct the displayed totals; retain the existing UCI
diagnostic output test; record evaluation throughput to prevent an accidental
regression.

**Risks**

Duplicating evaluation logic between modes, changing term evaluation order, or
letting trace-only code affect the normal path.

**Completion criteria**

Search evaluation has no type-erased diagnostic callback; trace data is a
stable structured value; formatting consumes that value rather than evaluator
internals; values and output remain unchanged.

### ORG-003 — Right-size evaluator implementation and test boundaries

**Motivation and evidence**

Nearly all implementation is in a 744-line header, including non-template
methods and diagnostics. Tests use a broad production friend and one 514-line
test file to reach private feature mechanics.

**Intended outcome**

- Keep only templates or measured hot inline code header-defined.
- Move non-template mechanics and formatting into focused implementation files.
- Organize evaluation tests by responsibility without multiplying tiny cases.
- Replace the broad fixture friendship with structured trace assertions or a
  narrow test-access seam where a private algorithm genuinely needs direct
  testing.
- Classify tests as stable invariants, feature-activation tests, or tunable
  golden scores.

**Likely components**

Evaluator source files, `tests/eval`, CMake, and possibly a focused evaluator
test-access helper.

**Dependencies and ordering**

After ORG-002 so tests can prefer the structured trace. This is the final
behavior-preserving organizational finding.

**Tests and measurement**

Preserve the complete test inventory unless a private-method assertion is
replaced by stronger public/trace behavior. Compare compiled size and evaluation
throughput before moving hot templates out of line.

**Risks**

Over-splitting a cohesive evaluator, losing template specialization/inlining,
or weakening feature coverage merely to remove friendship.

**Completion criteria**

Each file has a clear role; the main header is a navigable interface plus only
justified template definitions; tests no longer depend broadly on all private
state; exact evaluation behavior remains unchanged.

### ORG-004 — Make the Board-owned HCE cache explicit and independently verifiable

**Motivation and evidence**

Board incrementally owns material and piece-square scores and exposes both to
the evaluator. Existing move/history tests compare saved values after unmake,
but there is no independent recomputation that could detect a consistently
wrong cache.

Removing the cache during this bounded pass would either slow every evaluation
or require a new per-worker evaluator lifecycle synchronized with every board
transition. That is larger than an organizational cleanup and too close to
speculative NNUE preparation.

**Intended outcome**

- Retain the Board-owned material/PSQT cache for the HCE lifecycle, but name and
  document it as evaluation-specific incremental state rather than generic
  Board truth.
- Narrow Board's evaluation dependency to the smallest practical parameter and
  score interfaces.
- Provide an independent recomputation path used by tests and diagnostics.
- Verify FEN load, copy, ordinary move, capture, promotion, castling, en
  passant, null move, and complete unmake histories.
- Do not add NNUE accumulators, network ownership, observers, or virtual hooks
  to Board.

**Likely components**

Board representation/mutation, the evaluation base-score/parameter boundary,
Board snapshot support, and Board/evaluation tests.

**Dependencies and ordering**

After ORG-001 establishes ownership; it may run before or after ORG-002 but must
remain a separate focused change.

**Tests and measurement**

Compare incremental state against full recomputation after every transition in
compact table-driven move cases and across an unwindable sequence.

**Risks**

Turning Board into a generic backend host, adding duplicated update paths, or
mistaking a restored-but-wrong cache for correctness.

**Completion criteria**

The current dependency is deliberate, narrow, documented, and independently
validated; no new backend abstraction or score change is introduced.

## Phase 2: Measurement and Baselines

These findings add observability and tooling. They must not intentionally
change playing behavior.

### INFRA-001 — Establish a versioned evaluation corpus and score snapshots

**Motivation and evidence**

Current positions are embedded across unit tests, and there is no common corpus
for exact equivalence, trace inspection, profiling, or tuner validation.

**Intended outcome**

- Add a small, versioned, ID-addressable evaluation corpus covering openings,
  pawn structures, imbalances, mobility/pins, king safety, promotions, quiet
  endgames, and mirrored/color-swapped pairs.
- Add a deterministic tool or test mode that emits final scores and structured
  traces for that corpus.
- Capture a pre-tuning golden snapshot for behavior-preserving refactors and
  performance work.
- Treat golden changes during TUNE/MATH phases as reviewed artifacts, not
  automatic test fixes.

**Likely components**

Test/benchmark data, structured trace support, a small corpus runner, and
evaluation invariant tests.

**Dependencies and ordering**

Requires ORG-002's structured trace. Complete before any strength-changing
work.

**Tests and measurement**

Validate unique IDs, legal FENs, deterministic order/output, full-evaluation
color symmetry for paired positions, and trace-to-final consistency.

**Risks**

Using a small diagnostic corpus as training data, overrepresenting contrived
positions, or freezing chess-policy mistakes as permanent correctness rules.

**Completion criteria**

One command reproduces the same corpus scores/traces; invariants are explicit;
goldens are clearly labeled as baselines rather than eternal expected values.

### INFRA-002 — Add isolated evaluation and downstream search benchmarks

**Motivation and evidence**

The existing C++ benchmark measures perft, while the Python harness measures
whole-search NPS/nodes on six positions. Neither isolates evaluator throughput
or counts evaluation calls.

**Intended outcome**

- Add a repeatable evaluation benchmark over the versioned corpus with warmup,
  multiple samples, a checksum, and machine-readable results.
- Record evaluations/second or nanoseconds/evaluation, distribution summaries,
  compiler/build metadata, and corpus identity.
- Extend optional search instrumentation with evaluation-call counts if needed
  to interpret whole-search effects.
- Continue using the existing fixed-depth UCI suite for node, score, PV,
  bestmove, and NPS comparisons.
- Integrate evaluation results with the existing run-manifest/comparison style
  instead of creating an unrelated benchmark workflow.

**Likely components**

`bench/benchmark.cpp` or a focused evaluation benchmark binary, `bench.py` and
`benchlib`, the corpus, and optional search instrumentation.

**Dependencies and ordering**

Requires INFRA-001. Establish the baseline before rough tuning and before
performance optimization.

**Tests and measurement**

Verify deterministic checksums, schema validation, comparable manifests, and
multiple-run stability. Do not use wall-clock results as unit-test thresholds.

**Risks**

Compiler elimination, measuring parsing/allocation rather than evaluation,
overfitting optimizations to a tiny corpus, or interpreting NPS alone as search
strength.

**Completion criteria**

Baseline and candidate runs can compare isolated evaluation throughput and the
existing downstream search metrics reproducibly.

### INFRA-003 — Establish a reproducible strength-match workflow

**Motivation and evidence**

The repository has no engine-match harness. Search benchmarks expose speed and
tree-shape changes, but they cannot establish Elo improvement.

**Intended outcome**

- Define a reproducible baseline-versus-candidate workflow using a standard UCI
  match runner, a versioned opening suite, fixed time control, threads, hash,
  concurrency, adjudication, and seeds.
- Preserve raw PGNs and report W/D/L plus pentanomial results when paired
  openings are used.
- Distinguish smoke matches from statistically meaningful validation; use
  confidence intervals or SPRT for promotion decisions.
- Record both engine revisions and dirty state.

**Likely components**

Benchmark scripts/configuration, documentation, ignored match artifacts, and
possibly an external match-runner dependency. This need not modify engine code.

**Dependencies and ordering**

Can start after INFRA-001 and must be operational before accepting TUNE-002 or
MATH results as strength improvements.

**Tests and measurement**

Dry-run configuration validation, paired-opening reproducibility, engine crash
reporting, and correct statistical aggregation.

**Risks**

Claiming strength from too few games, biased openings, changing multiple match
settings between runs, or testing on the same positions used for manual tuning.

**Completion criteria**

A documented command produces reproducible paired matches and an auditable
statistical summary for baseline versus candidate.

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

Requires ORG and INFRA-001/002. Correctness changes should precede parameter
tuning and receive independent match/search evaluation.

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
fresh evaluator per call, carrying `std::function` trace machinery in normal
search, repeatedly querying attacks for threat terms, and maintaining several
derived attack/zone arrays. Source inspection alone does not establish which
cost dominates.

**Intended outcome**

- Profile release builds over both isolated evaluation and representative
  search before selecting optimizations.
- Address measured costs one focused change at a time: normal/trace separation,
  repeated attack computation, data layout, branch behavior, construction
  overhead, or carefully justified inlining/code-size changes.
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

## Recommended Execution Sequence

1. **ORG-001** — complete namespace and parameter ownership.
2. **ORG-002** — separate normal evaluation from structured tracing/formatting.
3. **ORG-003** — right-size implementation and test boundaries.
4. **ORG-004** — document, narrow, and independently validate Board's HCE cache.
5. **INFRA-001** — establish the evaluation corpus and golden trace snapshots.
6. **INFRA-002** — add isolated evaluation and downstream search benchmarks.
7. **INFRA-003** — establish reproducible engine matches.
8. **TUNE-001** — audit and correct feature semantics.
9. **TUNE-002** — perform staged human-guided rough tuning.
10. **PERF-001** — profile and optimize the retained HCE with exact equivalence.
11. **MATH-001** — export raw features and build leakage-resistant datasets.
12. **MATH-002** — tune linear parameters with held-out validation.
13. **MATH-003** — tune nonlinear and search-coupled parameters separately.
14. Create a separate NNUE roadmap only after the entry criteria are met.

### Change contract by phase

| Findings | Evaluation values | Search behavior | Required evidence |
|---|---|---|---|
| ORG-001 through ORG-004 | Exact preservation | Exact preservation at fixed depth | Corpus checksum, full tests, fixed-depth comparison |
| INFRA-001 through INFRA-003 | No intentional change | No intentional change | Tool/schema tests and reproducibility |
| TUNE-001 correctness fixes | Intentional only for confirmed defects | May change | Direct regression, corpus diff, search/match checks |
| TUNE-002 | Intentionally changes | Intentionally may change | Trace rationale, corpus diff, fixed-depth suite, matches |
| PERF-001 | Exact preservation | Exact preservation at fixed depth | Score checksum, profiler, throughput and search benchmark |
| MATH-001 | No normal-path change | No normal-path change | Feature reconstruction and dataset validation |
| MATH-002 and MATH-003 | Intentionally changes | Intentionally may change | Held-out metrics, coefficient sanity, statistical matches |

