# Search Knowledge

This document retains durable evidence and constraints for Latrunculi's search algorithm.
[Strength Development](strength.md) owns operational baselines, active work, pending experiments,
and short-to-medium-term results. Once an experiment family is no longer operationally relevant,
distill its lasting mechanism findings here before removing its coordination record.

## Evidence model

| Panel | Purpose and use |
|---|---|
| Objective tests | Source-pinned mate and material checks plus legality and crash detection. These decide correctness, not performance. |
| Tactical Screen corpus | All 200 Arasan positions in `tools/measurements/search.epd`; use for deterministic pruning, reductions, move ordering, aspiration, and comparable search changes. Source `bm`/`am` labels are context, not oracles. |
| Sentinels | Four cases in `tools/measurements/search-sentinels.epd`; use for descriptive trajectory diagnostics, not as move-quality oracles. |
| Benchmark fingerprint | Six fixed positions at depth 13; use its deterministic aggregate for OpenBench compatibility, not as a representative search-selection panel. |

Within Latrunculi, fixed-depth nodes measure selectivity and paired fixed-depth aggregate search
time measures search efficiency. Neither is playing strength. Cross-engine depth and NPS are
context only. The corpus is tactical offline evidence, not Elo; paired games decide whether a
changed tree is useful.

Fresh-process, one-thread repeats of the same build and inputs must agree exactly in completed
depth, score, actual nodes, best move, and PV; exclude timing and NPS. For a tree-changing
candidate, differences from the baseline in score, best move, or PV are expected and have no
default positive or negative meaning. Only exact-tree candidates must match baseline signatures.

**Latest confirmed stable-window depth** is the greatest depth `d >= 3` for which `d-2..d` keep
the same root move and score class, both adjacent PV transitions share a nonempty prefix, and
objective predicates hold. It is a horizon-sensitive trajectory description, not completed depth,
move quality, or playing strength.

Objective failures include illegality, crashes, and lost or delayed source-pinned mate or material
solutions. PV/NonPV disagreement compares search modes within one build and is a
search-consistency signal: diagnose it and block qualification while unresolved, but do not call
it incorrect chess without objective evidence.

### Default search panel

Classify each search experiment before implementation:

- A **tree-changing** candidate intentionally changes search signatures. Screen runs focused
  mechanism and objective checks, one fresh-process pass over the complete corpus at depth 10,
  one thread, one repetition, and 32 MiB Hash, plus one benchmark fingerprint. Qualification runs
  the complete release suite, a reproduced candidate benchmark fingerprint, only risk-specific
  checks justified by the mechanism, and the paired corpus timing panel.
- An **exact-tree** candidate must match the baseline corpus and benchmark signatures in Screen.
  Qualification runs the complete release suite, the paired corpus timing panel, and any justified
  risk-specific checks. Timing is its primary performance gate.

The first candidate pass in the timing panel supplies the fresh corpus repeat; apply the
reproducibility rule above. Do not substitute the benchmark or a corpus subset for the complete
Screen corpus.

Within Qualification, run cheap decisive work before environment-sensitive timing: the complete
release suite and deterministic, objective, fingerprint, and risk checks; applicable trajectory
diagnostics; required sanitizers; any needed A/A calibration; then paired candidate timing.

### Sentinel trajectories

Sentinel comparisons describe each build's trajectory. A lower stable-window depth, a late root
change, an A-B-A event, or a zero-prefix transition is a diagnostic flag, not a default
Qualification failure.

If a fixed horizon ends immediately after a new line appears and the flag matters to the decision,
the active task may predeclare a bounded confirmation that extends only that case for both builds
by at most two depths. A single flagged case cannot reject a candidate. Sentinel hard failures are
limited to objective failure or same-build nondeterminism. A task specifically about convergence
may predeclare another aggregate efficacy gate, but it must measure repeated instability rather
than agreement with the baseline move, score, or PV.

### Corpus metrics

Keep three reported axes separate. For each position `i`, let
`r_i = candidate_nodes_i / baseline_nodes_i`:

- `R_node_g = exp(mean(log(r_i)))`, the primary equal-position selectivity ratio and default
  Screen efficacy metric;
- `R_node_total = sum(candidate_nodes) / sum(baseline_nodes)`, the workload-weighted tree-size
  ratio; always report it as context, and use it as an additional Screen gate only when the task
  predeclares one;
- `R_time = sum(candidate_total_ns) / sum(baseline_total_ns)` for each timing pair, the aggregate
  measured search-time ratio and Qualification timing metric.

Also retain baseline-relative signature differences and useful diagnostics such as the median node
ratio, counts, and largest node increases, but do not turn them into additional default gates. Cost
per node and NPS can help an investigation, but a changed tree alters the mix of cheap and expensive
nodes; neither is the operational timing result. Never combine node and time evidence into one
score.

A tree-changing candidate must pass its predeclared Screen selectivity gate and show no repeatable
material search-time regression in Qualification. Timing inside the predeclared guard band is
neutral; it need not demonstrate another gain. An exact-tree candidate must show a search-time
improvement beyond its predeclared material threshold. A timing gain may not rescue a failed
selectivity hypothesis; testing that benefit requires a new task with a different predeclared
hypothesis.

### Paired timing

Timing evidence must use equivalent hashed binaries built with the same compiler and preset on the
same machine. Pin both binaries to the same physical core, avoid work on its sibling, and use fresh
processes with identical explicit options. Treat a complete 200-position process invocation as one
timing replicate, not as 200 independent samples. Discard one complete warm-up pass per binary,
then run six adjacent corpus pairs as one uninterrupted batch with three in each order:
`BC, CB, BC, CB, BC, CB`.
Here `B` is the baseline and `C` is the candidate. Predeclare the order.

`total_ns` begins immediately before `start_search()` and ends after the search threads finish; it
excludes board construction, static evaluation, heuristic and TT clearing, process startup, and
output. Report every pair's aggregate search-time ratio, the median and range, candidate win count,
and results separated by execution order. Validate that all baseline signatures agree and all
candidate signatures agree. An execution-order conflict is unresolved evidence, not a result to
average into an apparent win.

Use the current environment default `epsilon` recorded in `strength.md` as the largest timing
regression worth treating as neutral. Reference its calibration before Screen. If none is current,
predeclare the proposed default and record that calibration must be established if Screen passes. A
task may instead predeclare a justified deviation from the current default. For an exact-tree task,
also predeclare `delta > epsilon`, the smallest timing improvement worth accepting.

Before candidate paired timing, establish any needed A/A calibration with the same six-pair
protocol and byte-identical binaries. Its overall median and both execution-order medians must lie
within `1 +/- epsilon / 2`. The calibration remains current only while the machine, compiler, build
preset, search profile, and relevant operating conditions are unchanged. Record it as current in
`strength.md`; refresh it when any of those conditions change.

A single pass, cached-versus-live timing, an incomplete panel, or an unisolated run is diagnostic
only. If stable conditions cannot be established, defer the timing decision or use an idle machine;
do not alter an external workload without authorization.

First check for a material order effect: one execution-order median at most `1 - epsilon` while the
other is at least `1 + epsilon` leaves the panel unresolved. Direction differences inside the
neutral band do not. Otherwise, a tree-changing candidate passes timing when the overall median
paired `R_time` and both execution-order medians are at most `1 + epsilon`; improvement is not
required. An exact-tree candidate passes when all three medians are at most `1 - delta`.
Individual ratios and win counts are diagnostics, not independent votes. Rerun only after an
identified setup failure. Keep raw precision in artifacts, but report rounded effects rather than
precision unsupported by the calibration.

## Durable findings

| Area | Finding and consequence |
|---|---|
| Determinism and Hash | All 33 fixed-node triplicate groups were exact, dev/stats signatures agreed, and 32 versus 256 MiB Hash changed no result. Depth-to-depth movement is convergence behavior, not run-to-run instability. |
| Objective guards | Mate in one was stable at depth 1 and 46 nodes, mate in two at depth 3 and 767 nodes, and the loose-rook capture at depth 1 and 29 nodes. Keep these as tests, not performance rows. |
| Convergence | Three of four pilot moves were not reproduced at 524,288 nodes and had no shared counter profile. The audit found five late root changes, one sign flip, one A-B-A, 46 cp late-jump p90, 61.5% median PV survival, and five zero-prefix transitions. Pilots describe trajectories; they are not move-quality oracles. |
| Capture ordering | Late captures succeeded in 29,374 of 504,598 attempts: 7.20% in exact-SEE-good and 0.47% in exact-SEE-bad. CaptureHistory reduced nodes to 0.9716 but increased time to 1.0646, including 1.4129 at start position. Preserve current SEE bands unless a distinct proposal earns a new test. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. Delta margins 200/300/400 would have skipped 3,200/732/206 real NonPV cutoffs. Do not retry `stand_pat + captured_value + margin <= alpha_before_move` at those margins. |
| LMR | All 107 sampled high-history reduced fail-lows remained fail-lows at full depth. Do not retry the tested one-ply protection for combined history at least 1024; different formulas or re-search sequencing remain eligible. |
| Clock | Unfinished iterations consumed median node shares of 24–26%, but all 210 timed searches matched fresh-depth results. Predictor `T_d + m * (T_d - T_(d-1)) >= allocated_time` fired prematurely for `m` in `{1,2,4}`. Explicit `movetime` remains a hard request. |
| Futility | Guarded reverse futility pruning passed OpenBench and was retained. Main-search futility preserves quiet checks by filtering only nonchecking quiets; SW-08 was strength-neutral and retained by explicit correctness policy. SW-10's checks-first ordering passed Screen, then stopped under a retired single-sentinel gate. It established neither regression nor qualification, so the exact shape remains open. |
| Depth-1 LMP family | SW-12's unconditional after-eight rule cut nodes to 0.8271 but caused a 76/114 NonPV/PV disagreement. Negative-history gating in SW-18 passed offline at 0.9843 with objective checks passing and trajectory diagnostics recorded; moving it after six in SW-19 added no benefit. Do not retry unconditional after-eight or the after-six/after-four threshold path. |
| Depth-2 LMP family | SW-20's after-twelve negative-history rule passed offline at 0.9513. Its external disposition is tracked in `strength.md`. A `-64` tier for moves 11–12 in SW-23 never fired; after-ten and after-eleven rules in SW-24/SW-25 increased nodes to ratios 1.0043/1.0035, driven by `arasan20-48` and `arasan20-89`. Do not tune this boundary again without a materially different safety signal. |
| SW-20 throughput | SW-21 produced a nominal 0.94% corpus timing gain but tied the benchmark; SW-22 was order-dependent and also tied the benchmark. Rearranging SW-20 history, hint, and check-classification work has no demonstrated robust throughput gain. |
| Historical depth gap | In the pinned release gauntlet, Latrunculi's median reported depth was 12 versus 16 for 4ku and Willow and 18 for Weiss. This motivates selectivity and throughput work but is neither a current benchmark nor an acceptance gate. |
| Open mechanisms | Null move remains eligible for a materially different experiment; SW-04 rejected only the `static_eval >= beta` eligibility gate, and the audit lacked eligibility and counterfactual-failure denominators. |

## Reference evidence

The immutable audit anchor is `470a3d75c31a13e5f791dc0ee2476c6974be346d`. Do not modify
`search-001-470a3d7/`, `sa-02-470a3d7/`, `sa-03-470a3d7/`,
`sa-03-capture-history-b415f5a/`, `sa-04-470a3d7/`, `sa-05-470a3d7/`,
`sa-05-verifier-25bb133/`, or `sa-07-470a3d7/`. Commit `8c0d46d` retains the deleted audit report
for forensic replay.

Reference baseline `c6eb554` has sentinel horizon/stable-window-depth pairs `startpos` 14/11,
`arasan20-16` 16/16, `pilot14-g171-abrupt` 18/14, and `pilot15-g078-secondary` 13/13. Use
`sw-08-6c01040/derived/sentinel-summary.tsv`; older `sa-02-470a3d7` rows are audit evidence, not a
current comparison baseline. At the next operational-baseline refresh, store retained sentinel
trajectories beside the cached corpus instead of referring through a historical candidate artifact.

The exact rejected CaptureHistory patch is
`tools/measurements/output/sa-03-capture-history-b415f5a/meta/candidate.patch`. Aggregate counters
may overlap iterations or aspiration attempts; require mechanism-specific denominators when they
decide a change.

## Search guardrails

Ideas may draw from pinned reference engines and the Chess Programming Wiki, but must be
revalidated in Latrunculi; borrow mechanisms, not code or tuning constants.

Do not introduce broader machinery merely to raise displayed depth. Use bounded profiling before
optimizing evaluation, repetition detection, make/unmake, or TT work. Introduce an `improving`
flag only with a concrete LMP, LMR, or RFP candidate. The completed search audit did not justify
IIR, ProbCut, singular extensions, evaluation caching, or incremental state as immediate search
follow-ups. Treat cache or incremental-state proposals as separate exact-behavior experiments and
profile the opportunity before accepting their complexity.
