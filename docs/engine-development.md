# Engine Development

The current work board for improving Latrunculi.

Keep one concise live entry per experiment: question or claim, current decision,
next action, baseline and candidate branch/revisions, reviewed head, and evidence
and PR links when applicable. Keep detailed measurements and investigation
history in linked reports, and code-review discussion in the PR. Local-only
candidates retain review notes with their evidence.
Preserve IDs; number retained candidates and unresolved leads, but leave casual
nulls unnumbered.
Follow the shared [retention rules](../.agents/references/candidate-rules.md#retention)
for evidence and cleanup.

## Baseline

| Field | Current value |
|---|---|
| Engine | `6237d6e` (ENG-040: compact clock-aware TT) |
| OpenBench fingerprint | 3,423,173 nodes |
| Search corpus | `tools/analysis/output/search-baseline-6237d6e/`; 200 positions, depth 10, one thread, 32 MiB Hash; 54,571,535 nodes |
| Build | `release-dev`, GCC 15.2.0, x86-64, `gazelle`; refreshed 2026-10-07 |

Source revision: `6237d6eee1157b8622fa88277619394dc4de8ce2`.
Corpus and fingerprint match the approved candidate; Release, ASan/UBSan,
focused TSan and TT/mate-boundary checks passed.
[Integration evidence](../tools/analysis/output/eng-040-integration/report.md).

## Issues and leads

ENG-040's compact clock-aware TT is now the baseline.
ENG-036 has unresolved speed evidence; ENG-037/038 are lower-priority follow-ups.
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

### ENG-036 — Test huge-page backing for the TT

- **Decision:** performance unresolved; no candidate retained. Against `6237d6e`,
  the sole aligned-allocation/MADV_HUGEPAGE prototype fully backed sampled 32 MiB
  tables with huge pages and reduced completed load page walks by 37.5% in a
  diagnostic run. The 64 MiB sample received partial backing.
- **Evidence:** full D10/T1/H32 corpus signatures match the baseline; focused
  resize/clear and forced allocation/advice fallback probes pass. Two exploratory
  BC/CB blocks split wins, with balanced time ratios 0.9964 and 0.9997; background
  SMT activity prevents a repeatable small-speed claim. No later pipeline stage
  began. [Report, exact patch and inputs](../tools/analysis/output/eng-036-exploration/report.md).
- **Next:** set aside for now. If resumed, use an otherwise idle host/core with
  an unoccupied SMT sibling and the same benchmark harness on both sides.
  Verify backing in the timed processes and collect fresh balanced pairs at equal
  capacity. Retain only with repeatable speed evidence;
  keep layout/replacement/search fixed and require no global huge-page changes.

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

### ENG-018 — Reduce declining late quiet moves more

- **Claim:** one extra LMR ply for declining non-PV, non-killer quiets saves work;
  same-side trends reuse existing evaluations and exclude missing/null paths.
- **Identity:** `eng-018-static-trend-lmr`, candidate/reviewed/tested head
  `9a984c3`, base `6237d6e`. Tree-changing. [PR #69](https://github.com/zm-bm/latrunculi/pull/69)
  is ready for human review.
- **Evidence:** [exploration](../tools/analysis/output/eng-018-exploration/report.md),
  [review](../tools/analysis/output/eng-018-review/report.md),
  [offline pass](../tools/analysis/output/eng-018-offline/report.md).
  Correctness, repeatability, Release, ASan/UBSan and focused TSan pass.
  Six timing pairs: about 3.5% faster, 4.3% fewer corpus nodes; fingerprint 3,184,133.
- **OpenBench:** [#38](https://workstation-01.tail2abd87.ts.net/test/38/) active;
  standard `[0, 3]` normalized-Elo SPRT, alpha=beta=0.05, `10+0.1`, T1/H32,
  no game cap. [Verified submission](../tools/analysis/output/eng-018-openbench/report.md).
- **Acceptance/next:** local correctness and speed requirements are satisfied;
  strength requires the SPRT upper boundary. Check #38 only when requested.
  No integration.

## Ready for integration

None.

## Recent results

- **ENG-040 — Integrated 2026-10-07:** compact clock-aware TT, `6237d6e` from
  approved `17b4044`. OpenBench #36 accepted published `6b27a3f`; the user retained
  that game acceptance after complete current-baseline combination checks.
  Integration Release, sanitizers, TT/mate regressions and corpus identity pass;
  fingerprint **3,423,173**.
  [Evidence and full revisions](../tools/analysis/output/eng-040-integration/report.md).

- **ENG-039 — Integrated 2026-10-07:** fifty-move/checkmate precedence,
  `2eee9a5` from approved `2570878`. User explicitly waived OpenBench for this
  narrow correctness fix. Integration checks passed; no strength gain claimed.
  [Evidence and full revisions](../tools/analysis/output/eng-039-integration/report.md).
