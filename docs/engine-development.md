# Engine Development

Current work, decisions, and next actions. Keep detailed evidence in linked
reports and PRs; follow the [candidate rules](../.agents/references/candidate-rules.md).

## Baseline

| Field | Current value |
|---|---|
| Engine | [`6a9d7a8`](https://github.com/zm-bm/latrunculi/commit/6a9d7a815281b11fb83705035c3af53ce4e6973d) (ENG-042: first HCE tuning pass) |
| OpenBench fingerprint | 4,353,365 nodes |
| Search corpus | `tools/analysis/output/eng-042-integration/corpus/`; 200 positions, depth 10, one thread, 32 MiB Hash; 48,951,842 nodes |
| Build | `release-dev`, GCC 15.2.0, x86-64, `gazelle`; refreshed 2026-10-14 |

Validation and evidence reuse are recorded in the
[integration report](../tools/analysis/output/eng-042-integration/report.md).
The published [1.1.0 release](releases/1.1.0.md) predates this tuning.

## Issues and leads

ENG-037/038 are lower-priority follow-ups to the
[TT audit at d3116b1](../tools/analysis/output/tt-design-audit-d3116b1/report.md).

### ENG-032 — Explain persistent quiet-move disagreements

- **Question:** are alternative quiet root moves under-searched?
- **Evidence:** on `ccb718b`, anchor `deb4939117f9cebe` chooses h3 at 50k, 500k,
  and 5M nodes; a reference MultiPV report favors d4 by 125 cp. Disabling internal
  LMR repaired none of eight development or two confirmation targets at 500k nodes.
- **Next:** compare `d2d4` and `h2h3` with restricted roots, equal node budgets,
  and identical FEN/history, then check other development groups. Use fresh
  game/opening-disjoint confirmation; the pilot's confirmation groups were inspected.
- **Evidence files:** `tools/analysis/output/diagnostic-pilot-ccb718b/`.

### ENG-037 — Investigate TT aging and same-key retention

- **Question:** does retaining old deep entries over newer shallow results reduce
  useful TT reuse across searches?
- **Evidence:** ENG-040 permits aged same-tag replacement and refreshes move hints
  before the score-overwrite guard. Age is refreshed by accepted stores, not hits;
  no weakness in the current policy has been measured.
- **Next:** measure age, rejected updates, and useful hits across repeatable searches
  that retain the TT. Test one age/retention change with layout and other policy
  fixed. Cold-table runs alone cannot establish a benefit; treat policy changes
  as tree-changing.

### ENG-038 — Investigate evaluation-only TT records

- **Question:** can caching static evaluation without a searched bound save enough
  evaluation work to repay extra TT traffic and displacement?
- **Evidence:** at `d3116b1`, `TTBound::None` is invalid, preventing evaluation-only
  records. Their benefit to Latrunculi is unmeasured.
- **Next:** measure repeated evaluations and prototype one insertion site, holding
  entry size, layout, and evaluation values fixed. Evaluation-only hits must never
  authorize cutoffs. Measure saved evaluations, displaced bounds, nodes, and paired
  time; changed table occupancy requires strength testing.

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

- **ENG-042 — Integrated 2026-10-14:** first HCE tuning pass plus shared Python/
  OpenBench tooling and an independent LMR test correction, `6a9d7a8`,
  [PR #71](https://github.com/zm-bm/latrunculi/pull/71). OpenBench #45 acceptance
  applies through exact engine equivalence. [Evidence](../tools/analysis/output/eng-042-integration/report.md).

- **ENG-018 — Integrated 2026-10-08:** declining-quiet LMR, `e9bc6c9`,
  [PR #69](https://github.com/zm-bm/latrunculi/pull/69). OpenBench #38 acceptance
  retained after combination with ENG-036. [Evidence](../tools/analysis/output/eng-018-integration/report.md).

- **ENG-036 — Integrated 2026-10-08:** huge-page TT allocation, `62fd8a1`,
  [PR #70](https://github.com/zm-bm/latrunculi/pull/70). Tree-preserving; no strength
  gain claimed. [Evidence](../tools/analysis/output/eng-036-integration/report.md).

- **ENG-040 — Integrated 2026-10-07:** compact clock-aware TT, `6237d6e`.
  OpenBench #36 acceptance was retained after combination checks.
  [Evidence](../tools/analysis/output/eng-040-integration/report.md).

- **ENG-039 — Integrated 2026-10-07:** fifty-move/checkmate precedence,
  `2eee9a5`. Correctness checks passed; OpenBench was explicitly waived.
  [Evidence](../tools/analysis/output/eng-039-integration/report.md).
