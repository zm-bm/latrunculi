# Engine Development

This document tracks work intended to improve how Latrunculi plays, followed by the shared rules
for testing search changes and the durable findings. [OpenBench](openbench.md) defines game testing,
the [measurement guide](../tools/measurements/README.md) documents commands and formats, and the
[evaluation tuning workflow](../tools/tuning/workflow.md) covers linear HCE fitting.

Record decisions, not command histories. Keep artifacts for the current baseline and live
candidates; summarize terminal results here and remove their output unless it remains useful.

The `Next` field names the only action needed to advance a candidate:

| Next | Skill |
|---|---|
| `explore` | [`explore-latrunculi-change`](../.agents/skills/explore-latrunculi-change/SKILL.md) |
| `test offline` | [`test-latrunculi-candidate`](../.agents/skills/test-latrunculi-candidate/SKILL.md) |
| `submit OpenBench` | [`submit-latrunculi-openbench`](../.agents/skills/submit-latrunculi-openbench/SKILL.md) |
| `check OpenBench #ID` | [`check-latrunculi-openbench`](../.agents/skills/check-latrunculi-openbench/SKILL.md) |
| `integrate` | [`integrate-latrunculi-candidate`](../.agents/skills/integrate-latrunculi-candidate/SKILL.md) |

Keep an existing `SW-XX` or `ENG-XXX` ID throughout its life. Assign a new `ENG-XXX` only when new
work becomes a retained candidate or unresolved queue item; casual and null exploration stays
unnumbered. Keep one local CPU-sensitive task and one OpenBench test active at a time. Tree-changing
work requires games; tree-preserving work may skip them only when the evidence below passes. Unless
a request authorizes several named actions, each skill stops at its own boundary.

## Baseline

| Field | Current value |
|---|---|
| Engine | `f376cef` (SW-17 integration) |
| OpenBench fingerprint | 3,798,460 nodes |
| Cached search corpus | `tools/measurements/output/search-baseline-f376cef/` |

Refresh the cached corpus only after an approved integration changes the engine baseline, search
workload, or measurement meaning.

## Candidates

| ID | Change | Kind | Evidence | Next |
|---|---|---|---|---|

## Queue

These are audit-backed leads, not prequalified candidates. They are ordered by current priority.
**Direct experiment** means the audit supports a small behavior or implementation comparison now;
**measure first** means no behavior candidate should be retained until the stated discriminator is
observed. Explore one item at a time and recheck dependent items against the then-current baseline.

The numerical evidence in ENG-001 through ENG-018 comes from the 2026-09-25 audit of
`c8bfedd` (SW-14): the 200-position corpus at depth 10 and the eight positions now tracked in
[`search-profile.epd`](../tools/measurements/search-profile.epd) at depth 12, using one thread and
32 MiB Hash. Counts describe that fixed tree and cycle shares are sampled approximations; remeasure
affected evidence on the then-current baseline before retaining a candidate. Reference comparisons
were source-only at Ethereal `0e47e9b`, Stockfish `86f1df7`, and Minic `4317c14`; they motivate
mechanisms, not constants or expected outcomes.

**Direct experiments**

### ENG-002 — Recenter aspiration retries on the returned score

On a miss, move only the failed bound to `value +/- delta`, initially retaining the current 50-point
window and doubling schedule. The audit measured 735 misses and 6.58M retry nodes, 11.6% of the
corpus tree; Stockfish and Minic place retry bounds from the fail-soft score, while Ethereal uses a
different gradual-widening geometry. First compare exact retry and total nodes on the full corpus.
Expect fewer repeated misses and retry nodes. Stop if total work does not improve or objective and
convergence checks regress. This is distinct from SW-01's rejected 50-to-32 initial-window change.

### ENG-003 — Reuse static evaluation on same-key TT hits

Store and reuse the deterministic static evaluation in the existing TT payload without enlarging
the 16-byte entry or 64-byte cluster. Evaluation is about 39--40% of search cycles, with 8.07M
exact-key repeats; Ethereal, Stockfish, and Minic all retain a static eval in their TT entries. First
prototype compact bound/generation packing and count avoided calls on the corpus. Expect identical
evaluation values and fewer evaluation cycles. Stop on changed corpus signatures, an entry-size or
race regression, or no repeatable integrated timing gain.

**Profile-guided speed work — measure first**

### ENG-004 — Cache stable pawn-and-king evaluation terms

Measure reuse of a complete pawn/king/castling key, then prototype a small per-worker cache for only
the deterministic pawn-structure and shelter terms if reuse is material. Shelter is about 18% of
standalone evaluation and evaluation is 39--40% of search cycles; Ethereal and Minic demonstrate
pawn/king caches, not a suitable Latrunculi layout. Expect fewer shelter and pawn computations with
identical scores and trees. Stop on low hit rate, changed values, excessive cache overhead, or no
repeatable integrated timing gain.

### ENG-005 — Short-circuit transposition-table misses

Explore a key/tag layout that rejects a nonmatching TT entry before fully loading and decoding its
payload while preserving the 16-byte entry, 64-byte cluster, and current race contract. TT work is
15--16% of search cycles; 48% of main probes and 85.8% of qsearch probes miss, while the current
four-entry scan snapshots and decodes each candidate. First count miss-path loads and prototype one
layout compatible with ENG-003. Expect fewer TT instructions and cycles without changing search
signatures. Stop on added collision or race risk, larger entries, or no paired timing gain.

### ENG-006 — Add a thresholded SEE fast path

Add passive counts for SEE consumers and result bands, then prototype a `see_ge(0)`-style path for
sign-only good/bad classification while computing an exact value only where in-band ordering needs
it. SEE is 5--6% of search cycles and about 8% of branch misses, and every ordinary noisy candidate
currently receives an exact exchange score. Preserve the existing SEE bands and do not reintroduce
CaptureHistory. Expect less work on rejected or bad-band captures. Stop if duplicate work on good
captures cancels the saving, ordering changes unintentionally, or timing does not improve.

### ENG-007 — Remove common-path late-picker scanning

Count comparisons, rescans, alpha raises, and exact-node contributions in the ordinary-quiet and
bad-noisy stages, then compare one same-order bucket or partial-selection structure. Move picking is
18--20% of cycles and about 36% of branch misses; these stages return 71% of main candidates but
produce only 3.2% of beta cutoffs. This is common-path removal, not the rejected SW-20 score/history
reuse. Expect fewer picker branches and misses with identical move order and tree. Stop if no
dominant removable scan appears or a prototype changes signatures or lacks repeatable speedup.

### ENG-008 — Incrementalize tactical-cache maintenance

Attribute `refresh_tactical_cache` work to its maintained fields and prototype one exact incremental
update for the dominant field before considering a wider rewrite. Board transition maintenance is
about 80% of perft cycles but only 8--9% of integrated-search cycles, so perft alone cannot qualify
the change. Expect identical perft and search signatures with lower transition and integrated time.
Stop if the useful share is fragmented, correctness complexity is disproportionate, or integrated
timing does not improve.

**Search-behavior work — measure first**

### ENG-009 — Test one broader razoring eligibility cell

Use a stats-only adjacent depth or margin cell, chosen from the observed score distribution, and
verify its qsearch result before changing release behavior. Razoring activates at only 3.9% of
10.75M structurally eligible nodes and confirms 87.5% of its 421,499 tries. Expect a material number
of additional confirmed cutoffs at small qsearch cost. Stop if the adjacent cell is sparse, its
confirmation rate degrades materially, objective checks fail, or total tree work does not improve.

### ENG-010 — Test one broader main-search futility cell

Shadow one adjacent depth or margin cell while retaining all current mate, check, first-move, and
checking-quiet protections; use a bounded verifier to learn whether proposed skipped quiets later
raise alpha or cut off. Futility activates at 13.2% of 6.79M eligible nodes and reaches a real
nonchecking-quiet trigger 626,672 times. Expect more safe late-quiet skips and a smaller tree. Stop
if the new cell removes meaningful alpha raises or cutoffs, violates an objective check, or misses
the fixed-depth tree-size gate.

### ENG-011 — Make aspiration widening more gradual

After ENG-002 is resolved, record miss distances and compare one gradual one-bound growth schedule
without changing the initial 50-point window or retry depth. Aspiration retries are an exclusive
6.58M nodes, or 11.6% of the corpus tree; all three references use more gradual growth than the
current doubling schedule. Expect fewer unnecessarily wide retry trees without more repeated
misses. Stop if retry count or total nodes rises, convergence worsens, or the effect is not distinct
from ENG-002.

### ENG-012 — Reduce fail-high aspiration retry depth

After ENG-002, isolate the reference-engine pattern of reducing only the next fail-high retry depth,
leaving window geometry unchanged. The corpus recorded 433 fail-highs and their retry work is part
of the same exact 11.6% aspiration footprint. Expect cheaper fail-high recovery without degrading
completed-iteration stability. Stop on lost objective solutions, unstable root convergence,
increased total work, or no repeatable timing benefit.

### ENG-013 — Stratify LMR re-searches before testing adaptive verification depth

Split reduced fail-highs by PV/NonPV, reduction, depth, fail-high margin, and full-depth outcome; test
an intermediate or result-conditioned verification depth only if one stratum dominates cost. Just
25,959 of 7.58M reductions fail high, but their inclusive full-depth footprint is 16.44M nodes and
only 58.7% remain above alpha. Do not retry SW-06's exact full-depth null-window scout or the rejected
history thresholds. Expect less full-depth verification work. Stop if no stable discriminator
appears or a prototype loses objective results or merely moves work into another re-search.

### ENG-014 — Isolate expensive PVS misses

Stratify root and internal PVS re-searches by depth, move rank, miss margin, and exclusive work before
choosing any window or sequencing change. Miss rates are only 1.06% and 1.98%, but their inclusive
footprints are 5.23M and 7.50M nodes, so a few high-level misses may dominate. Expect a bounded
high-cost stratum suitable for one isolated policy experiment. Stop if the footprint is diffuse,
overlaps other re-search mechanisms beyond attribution, or no safe discriminator emerges.

### ENG-015 — Find a materially different qsearch capture-safety signal

Expose exact-SEE-negative picker rejects and classify searched captures by SEE band, check,
promotion, bound improvement, PV use, and cutoff. Qsearch is 69.2% of nodes, but 57.7% of stand-pat
evaluations and 92.4% of TT hits already cut off; the previous captured-value margins skipped real
cutoffs. Retain a pruning candidate only if the new combined signal identifies a substantial,
near-zero-utility stratum and is materially different from the rejected 200/300/400 rule. Expect a
measurable reduction opportunity defined by move outcomes rather than qsearch node share. Stop if
every plausible stratum contains meaningful cutoffs/PV moves or is too small to matter.

### ENG-016 — Specialize qsearch TT work by usefulness stratum

Split qsearch probes and stores by check state, node type, ply, hit payload, and resulting cutoff or
move use; consider skipping work only in a measured near-zero-benefit stratum. Qsearch performs
39.16M probes with a 14.2% hit rate, yet 92.4% of hits cut off, so neither global removal nor the raw
miss rate is a hypothesis. Expect less TT work without compensating node growth. Stop if every
stratum has material cutoff or ordering value, or a prototype changes results or fails timing.

### ENG-017 — Generate or reject evasions more cheaply

Classify illegal evasion rejects by move type and validation cost, then compare one exact legal-
evasion filter or generation rule that preserves order. Main and qsearch evasion stages reject
31.4% and 34.0% of returned candidates, while move generation and legality contribute measurable
branch misses. Expect fewer legality checks and picker branches with identical legal moves and
search signatures. Stop if the rejects are cheap, the legal generator becomes more complex than
the saved work, or integrated timing is neutral.

### ENG-018 — Measure static-evaluation trend as one search signal

Add a stats-only improving/worsening classification from same-side prior-ply static evaluations and
split razoring, futility, and LMR outcomes by it; select at most one consumer if the split is strong.
Reference engines use this kind of signal, but that is not evidence for a Latrunculi constant, and
the previously unhelpful improving-aware LMP variants remain closed. Expect a clearer safety signal
for one conservative gate or reduction decision. Stop if outcome rates do not separate materially
or the required state/evaluation work costs more than the prospective tree change.

## Recent results

| ID | Change | Evidence | Result |
|---|---|---|---|
| SW-17 | Use one bounded depth/static-surplus null-move reduction formula | Offline `R_node_g` 0.9397, `R_node_total` 0.9642, and `R_time_balanced` 0.9790; OpenBench #29 Base `c8bfedd`, Dev `54a8989`: accepted `[0, 3]` after 9,060 games at LLR +2.9659 and +10.55 +/- 5.14 Elo (95%); PGN `/api/pgns/29/`; integrated corpus exactly matched all 200 retained signatures | Integrated as `f376cef` |
| ENG-001 | Replace runtime check-danger division with exact function-local constexpr piece/count tables | Complete 128-state domain; exact evaluator checksum, two 5,101,317-node benchmarks, repeated 200-position corpus and 61 sentinel signatures; Release and ASan/UBSan passed; stable `R_time_balanced` 0.9871 with 6/6 wins | Integrated as `3ddf138`; tree-preserving, so games were skipped |
| SW-16 | Add one LMR ply to NonPV quiets with combined history at most -1 | Exploration found 169,273 effective opportunities; one complete corpus pass produced `R_node_g` 0.9937 and `R_node_total` 0.9868; benchmark fingerprint 6,014,947 nodes | Null result; missed the 0.9900 smaller-tree gate, so formal offline testing and games were not run |
| SW-15 | Replace locked per-node RMW with single-writer relaxed atomic load/store | Exact 5,101,317-node benchmark and 200-position corpus; complete tests, TSan, and 1/2/4-thread risk checks passed; balanced timing ratio 1.0217 with 1/6 wins | Rejected; missed the 0.9925 minimum-speedup threshold |
| SW-14 | Target-scoped release IPO/LTO | Exact Clang/GCC 5,101,317-node fingerprints and 200-position corpus; 3/3 CTest; integrated binaries match offline evidence; balanced timing ratio 0.9358 with 6/6 wins | Integrated as `c8bfedd`; tree-preserving, so games were skipped |

Git history retains older results.

## How candidates are tested

These are the default offline rules for search and search-speed candidates. A candidate records
only a justified override, a different mechanism check, or an extra risk test before measurement.

### Evidence and classification

| Panel | Purpose and use |
|---|---|
| Objective tests | Source-pinned mate and material checks plus legality and crash detection. These decide correctness, not performance. |
| Tactical corpus | All 200 Arasan positions in `tools/measurements/search.epd`; use for deterministic pruning, reductions, move ordering, aspiration, and comparable search changes. Source `bm`/`am` labels are context, not oracles. |
| Sentinels | Four cases in `tools/measurements/search-sentinels.epd`; use for descriptive trajectory diagnostics, not as move-quality oracles. |
| Benchmark fingerprint | Six fixed positions at depth 13; use its deterministic aggregate for OpenBench compatibility, not as a representative search-selection panel. |

Profiles locate CPU work, fixed-depth nodes measure selectivity, paired fixed-depth search time
measures integrated efficiency, and paired games decide playing strength. Keep those axes separate.
Perft or qsearch shares, cutoff yield, and observational correlations do not provide a
counterfactual, so they are hypothesis generators rather than change or acceptance gates.
Cross-engine depth and NPS are context only.

Fresh-process, one-thread repeats of the same build and inputs must agree in completed depth,
static score, searched score, actual nodes, best move, and PV; exclude timing and NPS. Differences
between a tree-changing candidate and its baseline are expected and carry no default judgment.

Call a candidate **tree-preserving** only when a code- or build-level argument shows that it
preserves search decisions by construction. Exact corpus and benchmark signatures support but do
not prove that claim. Otherwise it is tree-changing and requires paired games before integration.

Objective failures include illegality, crashes, and lost or delayed source-pinned mate or material
solutions. Unresolved PV/NonPV disagreement within one build blocks an offline pass, but is not
incorrect chess without objective evidence.

### Cheap checks and full offline testing

Classify each retained search candidate before formal offline testing:

- **Tree-changing:** Cheap checks cover the mechanism and objective results, one fresh-process pass
  over the complete corpus at depth 10, one thread, one repetition, and 32 MiB Hash, plus one
  benchmark fingerprint. Full testing adds the complete release suite, a reproduced fingerprint,
  required risk checks, and paired corpus timing.
- **Tree-preserving:** Cheap checks require the equivalence argument and exact baseline corpus and
  benchmark signatures. Full testing adds the complete release suite, required risk checks, and
  paired corpus timing, which is the performance gate.

The first candidate timing pass supplies the fresh corpus repeat. A benchmark, corpus subset, or
exploratory measurement cannot replace the formal complete-corpus check. Apply these defaults
without restating them in a separate declaration or manifest.

Run deterministic, objective, fingerprint, and risk checks first, followed by the release suite,
trajectory diagnostics, required sanitizers, and finally paired timing.

### Extra risk tests

The default corpus is cold, one-threaded, and fixed-depth; the runner clears search heuristics and
the transposition table before every position. Add an extra test only for behavior outside that
model:

- **Retained search state:** clearing, persistence, aging or generation, replacement, reuse, or behavior that depends on prior searches.
- **Clock and limits:** budget calculation, elapsed-time checks, stopping or polling, limit precedence or overshoot, or publication of a stopped or incomplete iteration.
- **Threading:** shared state, worker ownership or lifecycle, synchronization, aggregation or publication, resizing, stop/restart behavior, or worker-result selection.

Merely reading history or the TT, consulting a clock, or running in threaded code does not require
an extra test. State its risk, initial sequence, pass condition, repetitions, and sanitizer need;
use the smallest existing test or probe that establishes the claim.

### Sentinel trajectories

**Latest confirmed stable-window depth** is the greatest depth `d >= 3` for which `d-2..d` keep the
same root move and score class, both adjacent PV transitions share a nonempty prefix, and objective
predicates hold. It describes a horizon-sensitive trajectory, not move quality or playing strength.

A lower stable-window depth, late root change, A-B-A event, or zero-prefix transition is diagnostic,
not a default rejection.

If a fixed horizon ends immediately after a new line appears and the flag matters to the decision,
the candidate may state before measurement a bounded confirmation that extends only that case for
both builds by at most two depths. A single flagged case cannot reject a candidate. Sentinel hard
failures are limited to objective failure or same-build nondeterminism. A task specifically about
convergence may define another aggregate measurement gate, but it must measure repeated instability
rather than agreement with the baseline move, score, or PV.

### Corpus metrics

Keep three reported axes separate. For each position `i`, let
`r_i = candidate_nodes_i / baseline_nodes_i`:

- `R_node_g = exp(mean(log(r_i)))`, the primary equal-position selectivity ratio; when reduced
  fixed-depth tree size is the claimed effect, the default gate is `R_node_g <= 0.9900`;
- `R_node_total = sum(candidate_nodes) / sum(baseline_nodes)`, the workload-weighted tree-size
  ratio; always report it as context, and use it as an additional gate only when the candidate
  records that extra gate before measurement;
- `R_time = sum(candidate_total_ns) / sum(baseline_total_ns)` for each timing pair, the aggregate
  measured search-time ratio and full-test timing metric.

Retain signature differences and useful node diagnostics, but do not make them default gates. Cost
per node and NPS may aid investigation, but neither is the timing result; never combine node and
time evidence into one score.

Use `R_node_g <= 0.9900` for hypotheses claiming a smaller fixed-depth tree. A candidate that
intentionally spends more nodes instead states an observable mechanism check, keeps the full timing
budget, and uses paired games to decide strength.

Tree-changing work must pass its mechanism and timing gates; timing within the allowed range is
neutral. Tree-preserving work must pass its timing gate. A timing gain cannot rescue a failed
claimed effect; test that benefit as another candidate.

### Paired timing

Use separate equivalent binaries built with the same compiler and preset on the same machine. Pin
both to the same physical core, avoid its sibling, and use fresh processes with identical options.
One complete 200-position invocation is one replicate. Discard one warm-up per binary, then run six
adjacent pairs as one uninterrupted batch: `BC, CB, BC, CB, BC, CB` (`B` is baseline, `C` candidate).

Let `r_1..r_6` be the aggregate candidate-to-baseline time ratios. Treat adjacent opposite-order
pairs as three `B-C-C-B` blocks and compute
`b_j = sqrt(r_(2j-1) * r_(2j))` for `j` in `1..3`. The timing decision metric is
`R_time_balanced = median(b_1, b_2, b_3)`.

All baseline signatures and all candidate signatures must agree. Retain the helper's pair, block,
order, range, and win diagnostics, but only `R_time_balanced` is a gate. Balancing order does not
make an invalid or unstable environment valid.

Tree-changing work passes at `R_time_balanced <= 1.0100` (at most 1% slower); tree-preserving work
passes at `R_time_balanced <= 0.9925` (at least 0.75% faster). Record a justified exception before
measurement.

A/A timing is an optional environment or anomaly diagnostic, not a default gate or a correction to
candidate ratios.

A single pass, cached-versus-live timing, an incomplete panel, or an unisolated run is diagnostic
only. If conditions are not stable, defer timing or use an idle machine; do not alter an external
workload without authorization.

Tree-changing work need not improve timing within its allowance. Missing the applicable threshold
is a failure. Mark a test unresolved only for an identified setup or collection failure or an
unavailable required condition; candidate-caused nondeterminism fails. Rerun only after an
identified setup failure, and report effects no more precisely than the replicate spread supports.

### Integration checks

Before refreshing the operational baseline, require the integrated build's complete corpus
signatures and applicable benchmark fingerprint to match the retained offline- or game-tested
candidate. Change identity and tests remain necessary but do not substitute for this search
identity check.

## Durable findings

| Area | Finding and consequence |
|---|---|
| Capture ordering | CaptureHistory reduced nodes but increased total search time. SW-11's depth-1 late losing-capture rule reduced `R_node_g` to 0.9883 but raised total nodes to 1.0028 and slowed balanced corpus time to 1.0414 with zero wins in six pairs. Preserve the current SEE bands and do not retry that exact rule. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. Do not retry `stand_pat + captured_value + margin <= alpha_before_move` with the tested 200/300/400 margins; they skipped real NonPV cutoffs. |
| LMR | The tested one-ply protection for combined history at least 1024 did not change the sampled fail-lows. SW-16's extra reduction at combined history at most -1 reached `R_node_g = 0.9937`, short of the smaller-tree gate. Do not retry either exact shape; different formulas or re-search sequencing require new evidence. |
| Clock and limits | The tested next-iteration time predictor stopped too early for every sampled multiplier. Preserve explicit `movetime` as a hard request. |
| Futility | Guarded reverse futility pruning is retained. Main-search futility must preserve checking quiets by filtering only nonchecking quiets. |
| LMP families | The current depth-2 after-twelve negative-history rule passed OpenBench #27. Unconditional depth-1 pruning caused PV/NonPV disagreement; tested fixed-threshold, history-gated depth-1, and improving-aware variants were not compelling. Revisit LMP only with a materially different safety signal or profiling evidence. |
| TT prefetch | SW-13's parent-issued child-cluster prefetch preserved exact corpus and benchmark signatures and reduced balanced corpus time to 0.9823 with 6/6 paired wins. Retain it; games were skipped because the change was tree-preserving. |
| Release IPO | SW-14's CMake target-scoped Release IPO preserved exact Clang/GCC benchmark and corpus signatures and reduced balanced Clang corpus time to 0.9358 with 6/6 paired wins. Keep IPO on Latrunculi's object library and executables while leaving third-party static libraries and non-Release configurations unchanged. |
| Node accounting | Single-writer relaxed load/store was 2.17% slower, while worker-local counting with periodic publication was neutral in exploration despite removing locked hot-path increments. Retain the current atomic counter. Revisit only if profiling on a future baseline identifies node accounting as a material bottleneck or a compiler or architecture change gives a concrete reason to retest. |
| SW-20 throughput | Reusing SW-20 history or picker work produced no repeatable gain under the current timing method. The primary counter-hint lookup remained, and narrow score reuse traded fewer instructions for lower IPC and more branch misses. Revisit only with a proposal that removes common-path work. |

## Search guardrails

Borrow mechanisms, not code or tuning constants, from pinned engines or the Chess Programming Wiki;
revalidate them in Latrunculi. Do not add machinery merely to raise displayed depth. Profile before
hot-path or architectural work, and add search state only for a concrete consumer.
