# Engine Roadmap

Current priorities:

1. Cache pawn and king evaluation, if profiling supports it.
2. Reduce LMR verification cost, if measurements support it.
3. Recognize candidate passed pawns.

Possible improvements and investigations, grouped by area rather than priority.

## Search

### Reduce LMR verification cost

Investigate whether some late-move-reduction fail-highs can be verified at a
shallower depth without accepting false cutoffs or causing more re-searches later.

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
