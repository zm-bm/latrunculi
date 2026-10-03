# Earlier engine ideas

Temporary retention of the earlier queue and its evidence. These are historical proposals,
not the active work board. Their thresholds, priority bands, and serial prerequisites do not
govern new work. Renew the evidence for the current baseline before promoting a lead into
[Engine Development](engine-development.md#issues-and-leads); keep its original ID.

## Historical proposals

These recipes, including their formulas and acceptance thresholds, are provisional leads from
earlier baselines. Revalidate the hypothesis and evidence before using one. Priority bands and
serial prerequisites are not an execution order; preserve dependencies that affect the evidence.
The current [testing rules](engine-testing.md) govern acceptance. **Retain** means retain one candidate for
testing, not integration.

ENG-003 through ENG-018 derive from the 2026-09-25 audit of `c8bfedd`: the 200-position
depth-10 corpus and a historical eight-position depth-12 profile, with one thread and 32 MiB Hash.
Counts describe that fixed tree; sampled cycle shares are approximate. Source comparisons used
Ethereal `0e47e9b`, Stockfish `86f1df7`, and Minic `4317c14` as mechanism references.

ENG-019 through ENG-031 derive from the 2026-09-26 engine-gap audit of `0ff898c`: 96 group-unique stratified endgames, a
512-position phase-balanced evaluation panel, and 43 deeply checked earliest game deteriorations.
The audit used Stockfish fixed-node scores as approximate references, without local WDL/DTZ
tablebases or PGN clock annotations. Those samples did not support the tested rook-ending,
opposite-bishop, qsearch, king-activity, or rescaling explanations; they do not exclude those
areas as sources of weakness.

**Shared focused-panel protocol**

Use recorded game positions or a retained development dataset, with a full FEN and source group
for every row. Record the inputs and baseline revision. Canonicalize by the first four FEN fields,
retain at most one position per source group, and rank otherwise-equivalent choices by SHA-256
of the task ID, source group, and canonical FEN. Assign a group to validation when the unsigned
value of the first eight hex digits of SHA-256 of its group ID modulo five is zero; all others are
development. Freeze selection and the split before implementation. Record the panel's content hash.

Select at most 256 positions per task, balancing its named cells and relative pawn ranks as evenly
as availability permits. A comparison cell requires at least 20 source groups, and error
prerequisites count distinct source groups. Analyze the baseline at 50k and 500k requested nodes
with one thread, 32 MiB Hash, and fresh state. Record the reference engine's version/revision,
binary, and settings in the experiment evidence. Use that fixed reference-engine binary at
1M nodes, one thread, 32 MiB Hash, and MultiPV 3. If Latrunculi's move is outside MultiPV 3, search its
successor separately at the same reference limit and convert the score to the root perspective.
Choose a variant on development only, freeze it, and inspect validation once.

Predeclare an aggregate screening criterion for the claimed effect. On the frozen validation
panel, report paired mean move-loss change, uncertainty across source groups, and the counts and
severity of repaired and new large errors. Review serious regressions; a new approximate-reference
error or worse median alone is not a veto. Retain only when the aggregate evidence supports the
claim; sparse or unstable evidence stays unresolved. Reference agreement does not establish
correctness or playing strength. Independently verified exact-WDL rules remain hard checks.

**Higher potential**

### ENG-020 — Re-search quiet failures with one selectivity family disabled

**Start:** causal diagnostic, not a candidate; run after ENG-019 is terminal and without an
evaluation patch. The motivating cases are quiet choices still at least 100 cp worse at 500k nodes,
with no common mechanism in passive counters.

**Try:** deterministically select 128 source groups from each phase bucket 0--31, 32--63, 64--95,
and 96--128 using the shared hash rule and analyze them with the shared protocol. A persistent case
has at least 100 cp loss at 500k and a non-capture, non-promotion, non-checking reference move; a
repaired control has at least 100 cp loss at 50k and less than 50 cp at 500k. Keep at most 24 cases
and eight controls by task hash, requiring at least 12 and four. Record baseline completed depth,
then run three separate variants at that fixed depth: disable all LMR; disable null-move pruning;
disable razoring, static/child futility, and late-move/history pruning together. Change no ordering,
extension, or evaluation rule. Cap each variant at 10M nodes per case and mark capped cases
unavailable. A repair enters the reference engine's MultiPV 3 or falls below 50 cp loss. Record
repairs, new large control errors, nodes, and node multipliers.

**Decide:** implicate a family only when it uniquely repairs at least three persistent source
groups, creates no control error of at least 100 cp, and completes equal-depth searches for at least
80% of both panels. Route LMR evidence to ENG-013, PVS consequences to ENG-014, or create one new
bounded null/forward-pruning experiment naming the repaired stratum. If the panels are too small,
close unresolved; if no family qualifies, record a null result. Never retain a disabled-pruning
build or blend families.

### ENG-004 — Cache stable pawn-and-king evaluation terms

**Start:** measurement-first tree-preserving speed experiment. Shelter is about 18% of standalone
evaluation and evaluation is 39--40% of search cycles; reference caches establish feasibility, not
a Latrunculi layout.

**Try:** measure reuse and reuse distance for a complete pawn/king/castling key on the standard
search corpus. Continue only if the observed reusable work could plausibly clear the standard
timing gate, then prototype one small per-worker cache containing exactly the deterministic
pawn-structure and shelter results. Verify key completeness through castling-right, king-square,
and pawn-structure boundaries, exact cached versus uncached scores, exact search signatures, and
the absence of shared-worker races.

**Decide:** close as a null result on low reuse or insufficient upper-bound savings. Otherwise
retain only if scores and trees are exact, cache overhead remains bounded, and paired integrated
timing passes; reject on stale values, excessive footprint/lookup cost, or no repeatable gain.

### ENG-005 — Short-circuit transposition-table misses

**Start:** measurement-first tree-preserving speed experiment; run after ENG-003 is terminal so its
TT layout is fixed. TT work is 15--16% of cycles, with main/qsearch miss rates of 48%/85.8%; the
current four-entry scan snapshots and decodes every candidate.

**Try:** count miss-path loads and decoded payloads, then test one layout that checks a key/tag
before loading and decoding the remaining payload. Preserve the 16-byte entry, 64-byte cluster,
probe order, replacement/aging policy, and race contract, and remain compatible with ENG-003's
terminal layout. Test hits, misses, collisions, generations, replacement, concurrent snapshots,
and exact corpus and benchmark signatures.

**Decide:** close null if early rejection cannot remove enough work to plausibly pass timing.
Retain only with unchanged semantics and signatures plus a passing paired timing result; reject on
new collision/race risk, larger storage, extra hit-path cost that cancels savings, or no gain.

### ENG-006 — Add a thresholded SEE fast path

**Start:** measurement-first tree-preserving speed experiment. SEE consumes 5--6% of search cycles
and about 8% of branch misses, while every ordinary noisy candidate currently receives an exact
exchange score.

**Try:** count SEE calls by consumer, threshold, and result band. For consumers that only classify
a capture against a threshold, prototype an exact boolean `see_ge`-style path; retain exact SEE
where magnitude determines in-band ordering. Preserve the current SEE bands and do not reintroduce
CaptureHistory. Prove boolean equivalence to exact SEE across every used threshold with targeted and
random legal positions, preserve emitted move order, and compare exact search signatures and
paired timing.

**Decide:** close null if sign-only consumers are too sparse or duplicate exact work on surviving
captures removes the plausible gain. Retain only with exact classification/order/signatures and a
passing tree-preserving timing result; reject any unintended ordering change or mismatch.

### ENG-007 — Remove common-path late-picker scanning

**Start:** measurement-first tree-preserving speed experiment. Move picking is 18--20% of cycles
and about 36% of branch misses; ordinary-quiet and bad-noisy stages return 71% of main candidates
but produce only 3.2% of beta cutoffs.

**Try:** count comparisons, rescans, alpha raises, and exact-node contribution separately for those
stages. If one stage contains enough removable common-path work to plausibly pass timing, test one
same-order bucket or partial-selection structure for that stage only. Do not retry the rejected
score/history reuse. Compare the complete emitted sequence on targeted and randomized legal
positions, then require exact corpus and benchmark signatures and measure picker branches/misses
and paired integrated time.

**Decide:** close null if no dominant removable scan exists. Retain only when move order and search
trees are exact and timing passes; reject if bookkeeping shifts equivalent cost elsewhere, changes
order, or lacks repeatable speedup.

**Promising but lower-confidence or smaller**

### ENG-013 — Stratify LMR re-searches before testing adaptive verification depth

**Start:** measurement-first tree-changing search experiment. Only 25,959 of 7.58M reductions fail
high, but their inclusive full-depth footprint is 16.44M nodes and just 58.7% remain above alpha.

**Try:** split reduced fail-highs by PV/NonPV, reduction, depth, fail-high margin, and full-depth
outcome. Continue only if one repeatable stratum dominates avoidable verification work, then test
one intermediate or result-conditioned verification depth derived from that stratum. Do not retry
the rejected full-depth null-window scout or the rejected history thresholds. Record initial reduced
result, verification work, final outcome, and any displaced PVS or other re-search work.

**Decide:** close null when no stable high-cost discriminator appears. Retain only if the isolated
policy preserves objective results, meets its predeclared tree/timing gates, and reduces total
verification work rather than moving it elsewhere; otherwise reject.

### ENG-014 — Isolate expensive PVS misses

**Start:** causal diagnostic leading to at most one tree-changing experiment. Root/internal PVS
miss rates are only 1.06%/1.98%, but their inclusive footprints are 5.23M/7.50M nodes, so a small
high-level stratum may dominate.

**Try:** stratify root and internal misses by depth, move rank, miss margin, node type, and
exclusive re-search work. Attribute overlap with LMR and other retries. Continue only if one
bounded stratum accounts for a repeatable concentration of exclusive work, then test one window or
sequencing change for that stratum without changing other search policy.

**Decide:** close null if cost is diffuse, cannot be separated from another mechanism, or lacks a
safe discriminator. Retain a prototype only when its targeted misses and exclusive work fall,
objective results pass, and the predeclared complete-corpus tree/timing gates pass.

### ENG-008 — Incrementalize tactical-cache maintenance

**Start:** measurement-first tree-preserving speed experiment. Board-transition maintenance is
about 80% of perft cycles but only 8--9% of integrated-search cycles, so perft alone cannot qualify
the work.

**Try:** attribute `refresh_tactical_cache` cost to each maintained field on perft and integrated
search. If one field has enough integrated share to plausibly pass timing, prototype one exact
incremental update for that field only. Compare full recomputation after make, unmake, null move,
captures, promotions, castling, and en passant; require identical perft and search signatures and
measure transition plus integrated time.

**Decide:** close null when useful cost is fragmented or the upper bound is too small. Retain only
with exact cache state, perft, and search behavior plus a passing paired integrated timing result;
reject if correctness complexity is disproportionate or savings exist only in perft.

### ENG-017 — Generate or reject evasions more cheaply

**Start:** measurement-first tree-preserving speed experiment. Main and qsearch evasion stages
reject 31.4% and 34.0% of returned candidates, while generation and legality contribute measurable
branch misses.

**Try:** classify illegal rejects by move type and validation cost. If one class contains enough
removable integrated work, compare one exact legal-evasion filter or generation rule for that class
while preserving the legal set and order. Test single/double check, pins, interpositions, king
captures, promotions, and both main/qsearch paths; compare emitted sequences, exact search
signatures, legality checks, picker branches, and paired time.

**Decide:** close null if rejects are cheap or no class can plausibly clear timing. Retain only with
the identical legal sequence and tree plus a passing timing result; reject if generator complexity
or shifted validation cost cancels the saving.

### ENG-015 — Find a materially different qsearch capture-safety signal

**Start:** measurement-first tree-changing search experiment. Qsearch is 69.2% of nodes, but 57.7%
of stand-pat evaluations and 92.4% of TT hits already cut off; the prior captured-value margins of
200/300/400 skipped real cutoffs and remain closed.

**Try:** expose exact-SEE-negative picker rejects and classify searched captures by SEE band,
check, promotion, bound improvement, PV use, and cutoff. Continue only if one combined predicate
identifies a substantial near-zero-utility stratum and is materially different from the rejected
captured-value-only rule. Test that predicate alone, recording skipped moves, lost improvements,
cutoffs/PV moves, objective results, and complete-corpus tree and timing effects.

**Decide:** close null if every plausible stratum contains meaningful cutoffs or PV moves or is too
small to matter. Retain only if the named stratum remains safe and the prototype passes its
mechanism, objective, tree, and timing gates; otherwise reject.

### ENG-016 — Specialize qsearch TT work by usefulness stratum

**Start:** measurement-first tree-changing search experiment. Qsearch makes 39.16M probes with a
14.2% hit rate, but 92.4% of hits cut off; neither global removal nor raw miss rate is a valid
hypothesis.

**Try:** split probes and stores by check state, node type, ply, hit payload, and resulting cutoff
or move use. Continue only if one repeatable stratum has negligible cutoff and ordering value with
enough work to matter, then skip or specialize TT work for that stratum alone. Record compensated
node growth, search-result changes, probe/store cost, and paired integrated timing.

**Decide:** close null if every stratum has material cutoff/ordering value or insufficient cost.
Retain only if the selected policy passes objective, tree, and timing gates without compensating
node growth; reject changed results outside the predeclared tree-changing effect or absent gain.

### ENG-018 — Measure static-evaluation trend as one search signal

**Start:** measurement-first tree-changing search experiment. Test whether same-side prior-ply
static trends distinguish pruning outcomes in Latrunculi. Prior improving-aware LMP variants
remain closed.

**Try:** add a stats-only improving/worsening classification from same-side prior-ply static
evaluations and split razoring, futility, and LMR outcomes by it. Account for the state/evaluation
cost. Continue only if one consumer has a repeatable outcome separation large enough to justify
that cost, then test one conservative gate or reduction change for that consumer only; do not
reopen LMP.

**Decide:** close null if rates do not separate materially or prospective savings cannot exceed
the added work. Retain only if the single consumer passes its mechanism, objective, tree, and
timing gates; otherwise reject.

**Conditional or marginal**

### ENG-021 — Census every <=7-piece position with exact WDL and DTZ

**Start:** tree-preserving measurement, not a candidate. It is blocked until local Syzygy files
provide readable WDL and DTZ for every at-most-seven-piece material signature in the current
development dataset. If any signature is missing, report it and stop rather than mix exact and
approximate labels.

**Try:** reconstruct every full six-field FEN with at most seven pieces; recompute and record the
count (8,102 in the audit). For each root and legal successor, probe WDL and DTZ with one pinned
library, normalize to the root side, and preserve halfmove-clock and fifty-move semantics. Search
each root at 5k, 50k, and 500k requested nodes with one thread, 32 MiB Hash, and fresh state; do not
add in-engine probing. A WDL error selects a successor worse for the root than an available legal
alternative. Group by color-normalized material signature and source group, using the smallest
SHA-256 key as the primary observation per class/group. Report group-unique errors and 95% Wilson
intervals as primary results; retain all-position DTZ regret among preserving moves, static sign,
chosen move, depth repair, every raw probe, coverage/hash manifests, and every probe failure.

**Decide:** unlock ENG-026 through ENG-029 or one new bounded follow-up only for a class with at
least 20 covered source groups, five group-unique WDL errors, and a 95% Wilson interval not
overlapping the overall census interval. If no class qualifies, close specialized low-material work
as a null result. Keep this distinct from TB-001 production probing.

### ENG-030 — Add one-ply singular extension for a proven TT move

**Start:** tree-changing search experiment, blocked until ENG-020 is terminal. Replay the first
eight plies of each unrepaired case's fixed reference PV as independent roots and trace the
completed iteration's TT entry. Continue only if at least three source groups have a depth-six-or-
greater root where the legal TT move matches the reference continuation and satisfies the
eligibility below.

**Try:** at a non-root node of depth at least six, outside check and with no excluded move, require
a legal TT move, exact or lower bound, TT depth at least current depth minus two, and non-mate TT
score. Set singular beta to TT score minus twice current depth and verification depth to integer
`(depth - 1) / 2`. Search the same node excluding the TT move with null window
`[singular beta - 1, singular beta]`; on fail-low, extend only the TT move by one ply. Permit one
singular extension per root-to-leaf path and change no LMR, PVS, pruning, ordering, or evaluation.
Run baseline/candidate at 50k/500k on qualifying cases and ENG-020's repaired controls, recording
eligibility, excluded searches, extensions, repairs, new errors, and nodes. Test exclusion,
recursion blocking, in-check/mate/insufficient-TT rejection, both bound types, and fresh-search
determinism. On the full 200-position corpus require no objective regression and geometric-mean
node ratio at most 1.10.

**Decide:** retain only if at least three qualifying groups repair below 50 cp or into MultiPV 3,
no control becomes a 100+ cp error, the node cap and mechanism tests pass, and no LMR/PVS change is
stacked. Otherwise reject this singular shape.

### ENG-024 — Recognize one-exchange candidate passers

**Start:** tree-changing feature experiment, blocked until ENG-019, ENG-022, and ENG-023 are
terminal. Continue only with at least five distinct residual errors of at least 100 cp where the
reference move preserves or creates the candidate predicate and the baseline move does not.

**Try:** consider a non-passed pawn with exactly one enemy pawn in its forward same-or-adjacent-file
passed-pawn mask. Require a different friendly pawn to attack that blocker and verify on temporary
pawn bitboards that removing it by that capture makes the original pawn passed. Treat attacks
geometrically regardless of pins. Test an endgame bonus of 1/4 or 1/2 of the true-passer bonus for
that relative rank; exclude general majorities, multiple-exchange candidates, and non-pawn support.
Apply the shared protocol with true candidates, unsupported one-blocker positions, two blockers,
and existing passers as separate cells. Unit-test both colors, edge files, doubled pawns, two
supporters, wrong-blocker removal, and hypothetical recomputation; record whether reference lines
execute the enabling exchange.

**Decide:** retain the smaller fraction only if the focused rule passes, at least two prerequisite
errors repair, and new validation errors do not concentrate in losing pawn exchanges. Reject on
sparse support, near-miss leakage, or development-only improvement.

### ENG-028 — Scale blockaded lone-minor pawn endings

**Start:** tree-changing evaluation experiment, blocked until ENG-021 is complete. Filter exact
positions to a nominally stronger side with one bishop or knight and one to three pawns against no
enemy non-pawn piece and zero to three pawns; exclude queens and rooks. Continue only when the
predicate below has at least 20 source groups and five exact draws scored at least +150 cp for the
stronger side both statically and at 500k nodes.

**Try:** choose the stronger side from the unscaled endgame score. The predicate holds when none of
its pawns has a legal single push because its next square is occupied and the defending king is
within Chebyshev distance one of the next square of its most advanced pawn. Multiply only that
side's endgame score by 0, 16, or 32 over the current scale denominator 64. Select on development,
keeping bishop and knight results separate and retaining at most one minor-type rule whose direction
repeats. Exclude fortress, wrong-bishop, one-flank, and other blockade rules. Use ENG-021's exact
WDL/DTZ rows with the shared group split, balancing minor type, pawn counts, and side to move. Test
colors, empty/capturable blockers, pinned pawns, tied advanced pawns, king distances zero/one/two,
and adjacent material classes; report each factor's static score, 500k score/move, WDL, and any
exact win crossing below +50 cp.

**Decide:** retain the least aggressive factor reducing validation false wins without moving an
exact win below +50 cp, choosing a drawing/losing move, or applying outside the predicate. Reject
conflicting bishop/knight directions, sparse support, or no useful effect at factor 32; do not
broaden the geometry.

### ENG-029 — Scale low-pawn minor endings confined to one flank

**Start:** tree-changing evaluation experiment, blocked until ENG-021 is complete. Select positions
with no rook or queen, at least one but at most one minor per side, at most four total pawns, all
pawns on files a--d or all on e--h, and a non-pawn material edge no larger than one minor. Exclude
opposite-colored bishops. Continue only with at least 20 source groups and five exact draws scored
at least +150 cp for the stronger side both statically and at 500k nodes.

**Try:** choose the stronger side from the unscaled endgame score and cap its current pawn-count
scale at 16, 32, or 48 over denominator 64. Do not multiply scales, distinguish board flanks, or add
a general low-pawn, OCB, rook-ending, or fortress rule. Use ENG-021's exact rows with the shared
group split, balancing flank, minor material, pawn count, side to move, and WDL. Test a pawn
crossing d/e, zero/five pawns, a second minor, a rook/queen, equal material, an edge above one
minor, and OCB. Report each factor's static and 500k scores, move WDL, DTZ regret, and exact wins
moved below +50 cp.

**Decide:** retain the least aggressive factor reducing validation false wins with no WDL-losing
validation move and no exact win below +50 cp. Reject sparse support, flank/color asymmetry, harm to
exact wins, or no effect at factor 32; do not reopen OCB scaling.

### ENG-026 — Add a built-in exact KPK bitbase

**Start:** tree-changing exact-knowledge experiment, blocked until ENG-021 reports at least five
group-unique KPK WDL errors or an exhaustive KPK scan finds at least five legal states with a
WDL-losing baseline root choice. Otherwise close unsupported.

**Try:** generate a compact table for every legal king-pawn-versus-king state after normalizing the
pawn side to White, indexed by both kings, normalized pawn square, and side to move. Store WDL and
signed distance to the next zeroing move and combine them with the current halfmove clock. Probe
only exact KPK at non-root main/qsearch entry after rule draws and before TT/pruning. Return zero
for draws and a dedicated known-win score below mate and above static evaluation, signed by side and
distance-adjusted. At root, rank every successor by WDL then DTZ so a move is published. Treat a
nominal win as draw when no zeroing move fits the fifty-move allowance; leave file-backed probing
and UCI configuration to TB-001. Exhaustively compare every entry with independent Syzygy WDL/DTZ,
cover color normalization, horizontal reflection, side to move, illegal adjacent kings, checks,
promotions, and clocks around the boundary, and require a WDL-preserving depth-one root move for
every legal state. Repeat a deterministic fresh-process sample with both colors. Keep generation
and verification reproducible.

**Decide:** reject any table mismatch, false fifty-move win, mate-range collision, or WDL-losing
depth-one choice. Retain only with exhaustive agreement and elimination of the prerequisite errors;
never accept an opaque checked-in byte array.

### ENG-025 — Value rooks behind passed pawns

**Start:** tree-changing feature experiment, blocked until ENG-019 and ENG-022 through ENG-024 are
terminal. Because the broad rook sample had no error of at least 50 cp, continue only with at least
20 source groups in every placement cell and five distinct 100+ cp errors where the reference move
improves signed rook placement.

**Try:** for each true passer, inspect its file behind it toward its starting rank. With no
intervening piece, a friendly rook contributes +1 and an enemy rook -1; cap the per-passer sum to
[-1, 1]. Test an endgame unit of 1/8 or 1/4 of the baseline passer bonus at that rank. Exclude side
rooks, checking distance, bridge building, and general rook rules. Apply the shared protocol to
friendly-behind, enemy-behind, neither, and blocked-ray cells, plus a deterministic 96-position
rook-ending control panel with no eligible relation and one position per source group. Test colors,
multiple rooks, intervening pieces, a rook in front, and simultaneous friendly/enemy candidates;
inspect exact term counts and static deltas.

**Decide:** retain the smaller unit only if the focused rule passes and no control move worsens by
50 cp or more. Close as a supported null when the prerequisite discriminator is absent; reject
control regressions, ray errors, or benefit confined to one rank.

### ENG-027 — Recognize exact wrong-bishop rook-pawn draws

**Start:** tree-changing exact-draw experiment, blocked until ENG-021 finds at least five
group-unique king+bishop+rook-pawn versus king errors, or exhaustive enumeration finds at least five
legal drawn states scored 100+ cp from zero or played into a worse WDL class. Otherwise close
unsupported.

**Try:** restrict recognition to king, one bishop, and one a- or h-file pawn against a bare king,
where the bishop cannot control the promotion corner. Normalize color and flank. Generate a boolean
lookup indexed by both kings, bishop, pawn, and side to move; mark only states independently proved
drawn from halfmove clock zero. At non-root main/qsearch entry after rule draws and before
TT/pruning, return draw only for marked states. At root, prefer a legal successor remaining a marked
or rule draw. Do not use a broad heuristic. Exhaustively compare the normalized domain with
independent WDL, requiring zero false draws and complete marked-state coverage. Test color/flank
reflection, corner color, checks, adjacent kings, pawn ranks, both sides, and adjacent material;
require depth-one draw scores for marked states and ensure sampled unmarked wins are not zeroed.
Keep generation and verification reproducible and independent of ENG-026 and TB-001 at runtime.

**Decide:** reject any false draw, material leakage, symmetry mismatch, or missed marked state.
Retain only with exhaustive agreement and elimination of the prerequisite errors.

## Historical results


| ID | Change | Evidence | Result |
|---|---|---|---|
| — | Diagnose budget-sensitive game errors and disable internal LMR | Baseline `ccb718b`, fingerprint 3,507,960; 100 selected OpenBench #32 opening pairs yielded 24 development errors + eight controls and 14 confirmation errors + six controls. Matched 50k/500k/5M nodes; zero persistent quiet repairs (0/8 development, 0/2 confirmation). Confirmation 500k mean loss change −11.6 cp, 95% interval [−31.6, +3.0]; five new large errors at 5M. Local archive: `tools/analysis/output/diagnostic-pilot-ccb718b/` | Scoped null for removing all LMR at matched nodes; sparse quiet confirmation and reduced depth limit inference. Next: force-search d4 versus h3 in development anchor `deb4939117f9cebe` to distinguish choices improved by focused work from unresolved score differences |
| ENG-031 | Add adjacent-file friendly-pawn links at `{MG 13, EG 3}` | Deliberate gate override: exact correctness and reproducibility checks passed; the 200-position tree was 54,039,403 nodes, the fingerprint was 3,507,960 nodes, and `R_time_balanced = 1.0081` passed the tree-changing timing gate. OpenBench #31 Base `cd744df`, Dev `a6f009c`: accepted `[0, 3]` after 19,142 games (`6419-6100-6623`) at LLR +2.9575 and +5.79 +/- 3.69 Elo (95%); PGN `/api/pgns/31/`; focused validation remained sealed; integrated corpus and fingerprint exactly matched the retained candidate | Integrated as `ccb718b` |
| ENG-019 | Reward clear and safe passed-pawn paths | Exact score reconstruction passed; the selected 1/4-rank bonus reduced development 100+ cp errors from nine to four, but validation worsened from four to five with one repair and two new errors | Null; development benefit did not carry to validation |
| ENG-022 | Score passed-pawn races with king distance and tempo | Promotion-catch and king-proximity predicates explained zero and one development errors, respectively; geometry and reconstruction checks passed | Null; motivating discriminator absent |
| ENG-023 | Reward protected and connected passed pawns | Protected and connected true-passer predicates explained zero and one development errors, respectively; geometry and reconstruction checks passed | Null; motivating discriminator absent |
| ENG-002 | Recenter aspiration retries, widen by 1.5x, and reduce only the next non-mate fail-high retry by one ply | Offline `R_node_g` 0.9372, `R_node_total` 0.9607, and `R_time_balanced` 0.9637 with 6/6 wins; OpenBench #30 Base `0ff898c`, Dev `53913f5`: accepted `[0, 3]` after 22,576 games (`7421-7097-8058`) at LLR +2.9549 and +4.99 +/- 3.34 Elo (95%); PGN `/api/pgns/30/`; integrated corpus and 3,293,795-node fingerprint exactly matched the retained candidate | Integrated as `cd744df` |
| ENG-003 | Reuse deterministic static evaluation on exact same-key TT hits with lossless compact metadata packing | Avoided 5,701,544/44,526,905 evaluator calls (12.80%) with zero cached/fresh mismatches; exact static values, 54,533,083-node corpus, 61 sentinels, and 3,798,460-node fingerprint; Release and sanitizer/race checks passed; reviewed as `ec75aae`; `R_time_balanced = 0.9630` with 6/6 wins; integrated corpus exactly matched the retained candidate | Integrated as `7620762`; tree-preserving, so games were skipped |
| — | Use one bounded depth/static-surplus null-move reduction formula | Offline `R_node_g` 0.9397, `R_node_total` 0.9642, and `R_time_balanced` 0.9790; OpenBench #29 Base `c8bfedd`, Dev `54a8989`: accepted `[0, 3]` after 9,060 games at LLR +2.9659 and +10.55 +/- 5.14 Elo (95%); PGN `/api/pgns/29/`; integrated corpus exactly matched all 200 retained signatures | Integrated as `f376cef` |
| ENG-001 | Replace runtime check-danger division with exact function-local constexpr piece/count tables | Complete 128-state domain; exact evaluator checksum, two 5,101,317-node benchmarks, repeated 200-position corpus and 61 sentinel signatures; Release and ASan/UBSan passed; stable `R_time_balanced` 0.9871 with 6/6 wins | Integrated as `3ddf138`; tree-preserving, so games were skipped |
| — | Add one LMR ply to NonPV quiets with combined history at most -1 | Exploration found 169,273 effective opportunities; one complete corpus pass produced `R_node_g` 0.9937 and `R_node_total` 0.9868; benchmark fingerprint 6,014,947 nodes | Null result; missed the 0.9900 smaller-tree gate, so formal offline testing and games were not run |
| — | Replace locked per-node RMW with single-writer relaxed atomic load/store | Exact 5,101,317-node benchmark and 200-position corpus; complete tests, TSan, and 1/2/4-thread risk checks passed; balanced timing ratio 1.0217 with 1/6 wins | Rejected; missed the 0.9925 minimum-speedup threshold |
| — | Target-scoped release IPO/LTO | Exact Clang/GCC 5,101,317-node fingerprints and 200-position corpus; 3/3 CTest; integrated binaries match offline evidence; balanced timing ratio 0.9358 with 6/6 wins | Integrated as `c8bfedd`; tree-preserving, so games were skipped |

OpenBench #31 and #32 used revision `5184c6fde256bf20a085ee089f99c7026b88c43e`.
ENG-019, ENG-022, and ENG-023 summaries were recovered from `a90da10`; their raw measurement
artifacts are not present in this checkout. Git history retains older results.

## Historical findings


Findings apply to the recorded revision, sample, and variants. Revisit a hypothesis with new
causal evidence or a materially changed baseline; a null does not close a whole mechanism.

| Area | Finding and consequence |
|---|---|
| Engine-gap scope | At `0ff898c`, the sampled errors suggested passed-pawn path/race context: six persistent game cases and two of 12 promotion-race cases at least 200 cp worse. The panel found 0/12 rook and 0/12 opposite-bishop cases at least 50 cp worse, no queen case at least 100 cp worse, and no phase-specific evaluation collapse. These small samples did not identify a mechanism in those areas; they do not rule them out. Exact minor/pawn classification remains ENG-021. |
| Razoring | On `cd744df`, the complete depth-10 baseline produced 350,605 cutoffs from 401,943 tries (87.23%). The depth-1 static-evaluation deficit 400--499 shadow-confirmed 25,421/45,712 qsearches at 182,027 qnodes. ENG-009 tested margin 500 to 400: 55 focused checks passed, tries rose to 441,947 with 369,195 cutoffs, and corpus nodes fell from 52,388,829 to 51,647,066 (`R_node_total = 0.985841`), but `R_node_g = 0.991350` missed the then-predeclared 0.9900 gate. The single diagnostic timing ratio 0.8983 was not formal paired timing. No games were run, so this supplies no strength decision; margin 500 remains the baseline. |
| Capture ordering | The tested CaptureHistory implementation reduced nodes but increased total search time. The depth-1 late losing-capture rule reduced `R_node_g` to 0.9883 but raised total nodes to 1.0028 and slowed balanced corpus time to 1.0414 with zero wins in six pairs. These measurements establish cost for those implementations, not a general strength conclusion about capture ordering. The current SEE bands remain the baseline. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. The tested `stand_pat + captured_value + margin <= alpha_before_move` rule at margins 200/300/400 skipped real NonPV cutoffs. The `0ff898c` failure panel found immediate promotions and captures already visible. Those observations did not identify a qsearch-expansion cause in that sample; node share alone does not establish one. |
| LMR | The tested one-ply protection for combined history at least 1024 did not change the sampled fail-lows. The extra reduction at combined history at most -1 reached `R_node_g = 0.9937` and stopped at the then-predeclared smaller-tree gate without games. These results concern those thresholds and trees; they do not establish a general LMR strength conclusion. |
| Clock and limits | The tested next-iteration predictor stopped too early for every sampled multiplier. OpenBench #20 supplied 597,784 score/depth annotations but no clocks, so that audit could not attribute errors to time pressure. New budget hypotheses need per-ply remaining/elapsed time and iteration evidence with revision and time-control metadata. Preserve explicit `movetime` as a hard request. |
| Futility | Guarded reverse futility pruning is retained. Main-search futility must preserve checking quiets by filtering only nonchecking quiets. On `ccb718b`, ENG-010 rejected depth 4 at 700 cp after nine verified beta cutoffs despite `R_node_g = 0.975369`. Its reopened 50 cp trace found 1350 cp was the first threshold with zero verified cutoffs and at least 15,000 triggers. That prototype passed offline, but OpenBench #32 was manually stopped without a decision; see [OpenBench tests](engine-development.md#openbench-tests). Keep the 700 cp cell closed. |
| LMP families | The current depth-2 after-twelve negative-history rule passed OpenBench #27. Unconditional depth-1 pruning caused PV/NonPV disagreement; tested fixed-threshold, history-gated depth-1, and improving-aware variants were not compelling. Revisit LMP only with a materially different safety signal or profiling evidence. |
| TT prefetch | Parent-issued child-cluster prefetch preserved exact corpus and benchmark signatures and reduced balanced corpus time to 0.9823 with 6/6 paired wins. Retain it; games were skipped because the change was tree-preserving. |
| Release IPO | CMake target-scoped Release IPO preserved exact Clang/GCC benchmark and corpus signatures and reduced balanced Clang corpus time to 0.9358 with 6/6 paired wins. Keep IPO on Latrunculi's object library and executables while leaving third-party static libraries and non-Release configurations unchanged. |
| Node accounting | On the tested baseline, single-writer relaxed load/store was 2.17% slower, while worker-local counting with periodic publication was neutral despite removing locked hot-path increments. The current atomic counter remains. A new proposal needs evidence of removable integrated cost. |
| History/picker reuse | The tested history and picker reuse policies produced no repeatable gain under the current timing method. The primary counter-hint lookup remained, and narrow score reuse traded fewer instructions for lower IPC and more branch misses. Revisit only with a proposal that removes common-path work. |
