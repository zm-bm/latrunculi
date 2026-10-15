# Engine Ideas

Questions worth investigating, not a commitment or a current weakness list.
Recheck old observations on the current engine before implementing a proposal.
Existing IDs help find earlier evidence; new ideas do not require an ID.
See the [development manual](engine-development.md) for commands and the
[roadmap](roadmap.md) for longer-term feature direction.

Search-cost observations below came from `c8bfedd` (200-position depth-10 corpus
and eight-position depth-12 profile, one thread, 32 MiB Hash). Evaluation leads
came from the `0ff898c` game/endgame audit using approximate reference-engine
scores. Individual entries identify other historical revisions where applicable.
ENG-037/038 are lower-priority follow-ups to the TT audit.

## Search

### ENG-032 — Explain persistent quiet-move disagreements

- **Question:** are alternative quiet root moves under-searched?
- **Evidence:** on `ccb718b`, anchor `deb4939117f9cebe` chooses h3 at 50k, 500k,
  and 5M nodes; a reference MultiPV report favors d4 by 125 cp. Disabling internal
  LMR repaired none of eight development or two confirmation targets at 500k nodes.
- **Next:** compare `d2d4` and `h2h3` with restricted roots, equal node budgets,
  and identical FEN/history, then check other development groups. Use fresh
  game/opening-disjoint confirmation; the pilot's confirmation groups were inspected.
- **Evidence files:** `tools/analysis/output/diagnostic-pilot-ccb718b/`.

### ENG-013 — Stratify LMR re-searches before testing adaptive verification depth

Reduced fail-highs were rare but their full-depth re-searches were costly. Look
for a repeatable subset by depth, node type, or fail-high margin that could use
cheaper verification; account for work displaced into later re-searches.

### ENG-014 — Isolate expensive PVS misses

Root/internal PVS misses were only about 1–2% of attempts but triggered substantial
re-search work. Separate their exclusive cost from overlapping LMR retries before
testing one window or sequencing change for a specific subset.

### ENG-015 — Find a materially different qsearch capture-safety signal

Qsearch dominated node count, but many stand-pat evaluations and TT hits already
cut off. Look for a capture predicate that distinguishes useful from dispensable
work. The tested captured-value-only rule skipped real cutoffs; the prior
[captured-value margins of 200/300/400](https://github.com/zm-bm/latrunculi/blob/0823ac0935b5a6c97204c01dede331e777753d17/docs/engine-development.md#eng-015--find-a-materially-different-qsearch-capture-safety-signal)
remain a negative finding for that tested approach.

### ENG-016 — Specialize qsearch TT work by usefulness stratum

Only 14.2% of qsearch probes hit, but 92.4% of those hits cut off. Investigate
whether a particular check/node/ply category has little TT benefit before trying
selective TT use; measure the extra searching caused by lost hits.

### ENG-030 — Add one-ply singular extension for a proven TT move

Find positions where excluding a reliable TT move shows that alternatives are
substantially worse. Test whether a single-ply extension repairs search errors
enough to repay the additional verification and search work.

## Evaluation and endgames

### ENG-021 — Census endgames with exact WDL and DTZ

Use independently verified tablebase results to locate repeated low-material
errors, preserving full FENs and fifty-move semantics. Establish readable Syzygy
coverage for the selected material classes; keep uncovered cases unlabelled.
This is an external diagnostic, separate from adding engine tablebase support.

### ENG-024 — Recognize one-exchange candidate passers

Test whether evaluation misses pawns that become passed after another friendly
pawn removes their sole enemy pawn blocker. First find recurring game evidence
that distinguishes this geometry from unsupported or multiple-blocker cases.

### ENG-025 — Value rooks behind passed pawns

Investigate unobstructed friendly/enemy rook placement behind true passers. The
old broad rook sample did not establish this weakness; find relevant errors before
trying a focused evaluation term.

### ENG-026 — Add a built-in exact KPK bitbase

If exact analysis reveals recurring king-and-pawn versus king errors, consider a
compact generated bitbase. Independently verify the complete domain, fifty-move
handling, and root move selection; this is distinct from file-backed tablebases.

### ENG-027 — Recognize exact wrong-bishop rook-pawn draws

Look for errors in king, bishop, and rook-pawn versus king positions. Any draw
recognizer should cover independently verified states and respect side to move,
king placement, and promotion tactics; material and bishop color alone are insufficient.

### ENG-028 — Scale blockaded lone-minor pawn endings

Use exact endgame labels to determine whether a lone minor plus blocked pawns is
consistently overvalued when the defending king stops progress. Keep bishop and
knight cases separate and verify that a focused scaling rule preserves real wins.

### ENG-029 — Scale low-pawn minor endings confined to one flank

Check whether minor-piece endings with few pawns on one flank are overvalued
against exact labels. Isolate that geometry from opposite-colored bishops and
other draw mechanisms before testing a focused scale.

## Performance and caching

### ENG-004 — Cache stable pawn-and-king evaluation terms

Evaluation occupied roughly 40% of search cycles; shelter was 18% of standalone
evaluation. Measure reuse of deterministic pawn/shelter terms under a complete
pawn/king/castling key before trying a small per-worker cache with exact scores.

### ENG-006 — Add a thresholded SEE fast path

SEE occupied 5–6% of search cycles. Count consumers that need only a threshold
comparison, then consider an equivalent boolean fast path. Keep exact scores
where their magnitude determines move ordering; avoid duplicating work on survivors.

### ENG-007 — Remove common-path late-picker scanning

Move picking occupied 18–20% of cycles. Measure repeated selection/scanning of
ordinary quiets and bad captures, then consider a structure that emits the same
move sequence with less work. Earlier score/history reuse did not deliver a gain.

### ENG-008 — Incrementalize tactical-cache maintenance

Board transitions dominated perft but occupied only 8–9% of integrated search.
Attribute tactical-cache cost to individual fields before attempting incremental
maintenance; compare against full recomputation through all move types.

### ENG-017 — Generate or reject evasions more cheaply

About a third of returned evasion candidates were rejected as illegal. Identify
an expensive rejection class, then consider earlier filtering that preserves the
legal move set and order. Measure integrated search, including shifted work.

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

Historical context: [TT audit at d3116b1](../tools/analysis/output/tt-design-audit-d3116b1/report.md).

### ENG-038 — Investigate evaluation-only TT records

- **Question:** can caching static evaluation without a searched bound save enough
  evaluation work to repay extra TT traffic and displacement?
- **Evidence:** at `d3116b1`, `TTBound::None` is invalid, preventing evaluation-only
  records. Their benefit to Latrunculi is unmeasured.
- **Next:** measure repeated evaluations and prototype one insertion site, holding
  entry size, layout, and evaluation values fixed. Evaluation-only hits must never
  authorize cutoffs. Measure saved evaluations, displaced bounds, nodes, and paired
  time; changed table occupancy requires strength testing.

Historical context: [TT audit at d3116b1](../tools/analysis/output/tt-design-audit-d3116b1/report.md).
