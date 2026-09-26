# Engine Development

This document tracks work intended to improve how Latrunculi plays, followed by the shared rules
for testing search changes and the durable findings. [OpenBench](openbench.md) defines game testing,
the [measurement guide](../tools/measurements/README.md) documents commands and formats, and the
[evaluation tuning workflow](../tools/tuning/workflow.md) covers linear HCE fitting.

Record decisions, not command histories. Keep artifacts for the current baseline and live
candidates; summarize terminal results here and remove their output unless it remains useful.

The `Next` field names the only action needed to advance a candidate:

| Next | Skill |
|---|---|
| `explore` | [`explore-latrunculi-change`](../.agents/skills/explore-latrunculi-change/SKILL.md) |
| `test offline` | [`test-latrunculi-candidate`](../.agents/skills/test-latrunculi-candidate/SKILL.md) |
| `submit OpenBench` | [`submit-latrunculi-openbench`](../.agents/skills/submit-latrunculi-openbench/SKILL.md) |
| `check OpenBench #ID` | [`check-latrunculi-openbench`](../.agents/skills/check-latrunculi-openbench/SKILL.md) |
| `integrate` | [`integrate-latrunculi-candidate`](../.agents/skills/integrate-latrunculi-candidate/SKILL.md) |

Keep an existing `SW-XX` or `ENG-XXX` ID throughout its life. Assign a new `ENG-XXX` only when new
work becomes a retained candidate or unresolved queue item; casual and null exploration stays
unnumbered. Keep one local CPU-sensitive task and one OpenBench test active at a time. Tree-changing
work requires games; tree-preserving work may skip them only when the evidence below passes. Unless
a request authorizes several named actions, each skill stops at its own boundary.

## Baseline

| Field | Current value |
|---|---|
| Engine | `0ff898c` (behavior-equivalent algorithm-detail cleanup after SW-17) |
| OpenBench fingerprint | 3,798,460 nodes |
| Cached search corpus | `tools/measurements/output/search-baseline-0ff898c/` |

Refresh the cached corpus only after an approved integration changes the engine baseline, search
workload, or measurement meaning.

## Candidates

| ID | Change | Kind | Evidence | Next |
|---|---|---|---|---|
| ENG-002 | Recenter aspiration retries, widen by 1.5x, and reduce only the next non-mate fail-high retry by one ply | Tree-changing | Exact corpus `R_node_g = 0.9372`, `R_node_total = 0.9607`; `R_time_balanced = 0.9637` with 6/6 wins; OpenBench #30 Base `0ff898c`, Dev `53913f5` | check OpenBench #30 |

## Queue

These are evidence-gated leads, not prequalified candidates. The bands reflect approximate expected
playing-strength value from current evidence, plausible ceiling, breadth, tractability, and overfit
risk; they are neither Elo estimates nor a strict execution order. Named prerequisites remain
authoritative. Explore one item at a time against the then-current baseline, never stack unresolved
changes, and remeasure evidence affected by intervening integrations.

Every item uses the same three fields:

- **Start** states its classification, evidence, readiness, and prerequisites.
- **Try** defines the bounded measurement or isolated implementation and its focused checks.
- **Decide** defines retain, reject, null, and unresolved outcomes.

A numbered item may be a feature experiment, diagnostic, or measurement. For measurement-first
speed work, stop before prototyping when the measured upper bound cannot plausibly clear the
tree-preserving timing gate. For measurement-first tree-changing work, prototype at most one policy
from the named discriminator; a missing discriminator is a terminal null result. Here, **retain**
means retain one candidate for the applicable standard offline and game gates below, not authorize
integration.

ENG-003 through ENG-018 derive from the 2026-09-25 audit of `c8bfedd` (SW-14): the 200-position
depth-10 corpus and the eight depth-12 positions now in
[`search-profile.epd`](../tools/measurements/search-profile.epd), with one thread and 32 MiB Hash.
Counts describe that fixed tree and sampled cycle shares are approximate; remeasure affected
evidence before retaining a candidate. Source-only comparisons used Ethereal `0e47e9b`, Stockfish
`86f1df7`, and Minic `4317c14`; their mechanisms do not establish transferable constants.

ENG-019 through ENG-031 derive from the 2026-09-26 engine-gap audit of `0ff898c`, retained under
`tools/measurements/output/engine-gap-audit-0ff898c/`: 96 group-unique stratified endgames, a
512-position phase-balanced evaluation panel, and 43 deeply checked earliest game deteriorations.
The artifact is useful provenance but is not required to resume a task. Stockfish fixed-node scores
are approximate references; the audit had no local WDL/DTZ tablebases and its PGNs had no clock
annotations. Its null results exclude broad rook-ending or opposite-bishop scaling, generic qsearch
expansion, generic king-activity terms, and global evaluation rescaling.

**Shared focused-panel protocol**

Use the current development dataset from the evaluation-tuning workflow, with a full FEN and source
group for every row. Canonicalize by the first four FEN fields, retain at most one position per
source group, and rank otherwise-equivalent choices by SHA-256 of the task ID, source group, and
canonical FEN. Assign a group to validation when the unsigned value of the first eight hex digits
of SHA-256 of its group ID modulo five is zero; all others are development. Freeze selection and
the split before implementation. If no dataset is retained, regenerate it through the tuning
workflow and record its engine revision and content hash first.

Select at most 256 positions per task, balancing its named cells and relative pawn ranks as evenly
as availability permits. A comparison cell requires at least 20 source groups, and error
prerequisites count distinct source groups. Analyze the baseline at 50k and 500k requested nodes
with one thread, 32 MiB Hash, and fresh state. Use one fixed, recorded Stockfish binary at 1M nodes,
one thread, 32 MiB Hash, and MultiPV 3. If Latrunculi's move is outside MultiPV 3, search its
successor separately at the same Stockfish limit and convert the score to the root perspective.
Choose a variant on development only, freeze it, and inspect validation once.

Unless a task supplies an exact-WDL rule, a focused pass requires fewer validation moves with at
least 100 cp reference loss, no new validation error of that size, and no worse validation median
move loss. It only qualifies a candidate for the standard gates. If required cells or errors are
too sparse, close unresolved with the observed counts instead of weakening the gate.

**Higher potential**

### ENG-019 — Reward clear and safe passed-pawn paths

**Start:** tree-changing feature experiment; runnable first. Latrunculi values a true passer by
relative rank alone. Six persistent game errors and two 200+ cp errors in a 12-position
promotion-race sample implicate path context, while qsearch already sees immediate promotions.

**Try:** keep the current passer definition. A path is clear when every square through promotion is
empty; a next push is safe when its forward square is on board and absent from the complete enemy
attack map after hypothetically making the push. Award an endgame-only bonus only when both hold,
testing 1/8, 1/4, and 1/2 of the baseline endgame passer bonus for that relative rank. Exclude king,
support, candidate-passer, rook-placement, and extension logic, and require reconstructed evaluation
to reproduce the term exactly. Apply the shared protocol to relative ranks three through six with
clear-and-safe, clear-but-attacked, and blocked cells. Unit-test both colors, all enemy attack
classes, occupied path and next squares, discovered slider attacks after the hypothetical push,
and promotion-edge geometry; compare static deltas and 50k/500k move loss.

**Decide:** retain the smallest amplitude passing the focused rule. Reject all variants if benefit
is development-only, a blocked or attacked passer receives the bonus, or reconstructed evaluation
diverges.

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
unavailable. A repair enters Stockfish MultiPV 3 or falls below 50 cp loss. Record repairs, new
large control errors, nodes, and node multipliers.

**Decide:** implicate a family only when it uniquely repairs at least three persistent source
groups, creates no control error of at least 100 cp, and completes equal-depth searches for at least
80% of both panels. Route LMR evidence to ENG-013, PVS consequences to ENG-014, or create one new
bounded null/forward-pruning experiment naming the repaired stratum. If the panels are too small,
close unresolved; if no family qualifies, record a null result. Never retain a disabled-pruning
build or blend families.

### ENG-003 — Reuse static evaluation on same-key TT hits

**Start:** direct tree-preserving speed experiment. Evaluation consumes about 39--40% of search
cycles and the audited tree contained 8.07M exact-key repeats; all three reference engines retain a
static evaluation in their TT entries.

**Try:** store and reuse the deterministic static evaluation in the existing TT payload without
enlarging its 16-byte entry or 64-byte cluster. Prototype one compact bound/generation packing,
count avoided evaluator calls, and verify full-range encode/decode, replacement and aging behavior,
the existing race contract, exact reused values, and exact corpus and benchmark signatures.

**Decide:** retain only if entry and cluster sizes, semantics, values, and signatures remain exact
and paired integrated timing passes the tree-preserving gate. Reject on a packing or race
regression, changed search behavior, negligible avoided evaluation, or absent repeatable speedup.

### ENG-022 — Score passed-pawn races with king distance and tempo

**Start:** tree-changing feature experiment, blocked until ENG-019 is terminal. Rebuild its panel
and continue only with at least five distinct residual errors of at least 100 cp where the baseline
move's resulting race score is at least two buckets worse than the reference move's.

**Try:** apply the term only to a true passer on relative ranks three through six with an empty path
to promotion. Let `pushes` be its remaining single-square pushes, `enemy` and `friendly` the kings'
Chebyshev distances to the promotion square, and `tempo` one when the defender moves next and zero
otherwise. Define `race = clamp(enemy - pushes - tempo + clamp(enemy - friendly, -1, 1), -3, 3)`
and test coefficients 8, 16, and 24 cp. A blocked path scores zero. Exclude path-attack, support,
rook-placement, and extension logic. Apply the shared protocol across populated race buckets, side
to move, color, and relative rank, excluding immediate promotions and forcing captures. Unit-test
mirroring, distance, tempo reversal, clamping, blocked paths, and sign; inspect whether repairs are
the predicted pawn or king moves.

**Decide:** retain the smallest coefficient passing the focused rule and repairing at least two
prerequisite errors. Reject if direction changes by side or color, only immediate tactics improve,
or every coefficient creates a new large error. Do not stack ENG-019 unless it is already baseline.

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
same-order bucket or partial-selection structure for that stage only. This is not the rejected
SW-20 score/history reuse. Compare the complete emitted sequence on targeted and randomized legal
positions, then require exact corpus and benchmark signatures and measure picker branches/misses
and paired integrated time.

**Decide:** close null if no dominant removable scan exists. Retain only when move order and search
trees are exact and timing passes; reject if bookkeeping shifts equivalent cost elsewhere, changes
order, or lacks repeatable speedup.

**Promising but lower-confidence or smaller**

### ENG-010 — Test one broader main-search futility cell

**Start:** measurement-first tree-changing search experiment. Futility activates at 13.2% of
6.79M eligible nodes and reaches a real nonchecking-quiet trigger 626,672 times, indicating room for
one adjacent cell but not establishing its safety.

**Try:** shadow adjacent depth and margin cells while retaining all mate, check, first-move, and
checking-quiet protections. For each proposed skip, use a bounded verifier to record whether the
quiet later raises alpha or cuts off. Select at most one cell from the measured distribution, then
prototype only that cell and record trigger, unsafe-skip, node, and objective-result changes on the
complete corpus.

**Decide:** close null if no populated cell has a stable safety discriminator. Retain only if the
chosen cell removes no meaningful alpha raise or cutoff, passes objective checks and timing, and
meets the standard fixed-depth smaller-tree gate; otherwise reject.

### ENG-013 — Stratify LMR re-searches before testing adaptive verification depth

**Start:** measurement-first tree-changing search experiment. Only 25,959 of 7.58M reductions fail
high, but their inclusive full-depth footprint is 16.44M nodes and just 58.7% remain above alpha.

**Try:** split reduced fail-highs by PV/NonPV, reduction, depth, fail-high margin, and full-depth
outcome. Continue only if one repeatable stratum dominates avoidable verification work, then test
one intermediate or result-conditioned verification depth derived from that stratum. Do not retry
SW-06's full-depth null-window scout or the rejected history thresholds. Record initial reduced
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

### ENG-009 — Test one broader razoring eligibility cell

**Start:** measurement-first tree-changing search experiment. Razoring activates at only 3.9% of
10.75M structurally eligible nodes but confirms 87.5% of its 421,499 tries.

**Try:** shadow adjacent depth and margin cells selected from the observed score distribution and
verify each proposed razor with the existing qsearch before changing behavior. Choose at most one
cell with material opportunity and retained confirmation quality, then prototype only that cell
while recording tries, confirmations, qsearch cost, objective results, and total tree work.

**Decide:** close null if adjacent cells are sparse or confirmation quality degrades materially.
Retain only if the selected cell adds confirmed cutoffs at small qsearch cost, passes objective and
timing checks, and meets its predeclared smaller-tree gate; otherwise reject.

### ENG-023 — Reward protected and connected passed pawns

**Start:** tree-changing feature experiment, blocked until ENG-019 and ENG-022 are terminal.
Continue only with at least five distinct residual errors of at least 100 cp where the reference
move preserves or creates a higher support state than the baseline move.

**Try:** a true passer is protected when its square is attacked by a friendly pawn and connected
when another friendly true passer is on an adjacent file within one relative rank. Score neither as
zero, protected-only and connected-only as one unit, and both as two. Test an endgame-only unit of
1/8 or 1/4 of the baseline passer bonus at that rank; exclude path, king-distance, candidate,
rook-placement, and extension logic. Apply the shared protocol with separate state cells and at
least 20 source groups in every cell used. Unit-test both colors, file edges, rank separation,
non-passed supporters, doubled pawns, and a pawn qualifying both passers; verify exact term counts
and reconstructed evaluation.

**Decide:** retain the smaller unit only if the focused rule passes and at least one support cell
independently improves on validation. Reject if unsupported cells receive a value, benefit is
confined to immediate promotions or one group, or the combined cell improves while both individual
states regress.

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

**Start:** measurement-first tree-changing search experiment. Reference engines use same-side
prior-ply static trends, but that is not evidence for a Latrunculi constant and the prior
improving-aware LMP variants remain closed.

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

### ENG-031 — Extend a safe passed-pawn push to the seventh rank

**Start:** tree-changing search experiment, blocked until ENG-019 and ENG-022 are terminal. On
their residual panel, compare the 500k completed depth with one additional completed depth.
Continue only if at least three distinct 100+ cp cases repair below 50 cp or enter MultiPV 3 and the
repaired PV contains the trigger move within four plies.

**Try:** extend exactly one ply after a non-capturing, non-promoting true-passer move landing on
relative rank seven. Require an empty promotion square and, after the move, a destination absent
from the complete enemy attack map. Permit one such extension per path and change no passer
evaluation, other extension, reduction, pruning, or ordering rule. Run baseline/candidate at
50k/500k on all qualifying cases and an equally sized hash-selected control set that did not repair
with another depth. Record triggers, extensions, repairs, and nodes. Test both colors, captures,
promotions, occupied promotion squares, every enemy attack class after make/unmake, and the path
limit. On the full corpus require no objective regression and geometric-mean node ratio at most
1.05.

**Decide:** retain only if at least three prerequisite cases repair, no control becomes a new 100+
cp error, the node cap and focused rule pass, and the trigger explains the depth repair. Reject if
the same positions retain a static-evaluation sign error.

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

## Recent results

| ID | Change | Evidence | Result |
|---|---|---|---|
| SW-17 | Use one bounded depth/static-surplus null-move reduction formula | Offline `R_node_g` 0.9397, `R_node_total` 0.9642, and `R_time_balanced` 0.9790; OpenBench #29 Base `c8bfedd`, Dev `54a8989`: accepted `[0, 3]` after 9,060 games at LLR +2.9659 and +10.55 +/- 5.14 Elo (95%); PGN `/api/pgns/29/`; integrated corpus exactly matched all 200 retained signatures | Integrated as `f376cef` |
| ENG-001 | Replace runtime check-danger division with exact function-local constexpr piece/count tables | Complete 128-state domain; exact evaluator checksum, two 5,101,317-node benchmarks, repeated 200-position corpus and 61 sentinel signatures; Release and ASan/UBSan passed; stable `R_time_balanced` 0.9871 with 6/6 wins | Integrated as `3ddf138`; tree-preserving, so games were skipped |
| SW-16 | Add one LMR ply to NonPV quiets with combined history at most -1 | Exploration found 169,273 effective opportunities; one complete corpus pass produced `R_node_g` 0.9937 and `R_node_total` 0.9868; benchmark fingerprint 6,014,947 nodes | Null result; missed the 0.9900 smaller-tree gate, so formal offline testing and games were not run |
| SW-15 | Replace locked per-node RMW with single-writer relaxed atomic load/store | Exact 5,101,317-node benchmark and 200-position corpus; complete tests, TSan, and 1/2/4-thread risk checks passed; balanced timing ratio 1.0217 with 1/6 wins | Rejected; missed the 0.9925 minimum-speedup threshold |
| SW-14 | Target-scoped release IPO/LTO | Exact Clang/GCC 5,101,317-node fingerprints and 200-position corpus; 3/3 CTest; integrated binaries match offline evidence; balanced timing ratio 0.9358 with 6/6 wins | Integrated as `c8bfedd`; tree-preserving, so games were skipped |

Git history retains older results.

## How candidates are tested

These are the default offline rules for search and search-speed candidates. A candidate records
only a justified override, a different mechanism check, or an extra risk test before measurement.

### Evidence and classification

| Panel | Purpose and use |
|---|---|
| Objective tests | Source-pinned mate and material checks plus legality and crash detection. These decide correctness, not performance. |
| Tactical corpus | All 200 Arasan positions in `tools/measurements/search.epd`; use for deterministic pruning, reductions, move ordering, aspiration, and comparable search changes. Source `bm`/`am` labels are context, not oracles. |
| Sentinels | Four cases in `tools/measurements/search-sentinels.epd`; use for descriptive trajectory diagnostics, not as move-quality oracles. |
| Benchmark fingerprint | Six fixed positions at depth 13; use its deterministic aggregate for OpenBench compatibility, not as a representative search-selection panel. |

Profiles locate CPU work, fixed-depth nodes measure selectivity, paired fixed-depth search time
measures integrated efficiency, and paired games decide playing strength. Keep those axes separate.
Perft or qsearch shares, cutoff yield, and observational correlations do not provide a
counterfactual, so they are hypothesis generators rather than change or acceptance gates.
Cross-engine depth and NPS are context only.

Fresh-process, one-thread repeats of the same build and inputs must agree in completed depth,
static score, searched score, actual nodes, best move, and PV; exclude timing and NPS. Differences
between a tree-changing candidate and its baseline are expected and carry no default judgment.

Call a candidate **tree-preserving** only when a code- or build-level argument shows that it
preserves search decisions by construction. Exact corpus and benchmark signatures support but do
not prove that claim. Otherwise it is tree-changing and requires paired games before integration.

Objective failures include illegality, crashes, and lost or delayed source-pinned mate or material
solutions. Unresolved PV/NonPV disagreement within one build blocks an offline pass, but is not
incorrect chess without objective evidence.

### Cheap checks and full offline testing

Classify each retained search candidate before formal offline testing:

- **Tree-changing:** Cheap checks cover the mechanism and objective results, one fresh-process pass
  over the complete corpus at depth 10, one thread, one repetition, and 32 MiB Hash, plus one
  benchmark fingerprint. Full testing adds the complete release suite, a reproduced fingerprint,
  required risk checks, and paired corpus timing.
- **Tree-preserving:** Cheap checks require the equivalence argument and exact baseline corpus and
  benchmark signatures. Full testing adds the complete release suite, required risk checks, and
  paired corpus timing, which is the performance gate.

The first candidate timing pass supplies the fresh corpus repeat. A benchmark, corpus subset, or
exploratory measurement cannot replace the formal complete-corpus check. Apply these defaults
without restating them in a separate declaration or manifest.

Run deterministic, objective, fingerprint, and risk checks first, followed by the release suite,
trajectory diagnostics, required sanitizers, and finally paired timing.

### Extra risk tests

The default corpus is cold, one-threaded, and fixed-depth; the runner clears search heuristics and
the transposition table before every position. Add an extra test only for behavior outside that
model:

- **Retained search state:** clearing, persistence, aging or generation, replacement, reuse, or behavior that depends on prior searches.
- **Clock and limits:** budget calculation, elapsed-time checks, stopping or polling, limit precedence or overshoot, or publication of a stopped or incomplete iteration.
- **Threading:** shared state, worker ownership or lifecycle, synchronization, aggregation or publication, resizing, stop/restart behavior, or worker-result selection.

Merely reading history or the TT, consulting a clock, or running in threaded code does not require
an extra test. State its risk, initial sequence, pass condition, repetitions, and sanitizer need;
use the smallest existing test or probe that establishes the claim.

### Sentinel trajectories

**Latest confirmed stable-window depth** is the greatest depth `d >= 3` for which `d-2..d` keep the
same root move and score class, both adjacent PV transitions share a nonempty prefix, and objective
predicates hold. It describes a horizon-sensitive trajectory, not move quality or playing strength.

A lower stable-window depth, late root change, A-B-A event, or zero-prefix transition is diagnostic,
not a default rejection.

If a fixed horizon ends immediately after a new line appears and the flag matters to the decision,
the candidate may state before measurement a bounded confirmation that extends only that case for
both builds by at most two depths. A single flagged case cannot reject a candidate. Sentinel hard
failures are limited to objective failure or same-build nondeterminism. A task specifically about
convergence may define another aggregate measurement gate, but it must measure repeated instability
rather than agreement with the baseline move, score, or PV.

### Corpus metrics

Keep three reported axes separate. For each position `i`, let
`r_i = candidate_nodes_i / baseline_nodes_i`:

- `R_node_g = exp(mean(log(r_i)))`, the primary equal-position selectivity ratio; when reduced
  fixed-depth tree size is the claimed effect, the default gate is `R_node_g <= 0.9900`;
- `R_node_total = sum(candidate_nodes) / sum(baseline_nodes)`, the workload-weighted tree-size
  ratio; always report it as context, and use it as an additional gate only when the candidate
  records that extra gate before measurement;
- `R_time = sum(candidate_total_ns) / sum(baseline_total_ns)` for each timing pair, the aggregate
  measured search-time ratio and full-test timing metric.

Retain signature differences and useful node diagnostics, but do not make them default gates. Cost
per node and NPS may aid investigation, but neither is the timing result; never combine node and
time evidence into one score.

Use `R_node_g <= 0.9900` for hypotheses claiming a smaller fixed-depth tree. A candidate that
intentionally spends more nodes instead states an observable mechanism check, keeps the full timing
budget, and uses paired games to decide strength.

Tree-changing work must pass its mechanism and timing gates; timing within the allowed range is
neutral. Tree-preserving work must pass its timing gate. A timing gain cannot rescue a failed
claimed effect; test that benefit as another candidate.

### Paired timing

Use separate equivalent binaries built with the same compiler and preset on the same machine. Pin
both to the same physical core, avoid its sibling, and use fresh processes with identical options.
One complete 200-position invocation is one replicate. Discard one warm-up per binary, then run six
adjacent pairs as one uninterrupted batch: `BC, CB, BC, CB, BC, CB` (`B` is baseline, `C` candidate).

Let `r_1..r_6` be the aggregate candidate-to-baseline time ratios. Treat adjacent opposite-order
pairs as three `B-C-C-B` blocks and compute
`b_j = sqrt(r_(2j-1) * r_(2j))` for `j` in `1..3`. The timing decision metric is
`R_time_balanced = median(b_1, b_2, b_3)`.

All baseline signatures and all candidate signatures must agree. Retain the helper's pair, block,
order, range, and win diagnostics, but only `R_time_balanced` is a gate. Balancing order does not
make an invalid or unstable environment valid.

Tree-changing work passes at `R_time_balanced <= 1.0100` (at most 1% slower); tree-preserving work
passes at `R_time_balanced <= 0.9925` (at least 0.75% faster). Record a justified exception before
measurement.

A/A timing is an optional environment or anomaly diagnostic, not a default gate or a correction to
candidate ratios.

A single pass, cached-versus-live timing, an incomplete panel, or an unisolated run is diagnostic
only. If conditions are not stable, defer timing or use an idle machine; do not alter an external
workload without authorization.

Tree-changing work need not improve timing within its allowance. Missing the applicable threshold
is a failure. Mark a test unresolved only for an identified setup or collection failure or an
unavailable required condition; candidate-caused nondeterminism fails. Rerun only after an
identified setup failure, and report effects no more precisely than the replicate spread supports.

### Integration checks

Before refreshing the operational baseline, require the integrated build's complete corpus
signatures and applicable benchmark fingerprint to match the retained offline- or game-tested
candidate. Change identity and tests remain necessary but do not substitute for this search
identity check.

## Durable findings

| Area | Finding and consequence |
|---|---|
| Engine-gap scope | At `0ff898c`, passed-pawn path/race context was the only specific actionable endgame weakness: six persistent game cases and two of 12 promotion-race cases at least 200 cp worse. The balanced panel found 0/12 rook and 0/12 opposite-bishop cases at least 50 cp worse, no queen case at least 100 cp worse, and no phase-specific evaluation collapse; exact minor/pawn classification remains ENG-021. Keep broad rook, opposite-bishop, or queen scaling, generic king-activity terms, and global evaluation rescaling closed unless exact or group-disjoint held-out evidence changes those results. |
| Capture ordering | CaptureHistory reduced nodes but increased total search time. SW-11's depth-1 late losing-capture rule reduced `R_node_g` to 0.9883 but raised total nodes to 1.0028 and slowed balanced corpus time to 1.0414 with zero wins in six pairs. Preserve the current SEE bands and do not retry that exact rule. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. Do not retry `stand_pat + captured_value + margin <= alpha_before_move` with the tested 200/300/400 margins; they skipped real NonPV cutoffs. The `0ff898c` failure panel found immediate promotions and captures already visible, so qsearch node share or endgame misses do not justify generic expansion without a bounded new discriminator. |
| LMR | The tested one-ply protection for combined history at least 1024 did not change the sampled fail-lows. SW-16's extra reduction at combined history at most -1 reached `R_node_g = 0.9937`, short of the smaller-tree gate. Do not retry either exact shape; different formulas or re-search sequencing require new evidence. |
| Clock and limits | The tested next-iteration time predictor stopped too early for every sampled multiplier. Preserve explicit `movetime` as a hard request. OpenBench #20 supplied 597,784 score/depth annotations but no clock comments, so the audit supports no time-pressure or adaptive-budget task. Revisit only with per-ply remaining and elapsed time plus revision, worker scale, and time-control metadata; predeclare stable/unstable root bins before analysis. |
| Futility | Guarded reverse futility pruning is retained. Main-search futility must preserve checking quiets by filtering only nonchecking quiets. |
| LMP families | The current depth-2 after-twelve negative-history rule passed OpenBench #27. Unconditional depth-1 pruning caused PV/NonPV disagreement; tested fixed-threshold, history-gated depth-1, and improving-aware variants were not compelling. Revisit LMP only with a materially different safety signal or profiling evidence. |
| TT prefetch | SW-13's parent-issued child-cluster prefetch preserved exact corpus and benchmark signatures and reduced balanced corpus time to 0.9823 with 6/6 paired wins. Retain it; games were skipped because the change was tree-preserving. |
| Release IPO | SW-14's CMake target-scoped Release IPO preserved exact Clang/GCC benchmark and corpus signatures and reduced balanced Clang corpus time to 0.9358 with 6/6 paired wins. Keep IPO on Latrunculi's object library and executables while leaving third-party static libraries and non-Release configurations unchanged. |
| Node accounting | Single-writer relaxed load/store was 2.17% slower, while worker-local counting with periodic publication was neutral in exploration despite removing locked hot-path increments. Retain the current atomic counter. Revisit only if profiling on a future baseline identifies node accounting as a material bottleneck or a compiler or architecture change gives a concrete reason to retest. |
| SW-20 throughput | Reusing SW-20 history or picker work produced no repeatable gain under the current timing method. The primary counter-hint lookup remained, and narrow score reuse traded fewer instructions for lower IPC and more branch misses. Revisit only with a proposal that removes common-path work. |

## Search guardrails

Borrow mechanisms, not code or tuning constants, from pinned engines or the Chess Programming Wiki;
revalidate them in Latrunculi. Do not add machinery merely to raise displayed depth. Profile before
hot-path or architectural work, and add search state only for a concrete consumer.
