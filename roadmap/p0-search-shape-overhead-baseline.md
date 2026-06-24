# P0 Search-Shape Instrumentation Overhead Baseline

## Summary

Date: 2026-06-24

Purpose: establish a control benchmark for the P0 search-shape instrumentation.
This is a local direct-UCI baseline, not a strength result.

Commits:

- Pre-P0: `291010377f74ef9838b24d9dcc26ed48ee90ee1b`
- P0/current: `9d7f7ca02a2b55102dcc55576a9eaf0de7ffff75`

Raw TSVs:

```text
/home/rick/code/AGENTS/latrunculi/scratch/p0-overhead-baseline-20260624-123221/raw
```

## Method

Builds compared:

- `pre-p0-release-dev`: clean worktree at
  `291010377f74ef9838b24d9dcc26ed48ee90ee1b`, stats disabled.
- `current-release-dev`: current P0 commit, stats disabled.
- `current-release-stats-dev`: current P0 commit, stats enabled.

Benchmark command shape:

```bash
python3 bench/direct_uci_bench.py \
  --engine <engine> \
  --positions all \
  --threads 1,4 \
  --movetime 1000
```

Stats-build samples used:

```bash
python3 bench/direct_uci_bench.py \
  --engine ./build/release-stats-dev/bin/latrunculi \
  --positions all \
  --threads 1,4 \
  --movetime 1000 \
  --capture-stats
```

Each build was sampled five times over the six-position direct-UCI suite and
both thread counts, for 60 rows per build.

## Aggregate Results

| Build | Rows | Avg NPS | Avg Nodes | Avg Depth |
|---|---:|---:|---:|---:|
| `pre-p0-release-dev` | 60 | 4,208,047 | 4,211,872 | 14.77 |
| `current-release-dev` | 60 | 4,138,253 | 4,140,904 | 14.80 |
| `current-release-stats-dev` | 60 | 4,074,654 | 4,078,473 | 14.73 |

Paired deltas use per-position/per-thread averages before computing the ratio.

| Comparison | Threads | NPS Delta | Nodes Delta | Depth Delta |
|---|---:|---:|---:|---:|
| P0 `release-dev` vs pre-P0 `release-dev` | all | -1.99% | -2.01% | +0.03 |
| P0 `release-dev` vs pre-P0 `release-dev` | 1 | -2.86% | -2.86% | -0.10 |
| P0 `release-dev` vs pre-P0 `release-dev` | 4 | -1.13% | -1.15% | +0.17 |
| stats build vs current `release-dev` | all | -0.70% | -0.70% | -0.07 |
| stats build vs current `release-dev` | 1 | +0.28% | +0.26% | -0.03 |
| stats build vs current `release-dev` | 4 | -1.69% | -1.65% | -0.10 |

## Stats-Shape Baseline

These are simple row averages from the stats-enabled samples. They are useful as
a rough control for later P1-P4 experiments, not as tuned targets.

| Field | Mean |
|---|---:|
| Beta cutoffs | 185,231.47 |
| Cutoff early | 90.88% |
| Cutoff late | 9.12% |
| Cutoff avg index | 1.36 |
| PVS re-searches | 652.22 |
| Aspiration fail-low | 0.17 |
| Aspiration fail-high | 0.00 |
| Aspiration re-searches | 0.17 |
| Main TT hit | 25.02% |
| Main TT cutoff | 94.33% |
| QTT hit | 0.00% |
| QTT cutoff | 0.00% |
| QNode | 65.63% |
| EBF | 0.50 |
| Cumulative EBF | 1.06 |

## Interpretation

The stats-disabled P0 build measured about 2% lower NPS than the pre-P0 build in
this local fixed-time direct-UCI run, with essentially unchanged average depth.
That should be treated as the current control-band signal, not as proof of a
confirmed instrumentation overhead.

Supporting shape checks:

- `release-dev` `search.cpp.o` contains no `SearchStats` instrumentation symbols.
- `release-dev` binary text size is effectively unchanged:
  - pre-P0 `release-dev`: `563823`
  - P0 `release-dev`: `563678`
  - P0 `release-stats-dev`: `567990`
- The stats-enabled build measured about 0.7% lower NPS than current
  `release-dev`, which is acceptable for a diagnostic build.

Use this file as a baseline control for later performance patches. A future
change should not be judged on a single sub-2% direct-UCI delta alone; rerun the
same control, then escalate to fixed-depth or profiler evidence if the loss
repeats.
