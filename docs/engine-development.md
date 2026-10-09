# Engine Development

The current work board for improving Latrunculi.

Keep each experiment's question, decision, next action, and revision/evidence
links here. Preserve experiment IDs; keep detailed results in linked reports
and PRs. Follow the shared [candidate rules](../.agents/references/candidate-rules.md).

## Baseline

| Field | Current value |
|---|---|
| Engine | [`e9bc6c9`](https://github.com/zm-bm/latrunculi/commit/e9bc6c905860cccee39e66ed7086ebd27248157e) (ENG-018: reduce declining late quiet moves more) |
| OpenBench fingerprint | 3,184,133 nodes |
| Search corpus | `tools/analysis/output/search-baseline-e9bc6c9/`; 200 positions, depth 10, one thread, 32 MiB Hash; 52,202,177 nodes |
| Build | `release-dev`, GCC 15.2.0, x86-64, `gazelle`; refreshed 2026-10-08 |

Rechecked 2026-10-09 for [1.1.0](releases/1.1.0.md): the local version bump
passed Release tests and reproduced the baseline's benchmark and corpus
signatures. Engine behavior is unchanged. Sanitizer, timing, and game-acceptance
evidence remains in the [integration report](../tools/analysis/output/eng-018-integration/report.md).

## Issues and leads

ENG-037/038 are lower-priority follow-ups to the
[TT audit at d3116b1](../tools/analysis/output/tt-design-audit-d3116b1/report.md).

### ENG-032 — Explain persistent quiet-move disagreements

- **Question:** does the baseline under-search alternative quiet root moves, or
  does the score disagreement persist after focused work?
- **Evidence:** on `ccb718b`, anchor `deb4939117f9cebe` chooses h3 at 50k, 500k,
  and 5M nodes; a reference MultiPV report favors d4 by 125 cp. Disabling internal
  LMR repaired none of eight development or two confirmation targets at 500k nodes.
- **Next:** separately restrict the root to `d2d4` and `h2h3` at equal node budgets,
  preserving starting FEN/history. Compare scores and continuations, then check
  other development groups. Use new game/opening-disjoint confirmation; the pilot's
  inspected confirmation groups are no longer fresh holdouts.
- **Evidence files:** `tools/analysis/output/diagnostic-pilot-ccb718b/`.

### ENG-037 — Investigate TT aging and same-key retention

- **Question:** does retaining old deep entries over newer shallow results reduce
  useful TT reuse across searches?
- **Evidence:** the historical `d3116b1` TT could reject shallow same-key writes
  regardless of age. ENG-040 now permits aged same-tag replacement and refreshes
  available move hints before its score-overwrite guard. Entry age is still
  refreshed by accepted record stores, not hits. No weakness in the current
  aging policy has been measured.
- **Next:** measure entry age, rejected updates, and useful hits over repeatable
  search sequences that retain the TT. Use the findings to select one change to
  age refresh or same-key replacement, holding layout and other policy fixed.
  Compare fresh baseline/candidate sequences and the standard corpus; cold-table
  runs alone cannot establish the aging benefit. Treat policy changes as tree-changing.

### ENG-038 — Investigate evaluation-only TT records

- **Question:** can caching static evaluation without a searched bound save enough
  evaluation work to repay extra TT traffic and displacement?
- **Evidence:** at `d3116b1`, `TTBound::None` is invalid, so the table cannot retain
  evaluation-only records. Stockfish and Ethereal support them; their benefit to
  Latrunculi is unmeasured.
- **Next:** measure repeated evaluations and select one insertion site for a
  bounded prototype. Evaluation-only hits must never authorize score cutoffs.
  Keep entry size, cluster layout, and evaluation values fixed; account for saved
  evaluations, displaced useful bounds, nodes, and fresh paired elapsed time.
  Changed table occupancy can change the search tree and requires strength testing.

## Candidates for review

None.

## Candidates for local testing

None.

## Ready for OpenBench

None.

## OpenBench tests

None.

## Ready for integration

None.

## Recent results

- **ENG-018 — Integrated 2026-10-08:** declining-quiet LMR, `e9bc6c9`,
  [PR #69](https://github.com/zm-bm/latrunculi/pull/69). OpenBench #38 accepted
  the original candidate; acceptance was retained after combination checks
  with ENG-036. [Evidence](../tools/analysis/output/eng-018-integration/report.md).

- **ENG-036 — Integrated 2026-10-08:** huge-page TT allocation, `62fd8a1`,
  [PR #70](https://github.com/zm-bm/latrunculi/pull/70). Checks passed without
  changing the search tree; no strength gain claimed. Default Hash remains
  32 MiB. [Evidence](../tools/analysis/output/eng-036-integration/report.md).

- **ENG-040 — Integrated 2026-10-07:** compact clock-aware TT, `6237d6e`.
  OpenBench #36 acceptance was retained after combination checks.
  [Evidence](../tools/analysis/output/eng-040-integration/report.md).

- **ENG-039 — Integrated 2026-10-07:** fifty-move/checkmate precedence,
  `2eee9a5`. Correctness checks passed; OpenBench was explicitly waived.
  [Evidence](../tools/analysis/output/eng-039-integration/report.md).
