# Engine Roadmap

This document is a subsystem-oriented roadmap for the engine. Each section
should describe the current implementation, the boundaries that should be
preserved, and practical improvements to consider next.

## Move Ordering

Move ordering is currently a solid staged baseline. The engine uses one
`MovePicker` interface for main search and qsearch, while `MoveOrdering` owns
the per-worker heuristic state used by the picker and search updates.

### Current State

The main-search move order is:

1. TT move, after pseudo-legal validation.
2. Good noisy moves and promotions.
3. Two killer moves.
4. One countermove hint.
5. Generated quiet moves ordered by history.
6. Bad noisy moves.

When in check, the picker generates evasions instead of the normal staged main
search order. Qsearch uses the same picker interface, but only searches TT/noisy
moves outside check and evasions while in check; it does not use main-search
quiet hints.

Capture ordering is conservative and exact:

- promotions are scored above ordinary captures;
- ordinary captures are classified with `Board::seeMove()`;
- SEE-safe captures are ordered by victim value plus exact SEE score;
- SEE-losing captures remain reachable after quiets in main search;
- qsearch omits SEE-losing noisy moves outside check.

Quiet ordering uses a compact set of refutation and history tables:

- `KillerMoves` stores two quiet beta-cutoff refutations per ply;
- `CounterMoves` stores one quiet reply to the previous move context;
- `QuietHistory` scores quiet moves by side/from/to;
- `ContinuationHistory` adds one previous-move context for generated quiet
  moves;
- `MoveOrdering::Context` caches node-local color and previous-move keys so
  lookup work is not repeated per move.

Quiet-history updates use signed gravity. On a quiet beta cutoff, search rewards
the cutoff quiet, updates killers and countermoves, and applies a conservative
malus to a tiny list of previously searched ordinary quiets. The malus path is
depth-gated, excludes TT and killer quiets, requires at least two failed quiets,
and uses half-strength penalties.

`CaptureHistory` exists as tested table scaffolding, but it is not part of the
active search or picker path. Good/bad quiet splitting is also inactive; current
cutoff-by-index evidence does not show enough late-quiet weakness to justify an
extra picker stage.

### Potential Improvements

Keep the current staged order as the baseline. It is simple, predictable, and
already has the important refutation layers. Future work should prefer measured
search-policy use of the existing history data before adding new picker stages.

Recommended next directions:

- Use quiet and continuation history to tune late-move reductions. The current
  LMR formula only knows whether a move is quiet, a promotion, gives check, or
  is a killer. History scores could reduce less for strong quiets and reduce
  more for poor quiets.
- Add cautious history-based quiet pruning near the leaves. This should be a
  search-policy step, not a move-generation change, and it should preserve the
  first-legal-move safety assumptions used by current pruning.
- Revisit capture history only as a complete capture-ordering experiment:
  capture-history scoring, attempted-capture malus, and lazy threshold SEE need
  to be evaluated together. Exact SEE should remain the active baseline until
  that combined shape produces stable fixed-depth evidence.
- Consider adding a second previous-ply follow-up history only after the
  one-ply continuation table remains useful across broader tests. Avoid a
  family of history tables before there is evidence that the current context is
  saturated.
- Add move-kind-specific stats before another ordering split. Useful counters
  would separate TT, good noisy, killer, counter, generated quiet, and bad noisy
  cutoff sources instead of relying only on global cutoff index buckets.

Avoid for now:

- a standalone good/bad quiet picker split;
- capture-history reads in qsearch;
- copied score constants from other engines;
- pruning or reduction changes bundled with unrelated move-ordering changes.
