# Engine Development

The current work board for improving Latrunculi. [Engine testing](engine-testing.md)
explains what evidence lets an item advance; [earlier ideas](engine-ideas.md)
retains historical proposals and findings.

Keep one live entry per experiment and move it between stages. A lead names its
observation, baseline evidence, and next test. A candidate adds its claim, branch,
base and candidate revisions, evidence, and next action. Preserve IDs; number
retained candidates and unresolved leads, but leave casual nulls unnumbered.
For each bounded attempt, record its baseline, tested scope, outcome, and why
it stopped; keep live evidence and useful terminal findings. Run one local
CPU-sensitive task and one OpenBench test at a time.

## Baseline

| Field | Current value |
|---|---|
| Engine | `ccb718b` (ENG-031 connected-pawn link evaluation) |
| OpenBench fingerprint | 3,507,960 nodes |

## Issues and leads

### ENG-032 — Explain persistent quiet-move disagreements

- **Question:** does the baseline under-search alternative quiet root moves, or
  does the score disagreement persist after focused work?
- **Evidence:** on `ccb718b`, development anchor `deb4939117f9cebe` chooses h3 at 50k, 500k,
  and 5M nodes. One complete reference MultiPV report ranks d4 above h3 by 125 cp. Disabling
  all internal LMR repaired none of the eight development or two confirmation quiet targets
  at 500k nodes; this leaves targeted selectivity and evaluation unresolved.
- **Next:** separately restrict the root to `d2d4` and `h2h3` at equal node budgets,
  preserving the anchor's starting FEN and history. Compare scores and continuations to distinguish
  a ranking changed by focused work from an unresolved score difference, then check whether the
  cause recurs in other development groups.

Pilot report and raw archive: `tools/analysis/output/diagnostic-pilot-ccb718b/`.
Its inspected confirmation groups cannot serve as a fresh holdout for a later
candidate; use new group-disjoint confirmation.

## Candidates for local testing

None.

## Ready for OpenBench

None.

## OpenBench tests

### ENG-010 — Extend guarded main-search futility to depth 4 at 1350 cp

- **Revision:** published branch `eng-010-depth4-futility-1350`, candidate `4ab957b`, base
  `ccb718b`; speed, tree-changing. Reverse futility remains at depth 3; tactical guards are preserved.
  Full candidate: `4ab957b83a04c55bedb0a43e149e704e32ce5efb`;
  full base: `ccb718b90deafad0247cccffabf07bf88310b415`.
- **Offline evidence:** 23,584 shadow triggers; `R_node_g = 0.985618`,
  `R_node_total = 0.990811`, `R_time_balanced = 0.990284` with 6/6 wins.
  Two 3,412,800-node fingerprints, Release, ASan+UBSan, and exact corpus/sentinel repeats passed.
- **OpenBench #32:** stopped, inconclusive `[0, 3]` SPRT after 49,180 games
  (`15993-15746-17441`), LLR +1.06 inside ±2.94, Elo +1.74 ±2.27 (95%).
  Server test `/test/32/`, PGN `/api/pgns/32/`; OpenBench revision
  `5184c6fde256bf20a085ee089f99c7026b88c43e`.
- **Next:** decide whether to resume #32 unchanged or retire the candidate.

## Ready for integration

None.

## Recent results

- **Diagnostic pilot on `ccb718b`:** 100 opening pairs; 24 development errors
  and eight controls, 14 confirmation errors and six controls. Removing all
  internal LMR repaired 0/8 and 0/2 persistent quiet targets at matched 500k
  nodes. Confirmation mean loss changed −11.6 cp [−31.6, +3.0], with five
  new large errors at 5M. **Scoped null:** the no-LMR variant did not repair
  those targets. Lower completed depth and sparse quiet confirmation limit the
  conclusion; no candidate was retained.
- **ENG-031:** connected-pawn links `{MG 13, EG 3}` integrated as `ccb718b`.
  Offline reproducibility passed with a deliberate gate override. OpenBench #31
  accepted `[0, 3]` after 19,142 games: LLR +2.9575, Elo +5.79 ±3.69 (95%).
  Focused validation remained sealed.
- **ENG-019:** safe passed-pawn path bonus reduced large errors from nine to
  four in development, but validation changed four to five, with one repair
  and two new errors. **Null:** the development benefit did not carry over.
- **ENG-022:** tested passer-race predicates explained zero and one development
  errors. **Null for those discriminators.**
- **ENG-023:** protected/connected true-passer predicates explained zero and
  one development errors. **Null for those discriminators.**

Older outcomes and detailed evidence are retained in [Earlier ideas](engine-ideas.md).
ENG-019/022/023 summaries were recovered from `a90da10`; their raw artifacts are absent here.
