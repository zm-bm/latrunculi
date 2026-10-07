# Engine Development

The current work board for improving Latrunculi. [Engine testing](engine-testing.md)
explains what evidence lets an item advance.

Keep one live entry per experiment: question or claim, baseline evidence, next
action, and candidate branch/revisions when applicable. Preserve IDs; number
retained candidates and unresolved leads, but leave casual nulls unnumbered.
Keep detailed measurements and completed history in evidence reports and Git
history. Run one local CPU-sensitive task at a time; concurrent OpenBench tests
are allowed.

## Baseline

| Field | Current value |
|---|---|
| Engine | `836ffe2` (ENG-041: clock-safe adaptive time management) |
| OpenBench fingerprint | 3,507,960 nodes |
| Search corpus | `tools/analysis/output/search-baseline-836ffe2/`; 200 positions, depth 10, one thread, 32 MiB Hash; 54,039,403 nodes |
| Build | `release-dev`, GCC 15.2.0, x86-64, `gazelle`; refreshed 2026-10-07 |

Source revision: `836ffe25e1f4903af5592d7a8de28abe0370f34f`.
Corpus and fingerprint match the accepted candidate; Release, ASan/UBSan and
clock/lifecycle checks passed. [Integration evidence](../tools/analysis/output/eng-041-integration/report.md).

## Issues and leads

The TT clock-reuse defect remains in the baseline; ENG-040 is the active candidate.
ENG-036 is an independent allocation lead; ENG-037/038 are lower-priority follow-ups.
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

- **Question:** can huge-page allocation reduce TT address-translation cost while
  preserving the search tree?
- **Evidence:** the `d3116b1` audit found 4 KiB pages and no huge-page backing for
  fresh 32/64 MiB tables on `gazelle` with THP in `madvise` mode. The allocator
  makes no explicit huge-page request; several reference engines do. A speedup
  and TLB pressure have not been established.
- **Next:** try suitable allocation alignment and huge-page advice with a normal
  allocation fallback. Verify actual backing before fresh paired timings against
  the current baseline at the same table capacity; check exact search signatures.
  Hold layout, replacement, and search policy fixed. Exercise resize, clear, and
  fallback behavior; do not require system-wide huge-page settings.

### ENG-037 — Investigate TT aging and same-key retention

- **Question:** does retaining old deep entries over newer shallow results reduce
  useful TT reuse across searches?
- **Evidence:** at `d3116b1`, age refreshes only on successful stores, and shallow
  non-exact same-key writes can be rejected regardless of entry age. References
  vary: some refresh on hits or permit aged same-key replacement. No weakness in
  the current policy has been measured.
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

### ENG-039 — Checkmate at the fifty-move boundary

- **Defect:** recursive main search and qsearch check for a draw before detecting
  checkmate. With an empty TT, `7k/8/5KQ1/8/8/8/8/8 w - - 99 1` scores 0 cp
  at depth 6, although Qg7 checkmates at clock 100. At clock 98 it scores mate in one.
- **Evidence:** three repetitions on baseline `d3116b1` and ENG-035 candidate
  `4da58e7`; a separate legal-move enumeration verifies Qg7 leaves the king in
  check with no legal reply. This is separate from TT reuse. Reproductions and
  enumeration: `tools/analysis/output/eng-035-tt-clock/`.
- **Next:** reproduce on `836ffe2`, then fix fifty-move/checkmate precedence in
  recursive main search and qsearch. Keep TT policy, repetition/material draws
  and qsearch move generation unchanged; avoid legal-move generation at ordinary nodes.
- **Checks:** root/main search finds the clock-99 mate-in-one; direct main/qsearch
  recognizes the clock-100 checkmated child in PV and NonPV paths, with correct
  mate distance. Qsearch need not discover the parent's quiet Qg7. Checked
  clock-100 positions with legal evasions must remain draws; retain clock-98/99
  mate, stalemate and ordinary draw controls.
- **Tradeoff:** tree-changing correctness fix, independent of ENG-040. Measure
  runtime cost without requiring a speedup.

## Candidates for local testing

### ENG-040 — Compact tagged TT with clock-aware keys

- **Claim:** three 10-byte entries per aligned 32-byte cluster give 50% more
  entries at equal Hash capacity. TT keys share clocks 0–15, use eight-ply groups
  at 16–79, and become exact at 80+. Repetition keys stay unchanged.
  Tree-changing mitigation of the demonstrated reuse defect; strength pending.
- **Revision:** `candidate/eng-040-compact-tt-cleanup`, `6b27a3f` on `d3116b1`.
  Reviewed cleanup preserves the original engine/benchmark/stress binaries.
- **Baseline status:** needs local combination checks against `836ffe2` after
  ENG-041 integration. Preserve the published branch and #36 comparison; its
  games remain strength evidence for the tested revision. A baseline refresh
  alone does not schedule a replacement SPRT.
- **Offline pass (2026-10-06):** 397 Release cases plus stress, full ASan/UBSan
  with leak detection, focused TSan, all eleven material families and mate
  controls passed. Corpus signatures and fingerprint **3,423,173** repeat;
  1,200 four-thread searches complete with legal moves/PVs.
- **Tradeoff:** depth-10 corpus **54,571,535 nodes** (+0.98%), 26 changed best
  moves. Six alternating timing pairs give median balanced ratio **0.98808**
  (about 1.2% less search time), with appreciable variation. This is a local
  estimate; a speedup is not required for the correctness mitigation.
- **Risks:** 16-bit tags allow false hits; individually atomic fields can mix
  writes without the old signature validation. Clock sharing remains heuristic;
  cross-clock move/evaluation hints are lost. ENG-039 remains separate.
- **OpenBench:** [#36](https://workstation-01.tail2abd87.ts.net/test/36/),
  `6b27a3f` versus `d3116b1`, normalized-Elo SPRT **[-3,0]**, alpha=beta=0.05,
  **10+0.1**, T1 H32, standard UHO book/adjudication, no game cap.
  PGNs `/api/pgns/36/`; submitted 2026-10-06.
- **Acceptance and next:** await the declared boundary. Upper accepts under the
  permitted strength-tradeoff profile; lower rejects; an early stop is inconclusive.
  Confirmed correctness/operational defects block acceptance. No automatic
  longer-TC confirmation; additional games need specific justification and an
  explicit request. Complete local combination checks before requesting integration.
- **Evidence:** [exploration](../tools/analysis/output/eng-040-compact-tt/report.md),
  [review](../tools/analysis/output/eng-040-review/report.md),
  [offline tests](../tools/analysis/output/eng-040-offline/report.md),
  [OpenBench settings and provenance](../tools/analysis/output/eng-040-openbench/report.md).

## Ready for OpenBench

None.

## OpenBench tests

[ENG-040 / #36](https://workstation-01.tail2abd87.ts.net/test/36/) is active.
Its [candidate entry](#eng-040--compact-tagged-tt-with-clock-aware-keys) remains
under local testing because the operational baseline changed.

## Ready for integration

None.

## Recent results

- **ENG-041 — Integrated 2026-10-07:** clock-safe adaptive time management,
  `836ffe2` from approved `1bd5a1c`. OpenBench #37 accepted `[0,3]` at `10+0.1`:
  1,172 games, **+86.21 +/-15.34 Elo (95%)**, zero crashes/time forfeits.
  Integration checks passed; zero-increment game strength remains unmeasured.
  [Evidence and full revisions](../tools/analysis/output/eng-041-integration/report.md).
