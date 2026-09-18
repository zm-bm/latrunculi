# Search Knowledge

This document retains durable evidence and constraints for Latrunculi's search algorithm.
[Playing Strength Development](playing-strength.md) owns operational baselines, active work,
pending experiments, and short-to-medium-term results. Once an experiment family is no longer
operationally relevant, distill its lasting mechanism findings here before removing its
coordination record.

This guide owns search-specific default panels, reproducibility rules, and metric meaning.
`playing-strength.md` owns lifecycle state, task-specific thresholds, and predeclared deviations.

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
depth, static score, searched score, actual nodes, best move, and PV; exclude timing and NPS. For
a tree-changing candidate, differences from the baseline in static or searched score, best move,
or PV are expected and have no default positive or negative meaning.

Classify a candidate as **exact-tree** only when a code- or build-level equivalence argument shows
that it preserves search decisions by construction. Exact corpus and benchmark signatures
corroborate that argument but cannot establish it alone. Otherwise classify the candidate as
tree-changing and require paired games before integration.

**Latest confirmed stable-window depth** is the greatest depth `d >= 3` for which `d-2..d` keep
the same root move and score class, both adjacent PV transitions share a nonempty prefix, and
objective predicates hold. It is a horizon-sensitive trajectory description, not completed depth,
move quality, or playing strength.

Objective failures include illegality, crashes, and lost or delayed source-pinned mate or material
solutions. PV/NonPV disagreement compares search modes within one build and is a
search-consistency signal: diagnose it and block qualification while unresolved, but do not call
it incorrect chess without objective evidence.

### Default search panel

These are formal Qualify-stage panels. Classify each concrete search candidate before
Qualification implementation or Screen:

- A **tree-changing** candidate intentionally changes search signatures. Screen runs focused
  mechanism and objective checks, one fresh-process pass over the complete corpus at depth 10,
  one thread, one repetition, and 32 MiB Hash, plus one benchmark fingerprint. Qualification runs
  the complete release suite, a reproduced candidate benchmark fingerprint, only risk-specific
  checks justified by the mechanism, and the paired corpus timing panel.
- An **exact-tree** candidate must supply the equivalence argument above and match the baseline
  corpus and benchmark signatures in Screen. Qualification runs the complete release suite, the
  paired corpus timing panel, and any justified risk-specific checks. Timing is its primary
  performance gate.

The first candidate pass in the timing panel supplies the fresh corpus repeat; apply the
reproducibility rule above. Do not substitute the benchmark or a corpus subset for the complete
Screen corpus.

Explore may use focused probes, a corpus subset, or the complete corpus, but those results remain
exploratory and cannot satisfy Screen or Qualification.

The default corpus is a cold, one-thread, fixed-depth panel: the measurement runner clears search
heuristics and the transposition table before every position. Add a targeted panel only when a
candidate changes or relies on behavior outside that model:

- **Retained search state:** clearing, persistence, aging or generation, replacement, reuse, or
  behavior that depends on prior searches.
- **Clock and limits:** budget calculation, elapsed-time checks, stopping or polling, limit
  precedence or overshoot, or publication of a stopped or incomplete iteration.
- **Threading:** shared state, worker ownership or lifecycle, synchronization, aggregation or
  publication, resizing, stop/restart behavior, or worker-result selection.

Merely reading history or the TT, consulting a clock, or executing in threaded code does not
trigger a panel. For each triggered panel, freeze the risk, initial conditions and transition
sequence, covered configurations, observable invariants and tolerances, repetition and
reproducibility rule, sanitizer applicability, and reject and qualify rules. Use the smallest
existing tests or task-specific probes that establish those claims; there are no category-wide
default commands or case lists.

Within Qualification, run cheap decisive work before environment-sensitive timing: the complete
release suite and deterministic, objective, fingerprint, and risk checks; applicable trajectory
diagnostics; required sanitizers; then paired candidate timing.

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

- `R_node_g = exp(mean(log(r_i)))`, the primary equal-position selectivity ratio; use it as the
  default Screen efficacy metric when reduced fixed-depth tree size is the claimed mechanism;
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

Every tree-changing candidate must predeclare a mechanism-aligned Screen efficacy metric and
threshold. Use `R_node_g` by default for a pruning, reduction, move-ordering, or comparable
hypothesis that claims a smaller fixed-depth tree. If a candidate intentionally spends more nodes
to improve search quality, do not require node reduction: predeclare an observable mechanism
result, retain the Qualification timing budget, and use paired games to decide strength.

A tree-changing candidate must pass its predeclared Screen efficacy gate and show no repeatable
material search-time regression in Qualification. Timing inside the predeclared guard band is
neutral; it need not demonstrate another gain. An exact-tree candidate must show a search-time
improvement beyond its predeclared material threshold. A timing gain may not rescue a failed
efficacy hypothesis; testing that benefit requires a new task with a different predeclared
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
output. Let `r_1..r_6` be the six aggregate candidate-to-baseline time ratios. Treat adjacent
opposite-order pairs as three `B-C-C-B` blocks and compute
`b_j = sqrt(r_(2j-1) * r_(2j))` for `j` in `1..3`. The timing decision metric is
`R_time_balanced = median(b_1, b_2, b_3)`.

Report every pair ratio, all three balanced block ratios, `R_time_balanced`, the unbalanced
six-pair median and range, candidate win count, and BC and CB medians. Validate that all baseline
signatures agree and all candidate signatures agree. Everything except `R_time_balanced` is
diagnostic: individual ratios, execution-order medians, their difference or spread, and win counts
are not independent gates. Balancing reduces stable order bias; it does not make an invalid or
unstable environment valid.

For a tree-changing task, predeclare `epsilon` as the largest timing regression worth treating as
neutral. For an exact-tree task, predeclare `delta > 0` as the smallest timing improvement worth
accepting. The thresholds are independent.

A/A timing is an optional diagnostic for a new or changed measurement environment or for
investigating anomalous paired results. It is not a default panel or hard gate, does not replace the
candidate panel, and must not be subtracted from candidate ratios as a correction.

A single pass, cached-versus-live timing, an incomplete panel, or an unisolated run is diagnostic
only. If stable conditions cannot be established, defer the timing decision or use an idle machine;
do not alter an external workload without authorization.

A tree-changing candidate passes timing when `R_time_balanced <= 1 + epsilon`; improvement is not
required. An exact-tree candidate passes when `R_time_balanced <= 1 - delta`. Otherwise the
candidate fails the timing gate. Leave the panel unresolved only when it is invalid or incomplete
because of an identified setup or data-collection failure or an unavailable required condition.
Candidate-caused signature nondeterminism remains a failure under the reproducibility rule. Rerun
only after an identified setup failure. Keep raw precision in artifacts, but report rounded effects
rather than precision unsupported by the replicate spread.

### Integration identity

Before refreshing the operational baseline, require the integrated build's complete corpus
signatures and applicable benchmark fingerprint to match the retained qualified or externally
tested candidate. Patch identity and tests remain necessary but do not substitute for this search
identity check.

## Durable findings

| Area | Finding and consequence |
|---|---|
| Evidence interpretation | Repeated fixed-node searches were deterministic across the sampled builds and Hash sizes. Objective mate, material, legality, and crash checks decide correctness; depth-to-depth trajectories and overlapping aggregate counters are diagnostic. Require mechanism-specific denominators when counters decide a change. |
| Capture ordering | CaptureHistory reduced nodes but increased total search time. Preserve the current SEE bands unless a distinct proposal demonstrates a net benefit. |
| Qsearch | Ordinary exact-SEE-negative captures are already excluded. Do not retry `stand_pat + captured_value + margin <= alpha_before_move` with the tested 200/300/400 margins; they skipped real NonPV cutoffs. |
| LMR | The tested one-ply protection for combined history at least 1024 did not change the sampled fail-lows. Do not retry that shape; different formulas or re-search sequencing require new evidence. |
| Clock and limits | The tested next-iteration time predictor stopped too early for every sampled multiplier. Preserve explicit `movetime` as a hard request. |
| Futility | Guarded reverse futility pruning is retained. Main-search futility must preserve checking quiets by filtering only nonchecking quiets. |
| LMP families | Unconditional depth-1 pruning after eight moves caused PV/NonPV disagreement, and the tested after-six/after-four threshold path added no benefit. The current depth-2 after-twelve negative-history rule passed OpenBench #27; the tested extra tier and after-ten/after-eleven boundaries added no benefit. Do not retune these shapes without a materially different safety signal. |
| SW-20 throughput | Reusing SW-20 history or picker work produced no repeatable gain under the current timing method. The primary counter-hint lookup remained, and narrow score reuse traded fewer instructions for lower IPC and more branch misses. Revisit only with a proposal that removes common-path work. |

The detailed search audit for revision `470a3d7` remains available in Git history at commit
`8c0d46d`; the table above is its retained synthesis.

## Search guardrails

Ideas may draw from pinned reference engines and the Chess Programming Wiki, but must be
revalidated in Latrunculi; borrow mechanisms, not code or tuning constants.

Do not introduce broader machinery merely to raise displayed depth. Use bounded profiling before
hot-path or architectural optimization, and require a concrete consuming candidate before adding
new search state.
