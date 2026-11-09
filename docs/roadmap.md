# Engine Roadmap

Current experiment priorities, based on expected value and effort rather than
predicted Elo gains:

1. Adjust quiet-move reductions using history.
2. Learn static-evaluation corrections.
3. Refine LMR verification policy.
4. Cache pawn and king evaluation, if profiling supports it.
5. Investigate and tune king danger, guided by game mistakes.
6. Recognize candidate passed pawns.

Give caching a short feasibility investigation early and promote it if the
measurements show substantial savings. Revisit this order as game analysis and
experiments identify concrete weaknesses.

Possible improvements and investigations, grouped by area rather than priority.

## Search

### Adjust quiet-move reductions using history

Quiet history currently affects ordering and shallow pruning, while capture
history also adjusts LMR. Explore using quiet-history scores to reduce promising
late quiet moves less and poor ones more. Compare the decisions in Weiss and
Ethereal, adapting the adjustment to Latrunculi's history scale. Measure missed
moves, re-search cost, and playing strength; a lower re-search rate alone does
not establish an improvement.

### Learn static-evaluation corrections

Explore correction history: learn recurring differences between static
evaluation and search results, then use those corrections in evaluation-based
search decisions. Weiss provides a reference implementation. Keep cached raw
evaluations distinct from learned corrections and account for search bounds
when selecting updates. Test whether better pruning decisions justify the
added state and runtime cost.

### Refine LMR verification policy

At PV nodes, a reduced search that beats alpha currently proceeds directly to
a full-depth, full-window search. Test an intermediate full-depth, null-window
verification to determine whether it filters out enough unsuccessful attempts
to justify the extra search. Ethereal provides a reference implementation.

Separately, explore adapting verification depth to the reduced search result.
Measure decision quality and total cost, including false cutoffs and later
re-searches. Test independently of quiet-history LMR changes and reassess after
those changes, since they alter which moves need verification.

### Tune SEE and capture-ordering values

Investigate the fixed exchange prices in
[`see_values.hpp`](../src/board/see_values.hpp). They affect quiescence capture
filtering, main-search capture staging, and capture ordering, including evasions.
Changing them also changes the balance between material and capture history.
They are separate from the material weights fitted by the evaluation tuner.

Keep pawn=100 and constrain the other prices to preserve the fixed attacker
order: pawn, knight, bishop, rook, queen, with king handling unchanged. Start
with a small, bounded comparison of price sets after capture/search policy
stabilizes, holding history scaling and threshold formulas fixed. Use game
results to assess strength; the current evaluation fit cannot optimize these
search decisions. Check exchange correctness and changed capture decisions,
then independently confirm any selected values. Consider SPSA if a broader
tuning run justifies exposing the parameters through UCI.

### Refine deferred-capture placement

Explore whether deferred captures should precede quiet moves with poor history.
A zero-score split increased cold search cost without a clear warm-search
benefit. A better boundary or rationale is needed before another trial; the
policy has not been strength-tested.

### Clarify search completion ownership

Review completion across `Worker::search()`, `prepare_final_result()`, and
`Thread::idle_loop()`. Make helper shutdown, result selection, reporting, and
the transition to idle easier to follow together. Preserve ponder/infinite
waits and final publication under the thread state lock before idle becomes
visible. Require a concrete ownership improvement before restructuring.

### Hide thread-pool implementation details

`thread_pool.hpp` exposes the full `Thread` definition and includes `Worker`,
bringing search internals into UCI-facing headers. Consider hiding these
implementation dependencies while retaining the pool interface. Account for
tests that inspect threads directly; avoid extra allocations or runtime dispatch.

### Separate the TT interface from its storage

Keep records, probe/store operations, and the writer handle accessible to
consumers, while encapsulating `TTEntry`, `TTCluster`, and generation encoding.
Preserve compact layout, atomic access, replacement semantics, and hot-path cost.

## Evaluation and endgames

### Investigate and tune king danger

The nonlinear king-danger calculation has internal weights for attackers,
checks, weak defenses, and pins that the current linear tuning feature set
does not expose. Investigate recurring attack and defense errors, then test a
focused change to the model or its weights. Choose a fitting method compatible
with the nonlinear calculation, verify compiled evaluation behavior, and
confirm gains on held-out data and in games.

### Syzygy tablebases

Add optional WDL/DTZ probing using user-supplied tablebase files.

### Diagnose endgame errors with tablebases

Compare engine choices with external tablebase WDL/DTZ results to identify
recurring errors in low-material positions, accounting for the fifty-move rule.

### Recognize candidate passed pawns

Explore a bonus for pawns that become passed after another friendly pawn
exchanges away their only enemy blocker. Distinguish feasible breakthroughs
from unsupported pawns or multiple blockers.

### Evaluate rooks behind passed pawns

Explore evaluation terms for friendly and enemy rooks behind passed pawns,
accounting for intervening pieces and whether the rook supports or restrains
advancement.

### Add a KPK bitbase

Add a compact built-in bitbase for king-and-pawn versus king endings, with
correct side-to-move, promotion, and fifty-move handling.

### Recognize wrong-bishop rook-pawn draws

Recognize drawn king, bishop, and rook-pawn versus king positions using king
placement, bishop color, side to move, and promotion threats.

### Improve evaluation of drawish minor-piece endings

Investigate scaling for minor-piece endings with blocked pawns or few pawns on
one flank. Treat bishop and knight endings separately, account for other draw
effects, and preserve winning positions.

### NNUE

Explore a neural evaluation backend while retaining the handcrafted evaluation.

## Performance and parallelism

### Cache pawn and king evaluation

Cache reusable pawn-structure and king-shelter terms per worker. Include all
relevant pawn, king, and castling state in the key and preserve exact scores.
Measure the cacheable share of total search time and the savings after lookup
and memory costs. Limit cached king terms to those determined by the key;
king danger also depends on other pieces and their attacks. Confirm unchanged
search behavior and a net runtime gain before treating caching as an improvement.

### Reduce SEE cost and unnecessary calls

Capture ordering uses material and history scores, with threshold SEE deferred
until selection. Capture history also adjusts main-search SEE thresholds and
LMR; capture evasions use material scoring without SEE.

Profile the remaining exchange-loop cost before attempting further rewrites.
Avoid scoring the already-searched TT move when it cannot be selected again.
Preserve ordering and ties, inclusive thresholds, promotion and en passant
handling, king-recapture rules, and the existing non-king pin model.

### Reduce move-picker scanning

Reduce repeated scanning when selecting quiet moves and deferred captures,
preserving move order, scoring-time history snapshots, and ties. The selector
already compares scores before testing eligibility; profile the remaining scans
before adding sorting or cached selection state.

### Store evaluation-only TT entries

Explore storing static evaluations without searched bounds. Weigh saved
evaluation work against extra table traffic and displacement of search entries;
evaluation-only records must never justify search cutoffs.

## Interfaces and development

### UCI features

Add MultiPV, score-bound and current-move reporting, Chess960, and optional
strength controls.
