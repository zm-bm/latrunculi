# Earlier engine ideas

Unpromoted leads, kept briefly until reconsidered or retired. Bring fresh baseline
evidence to [Engine Development](engine-development.md#issues-and-leads) and keep
the original ID when taking one up. The current skills and testing guide govern
the work; old recipes and test histories remain available in Git.

Search-cost observations below came from `c8bfedd` (200-position depth-10 corpus
and eight-position depth-12 profile, one thread, 32 MiB Hash). Evaluation leads
came from the `0ff898c` game/endgame audit using approximate reference-engine
scores. These observations do not establish weaknesses in the current baseline.

## ENG-004 — Cache stable pawn-and-king evaluation terms

Evaluation occupied roughly 40% of search cycles; shelter was 18% of standalone
evaluation. Measure reuse of deterministic pawn/shelter terms under a complete
pawn/king/castling key before trying a small per-worker cache with exact scores.

## ENG-005 — Short-circuit transposition-table misses

TT probes occupied 15–16% of cycles, with many misses. Investigate whether an
independent rejection tag can avoid payload work while preserving entry size and
snapshot validation. Inspect generated code first: source-level decoding may
already be deferred by the compiler. This differs from changing cluster capacity.

## ENG-006 — Add a thresholded SEE fast path

SEE occupied 5–6% of search cycles. Count consumers that need only a threshold
comparison, then consider an equivalent boolean fast path. Keep exact scores
where their magnitude determines move ordering; avoid duplicating work on survivors.

## ENG-007 — Remove common-path late-picker scanning

Move picking occupied 18–20% of cycles. Measure repeated selection/scanning of
ordinary quiets and bad captures, then consider a structure that emits the same
move sequence with less work. Earlier score/history reuse did not deliver a gain.

## ENG-008 — Incrementalize tactical-cache maintenance

Board transitions dominated perft but occupied only 8–9% of integrated search.
Attribute tactical-cache cost to individual fields before attempting incremental
maintenance; compare against full recomputation through all move types.

## ENG-013 — Stratify LMR re-searches before testing adaptive verification depth

Reduced fail-highs were rare but their full-depth re-searches were costly. Look
for a repeatable subset by depth, node type, or fail-high margin that could use
cheaper verification; account for work displaced into later re-searches.

## ENG-014 — Isolate expensive PVS misses

Root/internal PVS misses were only about 1–2% of attempts but triggered substantial
re-search work. Separate their exclusive cost from overlapping LMR retries before
testing one window or sequencing change for a specific subset.

## ENG-015 — Find a materially different qsearch capture-safety signal

Qsearch dominated node count, but many stand-pat evaluations and TT hits already
cut off. Look for a capture predicate that distinguishes useful from dispensable
work. The tested captured-value-only rule skipped real cutoffs; its result is
retained on the development board.

## ENG-016 — Specialize qsearch TT work by usefulness stratum

Only 14.2% of qsearch probes hit, but 92.4% of those hits cut off. Investigate
whether a particular check/node/ply category has little TT benefit before trying
selective TT use; measure the extra searching caused by lost hits.

## ENG-017 — Generate or reject evasions more cheaply

About a third of returned evasion candidates were rejected as illegal. Identify
an expensive rejection class, then consider earlier filtering that preserves the
legal move set and order. Measure integrated search, including shifted work.

## ENG-018 — Measure static-evaluation trend as one search signal

Investigate whether same-side prior-ply evaluation trends distinguish outcomes
for a particular pruning or reduction rule. Establish predictive value sufficient
to repay the added state/evaluation cost before changing that rule.

## ENG-021 — Census endgames with exact WDL and DTZ

Use independently verified tablebase results to locate repeated low-material
errors, preserving full FENs and fifty-move semantics. Establish readable Syzygy
coverage for the selected material classes; keep uncovered cases unlabelled.
This is an external diagnostic, separate from adding engine tablebase support.

## ENG-024 — Recognize one-exchange candidate passers

Test whether evaluation misses pawns that become passed after another friendly
pawn removes their sole enemy pawn blocker. First find recurring game evidence
that distinguishes this geometry from unsupported or multiple-blocker cases.

## ENG-025 — Value rooks behind passed pawns

Investigate unobstructed friendly/enemy rook placement behind true passers. The
old broad rook sample did not establish this weakness; find relevant errors before
trying a focused evaluation term.

## ENG-026 — Add a built-in exact KPK bitbase

If exact analysis reveals recurring king-and-pawn versus king errors, consider a
compact generated bitbase. Independently verify the complete domain, fifty-move
handling, and root move selection; this is distinct from file-backed tablebases.

## ENG-027 — Recognize exact wrong-bishop rook-pawn draws

Look for errors in king, bishop, and rook-pawn versus king positions. Any draw
recognizer should cover independently verified states and respect side to move,
king placement, and promotion tactics; material and bishop color alone are insufficient.

## ENG-028 — Scale blockaded lone-minor pawn endings

Use exact endgame labels to determine whether a lone minor plus blocked pawns is
consistently overvalued when the defending king stops progress. Keep bishop and
knight cases separate and verify that a focused scaling rule preserves real wins.

## ENG-029 — Scale low-pawn minor endings confined to one flank

Check whether minor-piece endings with few pawns on one flank are overvalued
against exact labels. Isolate that geometry from opposite-colored bishops and
other draw mechanisms before testing a focused scale.

## ENG-030 — Add one-ply singular extension for a proven TT move

Find positions where excluding a reliable TT move shows that alternatives are
substantially worse. Test whether a single-ply extension repairs search errors
enough to repay the additional verification and search work.
