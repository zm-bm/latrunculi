# Engine Roadmap

Current experiment priorities, based on expected value and effort rather than
predicted Elo gains:

1. Adjust quiet-move reductions using history.
2. Learn static-evaluation corrections.
3. Refine LMR verification policy.

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

The sole evaluator is the frozen [HalfKP NNUE](nnue-halfkp.md). Tablebase and
rule-based endgame investigations remain applicable independently of its weights.

### Syzygy tablebases

Add optional WDL/DTZ probing using user-supplied tablebase files.

### Diagnose endgame errors with tablebases

Compare engine choices with external tablebase WDL/DTZ results to identify
recurring errors in low-material positions, accounting for the fifty-move rule.

### Add a KPK bitbase

Add a compact built-in bitbase for king-and-pawn versus king endings, with
correct side-to-move, promotion, and fifty-move handling.

### Recognize wrong-bishop rook-pawn draws

Recognize drawn king, bishop, and rook-pawn versus king positions using king
placement, bishop color, side to move, and promotion threats.

## Performance and parallelism

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
