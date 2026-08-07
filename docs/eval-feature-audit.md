# Handcrafted Evaluation Feature Audit

## Scope, status, and method

This document records the completed source-audit portion of **TUNE-001** and is
the active workspace for **TUNE-002**, **SCALE-002**, **PERF-001**, and
**PERF-002**. The audit was performed at `f243585`, before feature correction
or tuning; remediation status is current through `d9a7f7a`. It covers
`src/eval`, Board's incremental base terms, evaluation consumers in
Board/search/UCI, evaluation and related tests, the checked-in diagnostic
corpus and snapshot, `docs/search-stability.md`, relevant history, and selected
reference engines under `/home/rick/code/chess-engine-refs`.

The starting tree was coherent:

- the release `tests`, `benchmark`, and `latrunculi` targets built;
- the focused evaluation, Board, and search tests passed;
- the complete release suite passed; and
- `python3 bench/bench.py eval verify` reproduced
  `bench/eval/baseline.tsv` exactly.

Small paired positions and structured traces were used to distinguish feature
activation from parameter quality. Findings use exactly these classifications:

- **Confirmed correctness defect** — implementation contradicts its own stated
  semantics or a required invariant;
- **Test gap** — credible intended behavior lacks focused protection;
- **Policy decision** — the implementation is coherent, but more than one
  defensible chess meaning exists;
- **TUNE-002 candidate** — mechanics are coherent and the remaining question
  concerns weights or subjective chess policy; and
- **No action** — behavior is coherent and adequately covered.

Reference engines are supporting evidence, not specifications. The current
Stockfish reference is NNUE-based and is not a direct HCE feature model.
Ethereal and Minic have substantially broader HCEs, different attack-map and
scaling structures, and different tuning histories. CPW is a smaller HCE with
different search/evaluation coupling. Their implementations identify common
design choices, but do not make a different Latrunculi choice incorrect.

The initial audit changed no production code, tests, parameters, corpus rows,
or snapshot values. Its confirmed defects and test gaps were subsequently
handled in focused commits recorded below. TUNE-001 is now complete; this
document retains its evidence while tracking the bounded rough-tuning and
performance work defined at a higher level in
[`eval-roadmap.md`](eval-roadmap.md).

## Dependency and execution-order map

```text
eval::parameters
  |-- material values ------------+--> Board::non_pawn_material()
  |                               +--> Board::see()
  |                               +--> search::ordering::Picker
  |                               +--> null-move material guard
  |-- piece-square values ----+
  |-- feature weights         |
  v                           v
eval::BaseTerms <------ Board make/unmake/FEN/copy
  |  material + PSQT, incrementally maintained
  v
eval::Evaluator (fresh, single-use object per call)
  |
  |  1. seed king attacks; construct outpost, mobility, and king zones
  |  2. material and PSQT base terms
  |  3. pawns: structure + pawn attack maps
  |  4. knights, bishops, rooks, queens:
  |       pin-restricted moves, attack maps, mobility, threats, king attackers,
  |       then piece-specific terms
  |  5. king safety (shelter - accumulated danger)
  |  6. accumulated mobility and threats
  |  7. stronger-side endgame scaling
  |  8. material-phase tapering
  |  9. side-to-move conversion + tempo
  v
eval::evaluate() --------> alpha-beta static eval, quiescence, root fallback
eval::evaluate_trace() --> UCI `eval`, corpus snapshots, feature diagnostics
```

The apparent term list is not order-independent. Pawn and piece evaluation
populate attack, double-attack, mobility, threat, and king-attacker state used
by later terms. Any future feature change must preserve or deliberately revise
that dependency order.

## 1. Material, phase, tapering, endgame scaling, and tempo

### Current semantics

- `src/eval/parameters.hpp` is the sole authority for material and feature
  parameters. Material is `(MG, EG)` pawn 100/166, knight 630/680, bishop
  660/740, rook 1000/1100, and queen 2000/2150.
- `eval::BaseTerms` in `src/eval/base_terms.hpp` stores signed material and PSQT
  contributions. `Board` updates both on add/remove/move and independently
  recomputes them for tests.
- `Evaluator::phase()` in `src/eval/evaluation.cpp` sums both sides' MG
  non-pawn material, clamps it to `[2500, 10000]`, and maps it to `[0, 128]`.
- Only the endgame component is scaled. The stronger side is selected by the
  sign of the unscaled EG score; its pawn count gives
  `min(64, 36 + 5 * pawns) / 64`.
- MG/EG interpolation is integer arithmetic. The white-relative result is
  converted to the side-to-move perspective and receives a +20 tempo bonus.
- Commit `cbd96fd` deliberately fixed scale-factor ownership to the stronger
  side, and commit `641851c` established the current tempo/perspective rule.

### MAT-001 — Base-score and transformation mechanics

**Classification:** No action. **Confidence:** high.

The signed material/PSQT convention, incremental Board cache, stronger-side
scaling, phase clamp, taper, perspective conversion, and tempo pipeline are
internally consistent. Existing Board representation/history tests protect
FEN load, copy, ordinary moves, captures, promotions, castling, en passant,
null moves, and complete unmake histories. `EvaluatorTest` protects side-to-
move and null-move perspective; `EvaluationFeaturesTest` covers phase, scale,
and taper mechanics; `Trace` checks reconstruct the unscaled total. No paired
position exposed a contradiction.

The `Trace` formatter's use of the MG pawn value as a fixed display unit for
both columns is a centipawn-style presentation convention, not evidence that
the EG pawn value is ignored by evaluation.

### MAT-002 — Material, phase, scale, and tempo values

**Classification:** TUNE-002 candidate. **Confidence:** high.

- **Evidence:** the values in `src/eval/parameters.hpp` are hand-selected and
  have no mathematical tuning provenance. The generic pawn-count scaling has
  no specialized insufficient-material, opposite-bishop, or pawn-ending
  rules. Ethereal and Minic use materially different phase/scaling systems.
- **Intended semantic behavior:** material establishes the score unit; phase
  blends coherent MG/EG estimates; scaling reduces conversion confidence for
  drawish endings; tempo represents the initiative rather than material.
- **Representative positions:** the existing `kings_only`, start position,
  one-rook phase cases, and `endgame-arasan-16/21` corpus entries already span
  the formula. Add targeted positions only when a particular rule is proposed.
- **Trace and consumers:** Material, Squares, scaled/unscaled stages, and every
  final value are affected. Material also reaches SEE, noisy ordering, phase,
  and null-move eligibility.
- **Coverage:** current tests are sufficient to protect mechanics but their
  exact numbers must be intentionally updated during tuning.
- **Impact:** potentially broad tree, pruning, ordering, and strength changes.
- **Dependencies/risks:** follows the single-authority policy recorded in
  SCALE-001 and must not be used to conceal the TT/LMR instability documented in
  `docs/search-stability.md`.
- **Completion:** treat material/score scale, phase/endgame scaling, and tempo
  as separate measured TUNE-002 experiments, with snapshot review, fixed-depth
  search, and paired matches.
- **Future plan:** yes, but only as staged parameter groups; do not combine all
  four concerns in one commit.

### SCALE-001 — Keep one authoritative material table

**Classification:** Policy decision — resolved. **Confidence:** high.

`eval::piece()` remains the single material authority for evaluation, phase,
SEE, capture ordering, and material-based search eligibility. This keeps the
parameter set small and prevents competing definitions of piece value. Every
future material change must therefore audit all of those consumers, not present
itself as an evaluator-only change. Search margins remain separate and are
revisited under SCALE-002 after broad score changes. No code change is required;
a consumer should be decoupled only if later measurement demonstrates an
irreconcilable requirement.

## 2. Pawn structure and pawn attack state

### Current semantics

`Evaluator::evaluate_pawns()` in `src/eval/evaluator_detail.hpp` runs once per
color before pieces. It builds pawn attacks and double attacks, then applies:

- isolated: no friendly pawn exists on either adjacent file;
- backward: the pawn's stop square is enemy-pawn-attacked and lies outside the
  friendly pawn attack span; and
- doubled: a pawn has a friendly pawn behind it and is not pawn-supported.

These terms may overlap. There is no passed, candidate, connected, protected,
or pawn-mobility term. Pawn attacks then constrain outposts, mobility zones,
king shelter, safe checks, weak squares, and double-attack state.

### PAWN-001 — Existing pawn masks and color transforms

**Classification:** No action. **Confidence:** high.

The isolated, backward, and doubled masks match their comments, use color-
relative shifts, and have white/black activation cases in
`tests/eval/features.test.cpp`. The isolated+doubled stack is explicitly
covered. Pawn attack maps are produced before every dependent term, and trace
records show the expected signed color contribution. No A/H-file wrap or
color-direction defect was found.

### PAWN-002 — Whether structural defects should overlap

**Classification:** Policy decision — implemented and verified. **Confidence:** high.

Use a constrained taxonomy: an isolated pawn is not also scored as backward,
while the doubled-pawn penalty remains independent and may stack with either
classification. Isolated and backward largely express the same lack of
neighboring-pawn support; excluding their overlap avoids double-pricing that
cause and reduces parameter correlation. Doubling is a distinct structural
liability and remains additive.

The implementation remains branchless and bitboard-based: it removes isolated
pawns from the derived backward mask before counting. The existing pawn test
table now protects isolated/backward exclusion alongside backward-only and
isolated+doubled behavior. `endgame-arasan-16` contained one corpus overlap, so
its Black Pawn trace changed from -20/-55 to -10/-30 and the reviewed golden was
updated. The full suite and snapshot verification pass, all six depth-five best
moves remained unchanged, and isolated throughput showed no regression. This
is a semantic change, not a performance feature; PERF-002 remains the meaningful
later pawn-evaluation speed opportunity.

### PAWN-003 — Passed-pawn coverage

**Classification:** TUNE-002 candidate. **Confidence:** high.

- **Evidence:** no active passed-pawn feature exists, even though the corpus has
  `promotion-race` and history commit `5325c02` once added passed-pawn helpers
  for an older scaling design. Ethereal, Minic, and CPW all model passers, but
  with very different mechanics.
- **Intended semantic behavior:** recognize an advanced pawn with no opposing
  pawn in its forward/file attack span, then value rank, support, blockers,
  king distance, or rook relation only if individually justified.
- **Representative positions:** `promotion-race` plus a paired blocked/nonpassed
  version is the natural starting point. Do not infer a complete feature from
  PSQT gains alone.
- **Trace and consumers:** likely `Term::Pawns`; adding derived pawn state also
  affects the later PERF-002 pawn-cache design.
- **Coverage:** current absence is visible in snapshots but is not a defect and
  needs no regression test until semantics are selected.
- **Impact:** potentially large endgame strength and score-shape changes.
- **Dependencies/risks:** implement before final pawn weights and before the
  pawn hash; avoid restoring old Board APIs without revalidation.
- **Completion:** one bounded feature definition, paired activation/boundary
  tests, corpus review, fixed-depth endgame searches, and matches.
- **Future plan:** yes, a standalone TUNE-002 feature plan.

Horizontal file symmetry is not currently a universal pawn-evaluation
invariant because the PSQTs are file-asymmetric. Pawn-structure masks themselves
are file symmetric; CROSS-002 records the separate PSQT policy.

## 3. Knights, bishops, mobility, pins, and outposts

### Current semantics

The evaluator creates a mobility area excluding enemy-pawn attacks, the own
king square, and own pawns on their home rank. Piece attacks use actual
occupancy; a piece in `Board::blockers(color)` is restricted to its king/piece
collinear ray. Mobility uses those restricted moves. Outpost squares are on
relative ranks 4–6, cannot lie in the enemy pawn attack span, and must already
be supported by a friendly pawn. Knights receive an occupied or reachable
outpost bonus; bishops only an occupied bonus. Both minors receive a bonus for
standing directly behind a friendly pawn.

Bishops additionally receive long-diagonal, same-color pawn-blocker, and pair
terms. Long diagonals are evaluated through own pawns as the only occupancy.
The blocker penalty depends on same-color friendly pawn count, blocked central
pawns, and whether a pawn supports the bishop.

### MOB-001 — Mobility-area and supported-outpost definitions

**Classification:** TUNE-002 candidate. **Confidence:** high that mechanics
match comments; medium on stronger semantics.

- **Evidence:** `mobility_zone()` and `outposts_zone()` in
  `src/eval/evaluator_detail.hpp` implement their documented masks exactly.
  Ethereal excludes all rammed/blocked own pawns from mobility and distinguishes
  supported from unsupported outposts; Latrunculi excludes home-rank pawns and
  defines every outpost as supported.
- **Intended semantic behavior:** mobility should approximate useful safe
  activity rather than legal move count; an outpost should represent stable
  occupation that enemy pawns cannot challenge.
- **Paired positions:** use the existing mobility-restriction and knight-outpost
  cases, then vary a blocked pawn off the home rank or remove the supporting
  pawn without changing the enemy pawn span.
- **Trace and consumers:** piece terms change for occupied/reachable outposts;
  `Term::Mobility` changes for zone membership; attack maps and king danger do
  not use the mobility mask.
- **Coverage:** existing geometry and pin tests are strong, but deliberately
  encode only the current definition.
- **Impact:** broad middlegame score changes and possible opening-shape changes.
- **Dependencies/risks:** do not alter mobility zones and mobility curves in one
  experiment. Do not use an opening preference alone as proof of correctness.
- **Completion:** select one definition, use activation pairs, review snapshot
  deltas, and match it before tuning its table.
- **Future plan:** yes, after correctness/test-gap work; separate zone semantics
  from mobility weights.

### PIECE-001 — Meaning of “bishop pair” after promotion

**Classification:** Policy decision — resolved. **Confidence:** high.

Retain the existing `Board::count(C, BISHOP) > 1` rule: any two bishops earn
one `bishop_pair` bonus. The count is already cached and avoids extra
square-color inspection in the evaluation hot path. The semantic difference
arises only for same-colored bishops created by promotion, an exceedingly rare
case that does not justify additional work or tuning complexity in the current
HCE. Ordinary pair activation remains covered; no same-color promotion test or
code change is required unless later evidence motivates stricter semantics.

### PIECE-002 — Pin restriction and current minor/bishop activation

**Classification:** No action. **Confidence:** high.

Pin-restricted moves remain on the king/piece ray, rather than suppressing all
movement. `PinnedPieceMobilityStaysOnPinRay` directly protects that boundary.
Outpost, behind-pawn, long-diagonal, bishop-blocker, and ordinary bishop-pair
activation have focused white/black trace tests. X-ray attacks used for bishop
long-diagonal and king-zone pressure deliberately use a different occupancy
from legal mobility; that separation is coherent and should remain explicit.

The exact bonuses, blocker multiplier, and mobility curves remain subjective
TUNE-002 inputs under MOB-001; their unusual values are not correctness
defects.

## 4. Rooks and queens

### Current semantics

Rooks receive a semi-open bonus when their file has no friendly pawn and a
larger fully-open bonus when it has no pawn of either color. Otherwise they
receive a closed-file penalty if any friendly pawn on the file has its forward
square occupied. Queens receive a penalty described as “discovered attack” if
an enemy bishop or rook lies on the queen's empty-board ray with exactly one
occupied intervening square.

### ROOK-001 — Open and blocked file mechanics

**Classification:** No action. **Confidence:** high.

The implementation matches its comments, is color-relative, and has focused
open, semi-open, and blocked-file tests. Whether rank, seventh-rank, behind-
passer, or nearest-pawn context should be added is ordinary feature expansion,
not a defect in the retained term. Reconsider those only during TUNE-002 after
passed-pawn semantics are settled.

### QUEEN-001 — Correct queen discovery detection

**Classification:** Confirmed correctness defect — completed in `ca8699e`.

Discovery detection now requires exactly one intervening piece. A direct
bishop/rook attack no longer activates `queen_discover_att`, while existing
one-blocker activation and two-blocker rejection remain protected. The
evaluation snapshot stayed byte-identical; the reviewed depth-five comparison
showed only intentional score/tree consequences of removing the erroneous
penalty.

## 5. Threats and cross-piece interactions

### Current semantics

For each knight, bishop, rook, or queen, `update_threats()` asks
`Board::attacks_to()` for geometric attackers and defenders of the occupied
square. If attacker count exceeds defender count, the owning color receives a
piece-type weak penalty. Pawns and kings can participate in the counts, but
pawns and kings are not themselves scored as weak pieces. Attacker value, SEE,
capture legality, and equal-count value imbalance are not considered.

`Board::attacks_to()` intentionally includes pinned pieces; its contract and
`BoardRulesTest.GeometricAttacksIncludePinnedPieces` make that behavior
explicit. Piece mobility and king attack maps, by contrast, use the evaluator's
pin-restricted moves.

### THREAT-001 — Geometric weak-piece pressure

**Classification:** Policy decision — resolved. **Confidence:** high.

Retain the existing geometric attacker/defender counts, including pinned
pieces. This rule is used only by the coarse `Term::Threats` weak-piece
penalty; mobility already restricts pins independently. Refining pins in
isolation would polish one edge of a model that still ignores attacker value,
capture sequence, and SEE. Reconsider cached ray-aware filtering only as part
of THREAT-002's broader threat-model work, without changing the geometric
contract of `Board::attacks_to()`. No code change or additional pinned-piece
test is required now.

### THREAT-002 — Count-only weak-piece model

**Classification:** TUNE-002 candidate. **Confidence:** high.

- **Evidence:** `update_threats()` compares only counts and indexes one penalty
  by victim type. It does not distinguish pawn/minor/rook attackers, hanging
  pieces, safe pawn pushes, or SEE. Ethereal and Minic use richer attacker- and
  victim-aware terms, but with different evaluation architectures.
- **Intended semantic behavior:** penalize pieces whose tactical support is
  inadequate without duplicating full search or SEE.
- **Representative pairs:** the existing outnumbered-knight cases are a base;
  vary attacker value and equal attacker/defender counts while keeping the
  victim square fixed.
- **Trace and consumers:** `Term::Threats`, with possible interaction with the
  queen discovery feature and king weak-square maps.
- **Coverage:** current activation is protected; richer semantics are absent,
  not broken.
- **Impact:** potentially broad tactical ordering and pruning changes through
  static scores.
- **Dependencies/risks:** if revisiting THREAT-001, add one distinction at a
  time and avoid embedding a second SEE implementation in HCE.
- **Completion:** retain only distinctions supported by trace examples and
  match evidence.
- **Future plan:** yes, later in TUNE-002, after simpler positional groups.

## 6. King shelter and danger

### Current semantics

The shelter term examines the king's three adjacent files. It selects the
nearest safe friendly pawn ahead, nearest enemy pawn storm, blocked-storm
state, king-file bonus, and friendly/enemy open-file state. A friendly shelter
pawn currently attacked by an enemy pawn is excluded. If castling rights
exist, `evaluate_king_safety()` compares the actual king's shelter with the
corresponding c/g-file destination and chooses the largest MG shelter; danger
is always calculated around the actual king.

Piece evaluation accumulates king-zone attackers, attack weights, x-rays,
single/double attacks, and check squares. Raw danger combines check danger,
attacker weight times count, weak king-zone squares, and pinned friendly
pieces. Safe checks take precedence over unsafe checks for each piece type.
Check multiplicity scales toward twice a single-check value. Final danger is
quadratic in MG (`danger^2 / 2048`) and linear in EG (`danger / 8`).

### KING-001 — Protect castling-right shelter selection

**Classification:** Test gap — completed in `04f31ed`.

The existing King-safety test now compares identical positions with and without
White kingside castling rights. It verifies relationally that the better g1
shelter replaces the e1 shelter while danger remains based on the actual king,
without freezing another absolute tuned score.

### KING-002 — Shelter/check/danger weights and nonlinear conversion

**Classification:** TUNE-002 candidate. **Confidence:** high.

- **Evidence:** `src/eval/parameters.hpp` contains large hand-selected shelter,
  storm, check, weak-zone, pin, and attacker weights. History commits
  `a102bfe`, `ad499ee`, and `f020012` evolved the model, but no fitted baseline
  establishes the current curve. The tests mostly assert exact raw sums.
- **Intended semantic behavior:** danger should increase with genuinely safer
  checks, more coordinated attackers, weak king-zone squares, and exposed
  shelter; nonlinear conversion should avoid overwhelming ordinary eval until
  attacks are coordinated.
- **Representative positions:** existing `king-shelter`, `king-pressure`,
  `RawDanger`, and safe/unsafe check cases are suitable activation anchors.
  Add monotonic pairs only for a specific experiment.
- **Trace and consumers:** `Term::King`; the nonlinear term can dominate final
  MG scores and therefore pruning/search trees.
- **Coverage:** formula activation is good; exact-value tests should be updated
  deliberately during tuning and supplemented by invariant/monotonic checks
  where they remain valid.
- **Impact:** high strength and volatility risk.
- **Dependencies/risks:** tune after material, PSQT, pawn/piece, mobility, and
  threat groups; avoid simultaneous shelter semantics and danger-weight
  changes. Nonlinear parameters belong outside the first linear Texel pass.
- **Completion:** staged experiments with trace rationale, corpus review,
  fixed-depth search, and statistically meaningful matches.
- **Future plan:** yes, last in TUNE-002 or later MATH-003.

### KING-003 — Attack accumulation and danger execution order

**Classification:** No action. **Confidence:** high.

Pawn and pin-restricted piece attacks are accumulated before king danger;
double attacks are merged correctly; x-rays are deliberately separate from
legal mobility; pinned-piece danger intersects blockers with the owning
color's pieces. Existing raw-danger tests cover no danger, unsafe rook checks,
safe queen/bishop checks, and the prior enemy-blocker/pinned-defender boundary.
No ordering or color-index defect was found.

## 7. Ordering, shared state, symmetry, and term interaction

### Current semantics

Material/PSQT are precomputed by Board. Pawn evaluation seeds pawn attacks.
Each piece then updates attack maps, mobility, threats, and king attackers
before its specific feature. King, Mobility, and Threats are read only after
all basic terms. `Trace::term_total()` reconstructs the unscaled white-relative
score. The normal and traced paths share the same compile-time implementation.

Color symmetry means a 180-degree board rotation plus color and side-to-move
swap. Horizontal file mirroring is a separate property and is not guaranteed
by the current PSQTs.

### CROSS-001 — Protect full structured color symmetry

**Classification:** Test gap — completed in `04f31ed`.

The color-symmetry test now covers every trace term, per-color exchange, total
negation, every transformation stage, side-to-move metadata, and final value
perspectives. The PSQT symmetry loop now includes King. The test reuses one
representative asymmetric pair and adds no corpus relation machinery.

### CROSS-002 — Whether horizontal file symmetry is required

**Classification:** Policy decision — resolved. **Confidence:** high.

Retain complete eight-file PSQTs and allow queenside/kingside asymmetry.
Horizontal reflection is not a correctness invariant: initial king/queen
placement, castling geometry, opening structures, and real-game distributions
distinguish the wings. Mandatory color symmetry remains the 180-degree rotation
plus color swap protected by CROSS-001. Snapshot mirror positions continue to
expose PSQT differences without requiring equality. Control the larger
parameter set through held-out tuning validation and matches; a file-symmetric
model may compete later as an explicit regularization experiment, not as a
required property. No code change or mirror-invariance test is needed.

### CROSS-003 — Normal/trace equivalence and significant term order

**Classification:** No action. **Confidence:** high.

Normal and traced evaluation instantiate one `evaluate_impl` template;
`eval::evaluate(board) == trace.value()` and
`trace.term_total() == trace.unscaled_score()` are tested and revalidated by
the snapshot runner. Comments in `evaluation.cpp` distinguish base terms from
terms requiring accumulated state. No speculative reordering or independent
term execution should be introduced.

## 8. Search-scale assumptions and non-evaluation consumers

### Current semantics

All ordinary static scores use the same `EvalValue` unit. Search currently uses
an aspiration window of 50, razoring margins 500/900/1800, and futility margins
250/400/550. Null-move eligibility requires side-to-move non-pawn MG material
greater than one rook. SEE and noisy ordering use MG material values. These are
heuristic scale assumptions, not terms in the HCE trace.

### SCALE-002 — Recalibrate downstream thresholds after score-scale changes

**Classification:** TUNE-002 candidate. **Confidence:** high.

- **Evidence:** constants in `src/search/algorithm.cpp`, MG values in
  `src/board/board_see.cpp` and `src/search/ordering/picker.cpp`, and the
  Board non-pawn material calculation all consume evaluation-scale values.
- **Intended behavior:** aspiration should be narrow but stable; razoring and
  futility should prune only when static-eval error is plausibly bounded; SEE
  and noisy ordering should preserve tactical exchange ordering; null move
  should be disabled in pawn-only/zugzwang-prone material.
- **Representative measurement:** use the fixed-depth six-position suite and
  search instrumentation for aspiration failures, razor tries/cutoffs,
  futility skips, and null-move behavior. Static position pairs alone cannot
  validate search thresholds.
- **Trace and consumers:** no trace term directly; every changed HCE score can
  alter threshold crossings and the tree.
- **Coverage:** unit/search tests protect mechanics but do not establish that
  the present margins are well calibrated.
- **Impact:** potentially much larger than the nominal eval delta because
  pruning and move order change nodes and PVs.
- **Dependencies/risks:** follows MAT-002 and any broad HCE rescaling. The
  search-stability note identifies TT/LMR/history sensitivity as a confounder;
  do not “fix” it by widening eval gaps.
- **Completion:** audit threshold-crossing rates after a retained score-scale
  change, tune search policy separately, and require fixed-depth plus match
  evidence.
- **Future plan:** yes, but as search-coupled follow-up to material/scale tuning,
  not bundled with the HCE parameter commit.

### SCALE-003 — Present consumer contracts under the current scale

**Classification:** No action. **Confidence:** high.

No unit mismatch or narrowing defect was found in current consumers. Material
lookups have one authority, SEE uses the same MG exchange values consistently,
the null-move guard compares like units, and search margins are ordinary
`EvalValue` constants. The concern is future calibration, captured by
SCALE-001/002, rather than current correctness.

## Correctness and coverage status

No confirmed correctness defects or essential invariant-test gaps remain open.
QUEEN-001 was corrected in `ca8699e`; CROSS-001 and KING-001 were closed in
`04f31ed`. No parameter value, uncommon convention, or missing feature is
classified as a correctness defect merely because a reference engine differs.

The existing exact weighted feature tests are useful refactor guards today.
During tuning, prefer retaining activation, deactivation, boundary, symmetry,
and monotonic assertions while intentionally updating or relaxing only numbers
that are actually being tuned. Do not grow a cross-product of colors, pieces,
and tables where a shared implementation already supplies the invariant.

## Resolved policy decisions

- **SCALE-001:** use one authoritative material table.
- **PAWN-002:** isolated excludes backward; doubled remains independently
  stackable. The selected taxonomy is implemented and verified.
- **PIECE-001:** any two bishops receive one bishop-pair bonus.
- **THREAT-001:** weak-piece pressure uses geometric attacker/defender counts.
- **CROSS-002:** retain full eight-file PSQTs and permit horizontal asymmetry.

## Reviewed areas requiring no action

- Board's incremental material/PSQT cache and recomputation boundary.
- Signed material and color-relative PSQT lookup mechanics.
- Stronger-side scaling, phase clamp, taper arithmetic, perspective, and tempo
  execution order.
- Existing isolated/backward/doubled mask mechanics under their current stated
  definitions.
- Pin-ray restriction for piece mobility and attack accumulation.
- Existing outpost, long-diagonal, bishop-blocker, rook-file, shelter, check,
  and danger activation mechanics.
- Attack/double-attack state ordering and normal/trace implementation sharing.
- Current unit consistency in SEE, noisy ordering, null-move eligibility, and
  search margins.
- Diagnostic formatting and fixed 100-unit display normalization.

## Goal workspace

### Objective and boundary

Complete a bounded human-guided rough HCE pass, then profile and optimize the
retained evaluator. The intended outcome is a coherent, explainable baseline
for later mathematical tuning, not a claim of demonstrated playing-strength
improvement.

Included work is TUNE-002 rough semantic and parameter calibration, SCALE-002
downstream search-scale review, PERF-001 measured hot-path optimization, and
PERF-002 worker-local pawn evaluation caching. Explicitly deferred work is raw
feature export, training and held-out datasets, Texel or gradient tuning,
automatic parameter sweeps, repeated match-driven optimization, nonlinear
mathematical tuning, and NNUE.

### Tuning and reference policy

Rough tuning is conservative by default, but moderate coherent changes are
allowed when the current relationships are implausible or the feature evidence
supports a better model. Do not broadly rewrite tables such as the PSQTs, alter
many unrelated terms together, or change a parameter merely because another
engine uses a different value. A task may legitimately conclude that the
current parameters should remain unchanged.

The middlegame pawn value is the fixed score-scale anchor and remains exactly
100 centipawns. Tune every other middlegame material value relative to that
anchor rather than rescaling the whole evaluation. Endgame material values,
including the endgame pawn value, remain available for tuning. Phase endpoints,
endgame scaling, tempo, positional weights, and search thresholds remain
separate tasks even though they use the resulting score scale. This fixed
anchor also removes an arbitrary scaling degree of freedom from later
mathematical tuning unless a future explicit policy revisits it.

The engines under `/home/rick/code/chess-engine-refs` are available for design
ideas and comparative reference. Inspect Stockfish, Minic, Ethereal, CPW, and
other relevant implementations when evaluating feature definitions, parameter
relationships, data layout, or known performance techniques. Their choices
are evidence of established approaches, not authorities for Latrunculi:

- account for differences in evaluation architecture, search, score scale,
  tuning history, and NNUE versus HCE;
- do not copy weights or feature definitions without understanding their
  surrounding model;
- use reference implementations to generate hypotheses, then validate the
  selected behavior in Latrunculi's traces, tests, benchmarks, and matches; and
- preserve a simpler local design when additional reference-engine complexity
  lacks measurable value here.

### Preflight baseline

Before PAWN-003 begins, commit this workspace and require an otherwise clean
worktree. Capture one common baseline for the complete goal:

- record the starting revision and whether it is dirty;
- build and archive the release engine under `scratch/baselines/`;
- record the archived binary's path and SHA-256 checksum;
- require `python3 bench/bench.py eval verify` to pass;
- capture one isolated evaluation-throughput run; and
- capture the six-position, one-thread, depth-five search run.

The archived engine is the opponent for the final aggregate match. Individual
tasks and checkpoint matches use the immediately preceding retained commit or
checkpoint binary as appropriate. Record the resulting identifiers here when
the goal starts:

| Baseline field | Value |
|---|---|
| Revision | `d5c50e61cb59ee5f516a24b474dbb5a9bb618bea` |
| Dirty state | Clean |
| Archived engine | `scratch/baselines/latrunculi-eval-goal-d5c50e6` |
| Engine SHA-256 | `74b3c274f4392af47efc4e0bfd799d85a8ba13cf5c96d0e93db581f66ccec455` |
| Evaluation snapshot | Verified against `evaluation_snapshot_v1` |
| Throughput run | `scratch/bench-runs/20260807-181259-eval-goal-before` |
| Fixed-depth search run | `scratch/bench-runs/20260807-181301-eval-goal-before` |

### Ordered task ledger

| Order | ID | Work item | Contract | Checkpoint | Status |
|---:|---|---|---|---|---|
| 1 | PAWN-003 | Add bounded passed-pawn semantics | Intentional evaluation change | A | Complete — retained |
| 2 | MAT-002a | Tune material ratios around MG pawn = 100 | Intentional evaluation/search change | A | Complete — no change |
| 3 | MAT-002b | Review phase endpoints | Intentional evaluation change | A | Complete — retained |
| 4 | MAT-002c | Review endgame scaling | Intentional evaluation change | A | Complete — retained |
| 5 | MAT-002d | Review tempo | Intentional evaluation change | A | Complete — no change |
| 6 | PSQT-001 | Rough PSQT calibration | Intentional evaluation change | B | Complete — no change |
| 7 | PAWN-004 | Rough pawn-parameter calibration | Intentional evaluation change | B | Complete — no change |
| 8 | MOB-001 | Settle mobility-area and outpost semantics | Possible semantic change | B | Complete — retained |
| 9 | PIECE-003 | Rough piece-feature calibration | Intentional evaluation change | B | Pending |
| 10 | MOB-002 | Rough mobility-curve calibration | Intentional evaluation change | B | Pending |
| 11 | THREAT-002 | Consider one bounded threat refinement | Possible semantic change | C | Pending |
| 12 | KING-002 | Rough king-safety calibration | Intentional evaluation change | C | Pending |
| 13 | SCALE-002 | Audit downstream search thresholds | Search-policy change | C | Pending |
| 14 | PERF-001 | Profile and optimize measured hot paths | Exact evaluation preservation | Performance | Pending |
| 15 | PERF-002 | Add and measure a worker-local pawn hash | Exact evaluation preservation | Performance | Pending |

Checkpoint A covers pawn semantics and material transformations; B covers
PSQTs, pawn weights, pieces, and mobility; C covers threats, king safety, and
downstream search-scale calibration. MAT-002a may receive an immediate reduced
match before checkpoint A because material scale affects nearly every
evaluation and search consumer. Performance tasks use equivalence and speed
evidence rather than strength checkpoints.

PERF-001 begins with profiling rather than a speculative implementation list.
Only measured, independently worthwhile hotspots should become lettered work
items such as PERF-001a or PERF-001b, each with its own focused commit.

### Workspace record

Each task records its status, hypothesis, allowed scope, required evidence,
outcome, and commit. Use `pending`, `in progress`, `complete — retained`,
`complete — no change`, or `complete — rejected` as status values. A task entry
should use this shape:

```markdown
### ID — Title

Status: pending

Hypothesis:
...

Allowed scope:
...

Required evidence:
...

Outcome:
Pending.

Commit:
Pending.
```

#### PAWN-003 — Passed-pawn semantics

Status: complete — retained

Hypothesis:
A rank-only bonus for pawns with no opposing pawn ahead on the same or an
adjacent file supplies the missing core passed-pawn signal without prematurely
adding candidate, support, blocker, king-distance, or rook-relation features.
Use a conservative monotonic MG/EG curve that becomes material only on advanced
ranks.

Allowed scope:
Pawn-mask detection, one relative-rank parameter table, focused activation and
boundary rows in the existing pawn-feature test, reviewed snapshot changes,
and downstream measurements. Keep the result in `Term::Pawns` and introduce no
Board API or reusable passed-pawn state before PERF-002 determines the cache
boundary.

Required evidence:
Both colors must recognize symmetric passers; an opposing pawn ahead on the
same or adjacent file must suppress the bonus; advanced passers must receive a
larger bonus; existing pawn classifications and full color symmetry must pass;
and corpus, search, and throughput changes must be reviewed before retention.

Outcome:
Locally retained pending checkpoint A. The implementation adds the common
rank-only passer definition used as the foundation of the reference engines'
broader models, with a conservative monotonic curve from relative ranks 3--7.
It changes only `promotion-race`, `endgame-arasan-16`, and
`endgame-arasan-21` in the diagnostic corpus. The symmetric promotion race
still cancels in the total score; the Arasan endings gain the expected White
passed-pawn value.

Focused, full release, and focused sanitizer tests pass. The isolated
evaluation median changed from 260.321 to 266 ns/evaluation (+2.2%), a small
cost accepted provisionally before PERF-002. The depth-five suite leaves
startpos and `arasan20-01` identical, changes passer-relevant scores and trees,
and changes the best move in `arasan20-08` and `arasan20-16`. These are reviewed
semantic consequences rather than strength evidence; checkpoint A determines
whether the feature remains retained.

Commit:
`feat(eval): add passed-pawn evaluation` (this task's commit).

#### MAT-002a — Material ratios

Status: complete — no change

Hypothesis:
The unusually large non-pawn ratios might be arbitrary outliers that should be
replaced with conventional values around the fixed 100-centipawn MG pawn.

Allowed scope:
The five authoritative MG/EG material pairs and the existing exact-value tests.
Phase endpoints, PSQTs, positional features, and search thresholds remain
separate tasks.

Required evidence:
Trace the values through Board caches, phase, SEE, noisy ordering, and null-move
eligibility; compare their provenance and their relationship to the PSQTs with
the reference engines; retain a change only with a coherent local rationale.

Outcome:
No change. History shows that the complete material and PSQT set was introduced
together from an established tapered-evaluation model and later rescaled as a
unit while preserving its ratios. The current table is therefore not safely
replaceable with standalone textbook values: doing so would simultaneously
alter effective piece-plus-square values, phase, SEE, capture ordering, and
null-move eligibility. Stockfish, Minic, and Ethereal also demonstrate that
their material ratios depend strongly on their surrounding evaluation and
search architecture. The MG pawn remains exactly 100; later mathematical
tuning can fit the coupled material/PSQT system from data.

Commit:
None; the reviewed values were retained.

#### MAT-002b — Phase endpoints

Status: complete — retained

Hypothesis:
Mapping zero non-pawn material to pure endgame and the exact initial non-pawn
material to pure middlegame gives the current material-weighted phase formula
meaningful endpoints. The existing `[2500, 10000]` clamps classify positions
with substantial material as pure endgames and remain at pure middlegame after
some opening exchanges.

Allowed scope:
The two phase material endpoints and the existing phase test. Do not change
material values, interpolation arithmetic, scaling, tempo, or feature weights.

Required evidence:
Protect kings-only, initial, mixed-material, and rook-only phase behavior;
review the deterministic snapshot and fixed-depth search changes; require full
tests and color symmetry before retention.

Outcome:
Retained. The endpoints are now zero non-pawn material and the exact initial
non-pawn material derived from the authoritative MG piece values. This keeps
the existing material-weighted interpolation while removing both arbitrary
clamps. Kings-only remains pure EG, the initial position remains pure MG, and
a two-rook ending now retains phase 19 rather than being forced to pure EG.

Focused and full release tests pass, and the deterministic baseline was
reviewed and regenerated. The isolated median remains 266 ns/evaluation. The
depth-five suite shows the expected broad score/tree sensitivity; startpos is
unchanged, while `arasan20-30` changes most strongly. This is accepted as the
deliberate consequence of allowing remaining material to determine phase
rather than saturating at the old thresholds, subject to checkpoint A's paired
match.

Commit:
`feat(eval): use full material range for game phase` (this task's commit).

#### MAT-002c — Endgame scaling

Status: complete — retained

Hypothesis:
The generic stronger-side pawn scaler should remain deliberately simple, but
its current `36 + 5 * pawns` curve discounts every pawnless advantage to 56%
even when the material is plainly decisive. A `48 + 4 * pawns` curve retains
the established low-pawn drawishness signal while applying a less aggressive
default discount and reaching full scale at four pawns.

Allowed scope:
The generic scale base and per-pawn slope, their placement with evaluation
parameters, and the existing scale-factor test. Do not add specialized
insufficient-material, opposite-bishop, fortress, or material-pattern rules.

Required evidence:
Compare established reference-engine generic scale curves, protect zero-, one-,
and many-pawn boundaries, review snapshots and fixed-depth search, and require
the full release and symmetry suites before retention.

Outcome:
Retained. The generic curve is now `min(64, 48 + 4 * stronger_pawns)`, matching
the conservative shape used as the fallback by established HCEs without
importing their specialized material recognizers. The literals are named
evaluation parameters. Zero-, one-, and four-pawn boundaries are protected by
the existing scale test.

The deterministic changes are confined to scaled/final values and were
reviewed and regenerated. The depth-five suite retains all best moves except
the deliberate phase-sensitive changes already introduced by checkpoint A;
notably, `arasan20-30` returns from the phase-only experiment's +292 score to
-12. One search test formerly froze an exact fail-soft score despite testing
only that tactical moves survive futility pruning; its assertion now checks the
owned beta-cutoff invariant. Full release verification passes. Throughput is
measured again at checkpoint A because this arithmetic-only parameter change
cannot structurally add evaluator work and the first sample was noisy.

Commit:
`feat(eval): soften generic endgame scaling` (this task's commit).

#### MAT-002d — Tempo

Status: complete — no change

Hypothesis:
The +20 initiative bonus might need recalibration after the checkpoint's phase
and scaling changes.

Allowed scope:
The single tempo parameter and its perspective/null-move invariants. Do not
change material transformations, positional terms, or search margins.

Required evidence:
Review history, current invariants, and comparable HCE values; retain a change
only with evidence stronger than choosing another plausible small constant.

Outcome:
No change. The current +20 is deliberately applied after tapering and scaling,
is protected by side-to-move and null-move relations, and matches Ethereal's
established HCE tempo (Minic uses the nearby value 15). The checkpoint supplies
no evidence that choosing a different plausible constant would improve this
engine, so tempo remains a later mathematical-tuning parameter.

Commit:
None; the reviewed value was retained.

### Checkpoint A result

Checkpoint A is retained. The completed standard-profile screen used 100
opening pairs at `10+0.1`, one engine thread, 32 MB Hash, and concurrency 8
against the archived pre-goal `d5c50e6` binary. Run directory:
`scratch/bench-runs/20260807-183021-checkpoint-a-c8`.

The candidate scored 86 wins, 81 draws, and 33 losses (63.2%), with
pentanomial counts `[3, 13, 30, 36, 18]` and Cute Chess reporting
`+94.3 +/- 37.6 Elo`, LOS 100%. This is strong screening evidence for the
checkpoint as a group, not attribution to any one parameter. The final
aggregate standard match remains required after all tuning and performance
work.

#### PSQT-001 — Rough PSQT calibration

Status: complete — no change

Hypothesis:
The inherited piece-square tables might contain clear local outliers suitable
for conservative manual correction before mathematical tuning.

Allowed scope:
Small, explainable relationships within one piece/phase table. Do not broadly
smooth, symmetrize, replace, or jointly rescale the tables.

Required evidence:
Review history, effective material-plus-square values, opening diagnostics,
color symmetry, and reference tables; retain only a correction supported
without fitting hundreds of correlated cells by eye.

Outcome:
No change. The tables were introduced and rescaled together with material, are
color-symmetric, and contain no isolated value that is demonstrably erroneous
independent of the surrounding search and evaluation. Hand-editing this
768-value correlated system from a six-position search sample would be
overfitting. PSQTs remain a primary target for held-out mathematical tuning.

Commit:
None; the reviewed tables were retained.

#### PAWN-004 — Rough pawn-parameter calibration

Status: complete — no change

Hypothesis:
The isolated, backward, doubled, and newly added passed-pawn weights might have
an implausible relationship suitable for a bounded manual adjustment.

Allowed scope:
Only those pawn weights; taxonomy and passer detection remain fixed.

Required evidence:
Compare activation traces and reference-engine magnitudes, preserve monotonic
passer growth, and change only a clear outlier.

Outcome:
No change. The structural penalties are small in MG, larger in EG, and fall
within the broad range of established HCEs; the passer curve is conservative,
monotonic, and already survived checkpoint A. More precise relationships are
correlated with PSQTs and belong to mathematical tuning rather than another
manual guess.

Commit:
None; the reviewed parameters were retained.

#### MOB-001 — Mobility-area and outpost semantics

Status: complete — retained

Hypothesis:
Mobility should exclude own pawns that are actually blocked rather than every
own pawn on its home rank. This measures useful latent activity consistently
after pawns leave home and follows the established Ethereal/Stockfish-style
mobility-area model. The existing supported-only outpost definition remains a
coherent simpler policy.

Allowed scope:
The mobility-area pawn mask, its existing test, and reviewed evaluation
snapshots. Do not change mobility curves, attack maps, pin handling, or outpost
bonuses/eligibility in the same experiment.

Required evidence:
Protect initial, empty, and mutually blocked pawn positions for both colors;
retain all mobility/pin/outpost tests; review corpus, throughput, search, and
checkpoint evidence.

Outcome:
Retained. Mobility now excludes actually blocked friendly pawns rather than all
home-rank pawns. Initial, empty, and mutually blocked positions protect both
colors; the existing mobility, outpost, pin-ray, and full symmetry tests pass.
Supported-only outposts remain unchanged as the simpler local policy.

The reviewed snapshot changes are confined to Mobility and downstream summary
rows. Opening `e4` drops from +86 to +32 because both sides receive latent
mobility behind pawns that can advance, while genuinely blocked pawn squares
remain excluded. The median isolated run improved from 264.661 to 252.911
ns/evaluation, although that arithmetic-neutral result is treated as favorable
noise rather than a claimed optimization. All six depth-five best moves remain
unchanged; startpos nodes fall 29.3% and the other node changes are bounded.
Focused and full release tests pass.

Commit:
`feat(eval): count latent mobility behind movable pawns` (this task's commit).

Before changing code, independently revalidate the task against the current
source. Work on one coherent hypothesis at a time and create one focused commit
for each retained task, including its workspace update. Fully revert rejected
experiments; do not create an empty source commit merely to preserve them.
Record rejected outcomes in a later workspace documentation update. Preserve
unrelated changes. The goal is authorized to create these commits and continue
autonomously without approval between tasks. Stop only when a consequential
new policy decision, external dependency, or genuine blocker requires user
direction.

### Measurement policy

For semantic and parameter changes:

- retain focused activation, boundary, symmetry, and monotonic tests;
- review and intentionally regenerate deterministic snapshots when scores
  change;
- compare the fixed-depth search suite;
- review isolated throughput when the change could affect hot-path cost; and
- do not present snapshot, trace, throughput, smoke-match, or short-match
  results as strength evidence.

For performance changes:

- require profiler, cache, or similarly direct evidence before implementation;
- require byte-identical evaluation snapshots;
- preserve deterministic fixed-depth scores, nodes, PVs, and best moves where
  applicable; and
- compare isolated evaluation throughput and downstream search performance.

Match testing uses a bounded evidence funnel rather than either no intermediate
testing or a full standard match for every small edit:

1. Every task first passes its focused tests, snapshot review, fixed-depth
   search comparison, and relevant throughput checks. A retained task is one
   focused commit and remains locally validated rather than independently
   strength-proven.
2. After each coherent group, run a reduced checkpoint match of roughly
   100--200 opening pairs using the existing standard settings. The intended
   checkpoints are pawn semantics plus material transformations; PSQTs, pawn
   weights, pieces, and mobility; and threats, king safety, plus downstream
   search-scale calibration.
3. Treat checkpoint matches as screening evidence. They can identify a clear
   regression but cannot establish a small Elo gain. If a checkpoint is
   materially negative, use the separate commits to isolate the responsible
   change with targeted follow-up matches.
4. Give a broad material-scale change or other high-impact semantic change an
   immediate reduced match when waiting for its group would create too much
   uncertainty.
5. After the complete rough pass, run one full standard match against the
   archived pre-goal engine. A positive interval supports a strength claim; an
   inconclusive result permits only the description "coherent rough
   calibration"; a clearly negative result requires checkpoint-level
   investigation and appropriate reverts.

Correctness fixes may be retained from direct correctness evidence even when a
match is inconclusive. Subjective changes remain provisional until their
checkpoint is not clearly harmful. Performance-only changes need no individual
match when evaluation snapshots and deterministic fixed-depth behavior remain
exact and repeatable throughput or search-speed measurements improve.

The opening gaps in `docs/search-stability.md` remain evidence for
PSQT/development review, not a license to tune around TT/LMR/history
instability.

### Completion assessment

The feature audit, correctness/test follow-up, policy decisions, and PAWN-002
implementation are complete, so TUNE-001 is complete. The rough-tuning and
performance workspace is ready to execute. Mathematical tuning and NNUE remain
governed by [`eval-roadmap.md`](eval-roadmap.md) and are outside this goal.
