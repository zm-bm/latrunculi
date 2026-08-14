# Evaluation and Endgame Roadmap

## Purpose

This roadmap covers the remaining work on Latrunculi's handcrafted evaluation,
endgame handling, and possible future NNUE evaluation. It deliberately
separates four concerns:

- chess-rule results that search must recognize exactly;
- parameterized handcrafted evaluation and automated tuning;
- optional endgame knowledge and tablebase integration; and
- a future NNUE backend.

These concerns interact, but they do not have the same owner or validation
contract. In particular, an exact draw rule is not an evaluation heuristic,
and a tablebase probe is not an evaluation term.

## Completed Foundation

The preparatory HCE work is complete:

- all evaluation code is owned by `namespace eval` behind the public
  `eval::evaluate()` and `eval::evaluate_trace()` boundary;
- material and every HCE parameter have one authoritative owner;
- `Board` explicitly maintains and independently verifies its incremental
  material and piece-square base terms;
- normal and traced evaluation share mechanics without adding tracing overhead
  to search;
- feature, boundary, color-symmetry, make/unmake, and trace-consistency tests
  protect the current implementation;
- a fixed embedded evaluation workload supplies an order-sensitive behavior
  checksum and isolated throughput measurement;
- a fixed-depth component measurement compares integrated search independently
  from evaluator throughput; and
- a bounded rough pass corrected feature semantics, added passed-pawn scoring,
  reviewed all major parameter groups, and profiled the retained evaluator.

The current HCE already covers the principal general-purpose feature classes:
material, PSQTs, tapered MG/EG scoring, common pawn defects and passed pawns,
mobility, outposts, bishop and rook features, weak-piece threats, king shelter
and danger, phase, endgame scaling, and tempo. More features should be added
only in response to measured representational gaps, not from a checklist of
traditional chess knowledge.

## Immediate Rules Work

### RULE-001 — Recognize basic dead-by-material positions

**Motivation and current evidence**

`Board::is_draw()` currently recognizes the fifty-move rule and repetition.
It does not recognize positions whose material makes checkmate impossible, so
search can assign a positional or tempo score to an exact draw. This is a
rules/search result, not a tunable HCE feature. Allowing such positions into a
WDL tuning dataset could also push material and PSQT weights to compensate for
a missing terminal rule.

**Intended outcome**

- Add a conservative, demonstrably correct dead-by-material predicate.
- Integrate it into normal and quiescence draw detection.
- Cover at least the elementary king-only and lone-minor cases.
- Distinguish positions where mate cannot be forced from positions where no
  legal sequence can produce mate; do not classify material such as K+NN vs K
  as automatically drawn merely because mate cannot be forced.
- Independently settle promoted-bishop and bishop-square-color cases rather
  than relying on piece counts alone.
- Keep repetition, fifty-move, stalemate, and tablebase outcomes as separate
  concepts with their existing ownership.

**Likely components**

Board material queries and rules, search draw exits, focused Board/search
tests, and tuning-dataset filtering. No evaluation parameter should change.

**Testing and completion**

Use legal positive and negative material cases, verify both main and
quiescence search return the draw score, and preserve repetition and
fifty-move behavior. Completion requires a documented exact material contract
and no false draw classification for merely difficult-to-win endings.

## Benchmarking and Mathematical HCE Tuning

### Repository boundary

Keep Latrunculi's benchmark and tuning implementation in a distinct top-level
`bench/` directory. It owns the deterministic engine-benchmark workload,
OpenBench integration documentation and configuration, the optional C++ raw-
feature exporter, Python dataset construction and optimization code,
reproducible configurations, and small test fixtures. Keep generated games,
feature matrices, datasets, candidate artifacts, and reports in an ignored
`bench/workspace/` subtree.

The separately built `latrunculi-measure` executable remains responsible only
for local component performance measurement. Run a pinned, self-hosted
[OpenBench](https://github.com/AndyGrant/OpenBench) checkout as a separate
service. Keep normal engine builds and runtime code independent of benchmark
and tuning tools and their Python dependencies.

### BENCH-001 — Establish the deterministic engine benchmark and OpenBench integration

**Intended outcome**

- Add `latrunculi bench` with a fixed low-depth position suite, deterministic
  aggregate node count, elapsed time, and final nodes-per-second output in the
  form required by
  [OpenBench](https://github.com/AndyGrant/OpenBench/wiki/Requirements-For-Public-Engines).
- Add the minimal OpenBench-compatible build entry point while retaining CMake
  as the project's normal build system.
- Document and version Latrunculi's OpenBench engine configuration, opening
  suite identity, test modes, time controls, Hash, thread count, adjudication,
  and SPRT defaults under `bench/`.
- Pin and operate a private OpenBench server and worker on the dedicated
  workstation, beginning with fixed-game smoke tests and a short SPRT.
- Keep OpenBench responsible for engine builds, worker coordination, paired
  color-reversed games, Fastchess execution, PGNs, and test results.

**Testing and completion**

Repeated `latrunculi bench` runs must produce the same aggregate node count.
OpenBench must build both revisions, normalize worker speed from the benchmark,
and complete paired fixed-game and SPRT smoke tests. Record the OpenBench test
IDs, engine revisions, benchmark node count, configuration revision, and final
decision.

### Strength-validation policy

Use the self-hosted OpenBench instance for all retained strength claims. Run
paired, color-reversed SPRT games against the pre-change revision using the
pinned opening suite, one thread per engine, 32 MB Hash, and the versioned time-
control and adjudication settings. Use normalized-Elo bounds `[0, 5]` with
`alpha = beta = 0.05` to screen candidates, then `[0, 3]` with the same error
rates to confirm a retained batch. Record the OpenBench test ID, both engine
revisions, configuration revision, SPRT decision, and relevant PGN location.

### MATH-001 — Export parameter-independent features and construct datasets

**Motivation and current evidence**

The existing trace records weighted term contributions. It cannot recover the
raw sparse coefficients needed to tune individual material, PSQT, mobility,
pawn, and piece-feature parameters. The embedded 24-position evaluation
workload is a throughput input and compact behavior fingerprint, not a training
dataset.

**Intended outcome**

- Define a deterministic, versioned feature schema for the linear portion of
  the HCE.
- Export raw MG/EG coefficients, phase and scaling context, side to move, game
  result, and source identity without changing normal search evaluation.
- Prove that the supported weighted terms reconstruct the production
  evaluator before fitting any parameters.
- Build a large, deduplicated WDL-labeled position dataset.
- Split by game—not by individual position—into training, validation, and
  untouched held-out sets.
- Filter checkmates, stalemates, recognized dead positions, malformed games,
  and other positions for which a static-evaluation label is inappropriate.
- Record source, sampling interval, quiet-position policy, balancing, seeds,
  schema version, and all filtering decisions.
- Keep the embedded measurement workload separate from tuning data.
- Report phase and material distributions and preserve a dedicated endgame
  validation slice so middlegames cannot hide endgame errors.

**Scope boundary**

Do not tune weights, change normal evaluation, add features, implement a
generic machine-learning framework, or commit large generated datasets without
an explicit storage policy. Keep the versioned exporter, schema, construction
code, configurations, and compact manifests in `bench/`; keep large generated
data in `bench/workspace/`.

**Testing and completion**

Require deterministic export, exact reconstruction for supported terms,
schema validation, duplicate/leak detection, game-level split validation, and
dataset distribution reports. MATH-001 must leave production evaluations,
fixed-depth search, and matches unchanged.

### MATH-002 — Tune the existing linear HCE reproducibly

**Motivation**

Material, PSQTs, mobility tables, passed-pawn ranks, and many positional
bonuses are linear or can be represented as phase-coupled linear features.
Their current values are coherent but largely hand selected.

**Intended outcome**

- Fit the evaluation-to-WDL logistic scale explicitly.
- Tune selected coherent parameter groups with deterministic configuration and
  seeds.
- Keep the middlegame pawn fixed at 100 centipawns as the score-scale anchor.
- Apply bounds, regularization, symmetry constraints, or staged groups to
  control correlated features and implausible coefficients.
- Report training, validation, held-out, and endgame-slice loss separately.
- Produce a reviewable parameter artifact or patch rather than mutating source
  constants opaquely.
- Validate candidates through focused evaluator tests, the evaluation checksum,
  fixed-depth search, and the canonical OpenBench SPRT workflow; lower
  prediction loss alone is not sufficient strength evidence.

**Testing and completion**

The tuner must reproduce results from its recorded inputs. Retained parameters
must improve held-out prediction loss, remain structurally plausible, preserve
evaluation invariants, and have credible candidate-versus-baseline match
evidence.

### MATH-003 — Tune nonlinear and search-coupled behavior separately

**Motivation**

King danger is nonlinear, while phase, generic and specialized endgame
scaling, tempo, and search margins do not behave as independent linear feature
weights. Combining them with the first linear fit would make the result harder
to understand and validate.

**Intended outcome**

- Keep king-danger conversion, phase, scaling, and tempo out of the initial
  linear fit.
- Use an appropriate constrained method for each nonlinear group.
- Tune aspiration, razoring, futility, SEE-dependent policy, and other search
  thresholds separately from HCE weights even when they share score units.
- Require deterministic configurations and OpenBench SPRT evidence for retained
  changes.

**Testing and completion**

Protect formula boundaries and monotonicity where intended. Report held-out
prediction effects when meaningful, fixed-depth search behavior, and paired
match evidence. Do not accept a nonlinear or search-policy change solely from
training loss.

## Evidence-Driven Endgame Knowledge

### END-001 — Audit endgame residuals after linear tuning

**Motivation**

Automated tuning can choose weights for existing features but cannot invent a
missing chess relationship. Conversely, adding traditional endgame rules
before measuring the tuned evaluator expands the model and parameter space
without evidence that the added complexity solves a current weakness.

**Intended outcome**

- Analyze the tuned evaluator's largest and most systematic errors on the
  held-out endgame slice.
- Compare score sign, calibration, and conversion behavior with exact
  tablebase WDL/DTZ results where available.
- Separate failures caused by a missing general feature from exact material
  recognizers, draw scaling, search horizon, and tablebase-covered play.
- Create focused candidate findings only for repeated, explainable failures.

**Testing and completion**

Produce a compact evidence report organized by material class and failure
type. Completion does not require a production change; it requires a justified
decision about which, if any, endgame mechanisms deserve their own plans.

### END-002 — Add only justified endgame mechanisms

Possible findings from END-001 may include:

- opposite-colored-bishop or other material-specific draw scaling;
- wrong-bishop rook-pawn recognition;
- king distance or rule-of-the-square context for passed pawns;
- mop-up guidance in won bare-king endings;
- KPK or another small exact bitbase; or
- rook/pass-pawn and other conversion-oriented relationships.

This list is illustrative, not a checklist. Each retained mechanism must have
one precise chess meaning, activation and counterexample tests, held-out or
tablebase evidence, reviewed evaluation and search changes, and a paired match
against the then-current tuned baseline. Retrain affected linear parameters
after adding a new feature.

The broader set of established endgame techniques is summarized by the
[Chessprogramming Wiki endgame overview](https://chessprogramming.org/Endgame).

## Related Engine Capability

### TB-001 — Add optional Syzygy tablebase support

**Ownership and independence**

Tablebases are exact external oracles used by root handling and search. They
are not HCE terms and are not a prerequisite for tuning positions outside
their coverage. This work may proceed independently after RULE-001, although
END-001 can use an offline tablebase tool without waiting for production
integration.

**Intended outcome**

- Add optional Syzygy WDL and DTZ probing with no bundled tablebase files.
- Define UCI configuration, unavailable/invalid-path behavior, supported piece
  counts, castling/en-passant restrictions, probe depth, and thread safety.
- Distinguish interior-node WDL use from root DTZ move selection.
- Respect the fifty-move counter and preserve correct mate/stalemate/draw
  precedence.
- Measure probe hit rate and search cost; disk access and decompression must not
  silently reduce overall strength.
- Keep ordinary search fully functional and reproducible when no tablebase path
  is configured.

**Testing and completion**

Use a tiny externally supplied test fixture or mockable probing boundary rather
than committing large tables. Cover win/draw/loss, root move choice, the
fifty-move boundary, unavailable files, unsupported positions, multiple
workers, and UCI option behavior. Run matches both with tablebases disabled and
with identical tablebase access for both engines.

Tablebase formats and the WDL/DTZ tradeoffs are surveyed in the
[Chessprogramming Wiki tablebase overview](https://chessprogramming.org/Endgame_Tablebases).

## Deferred Evaluation Backend

### NNUE — Create a separate implementation roadmap

NNUE remains a future evaluation backend rather than another task inside the
HCE tuning plan. Create its detailed roadmap only after the mathematical HCE
pipeline is operational and its lessons about data, validation, and match
testing are understood.

The later design must respect these constraints:

- immutable network weights are shared above individual workers;
- each search worker owns mutable accumulator stacks and refresh state;
- accumulator push/pop remains coherent with all make/unmake operations,
  promotions, castling, en passant, null moves, root installation, and Board
  copies;
- a scalar full-refresh implementation establishes correctness before
  incremental updates or SIMD;
- the HCE remains available as a readable reference and diagnostic comparator;
  and
- network loading, UCI options, fallback behavior, distribution, and licensing
  are explicit integration decisions.

## Recommended Execution Sequence

1. **RULE-001** — basic dead-by-material recognition.
2. **BENCH-001** — deterministic engine benchmark and self-hosted OpenBench.
3. **MATH-001** — raw feature export and reproducible datasets.
4. **MATH-002** — linear HCE tuning and OpenBench SPRT validation.
5. **END-001** — tablebase-backed analysis of remaining endgame errors.
6. **END-002** — only the endgame mechanisms justified by that analysis.
7. **MATH-003** — nonlinear evaluation and search-coupled tuning.
8. **TB-001** — optional production Syzygy integration; it may run in parallel
   after RULE-001 and need not delay MATH-001 or MATH-002.
9. Create a separate NNUE roadmap.

### Change Contract

| Finding | Evaluation values | Search behavior | Required evidence |
|---|---|---|---|
| RULE-001 | Unchanged outside exact draws | Intentional draw correction | Rules cases and main/qsearch integration |
| BENCH-001 | Exact preservation | Exact preservation | Stable node count, OpenBench build and paired-game smoke tests |
| MATH-001 | Exact preservation | Exact preservation | Feature reconstruction and dataset validation |
| MATH-002 | Intentionally changes | Intentionally may change | Held-out loss, coefficient sanity, evaluation checksum, search and matches |
| END-001 | No production change | No production change | Held-out and tablebase-backed analysis |
| END-002 | Intentionally may change | Intentionally may change | Activation tests, exact/endgame evidence, retraining and matches |
| MATH-003 | Intentionally changes | Intentionally may change | Formula tests, reproducible optimization and matches |
| TB-001 | HCE unchanged | Exact tablebase outcomes in covered nodes | Probe correctness, performance and equal-access matches |
| NNUE | New evaluation backend | Intentionally changes | Separate roadmap and staged equivalence/strength validation |
