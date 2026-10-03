---
name: tune-latrunculi-evaluation
description: Prepare data, fit linear Latrunculi evaluation weights, and retain a locally verified candidate. Use for supplied PGNs, prepared datasets, or resuming a fit; arrange fresh baseline self-play only when explicitly requested. Stop at local testing.
---

# Tune Latrunculi Evaluation

Resolve paths from the Latrunculi checkout. Read the current [development board](../../../docs/engine-development.md), [fitting checks](references/fitting-checks.md), and [candidate revision rules](../explore-latrunculi-change/references/candidate-revisions.md). Use the [tuning guide](../../../tools/tuning/README.md) for commands. Work on one local CPU-sensitive task at a time.

## Prepare and fit

Use the requested item or retain one in **Issues and leads**. Inspect its baseline, input provenance, and prior outputs. Preserve historical evidence; reuse prepared data or a fit only when schema, baseline evaluation, and policy remain applicable.

Use supplied PGNs or a prepared dataset. If fresh self-play was explicitly requested, hand the pinned baseline to `manage-latrunculi-openbench` under its [tuning corpus rules](../manage-latrunculi-openbench/references/workloads-and-decisions.md#tuning-corpus). Record the workload and exact resume condition, then stop while data is pending; do not poll or restart it automatically. A stopped or incomplete workload may still yield usable data if preparation meets its requirements.

Build the operational baseline in an isolated worktree when preparation needs an exporter. Run `prepare` and `fit` under ignored `tools/tuning/output/`, reusing only matching completed checkpoints. Keep missing inputs or incomplete work in **Issues and leads** with available outputs and the exact resume condition. Review `cross-validation.json` and `candidate.json`; do not replace the versioned method with ad hoc thresholds or parameter variants.

## Implement and hand off

Recheck the current baseline and stale-evidence rules before implementation. If the fit lacks numerical support or its integer weights are unchanged, record a scoped null in **Recent results** without a candidate. For supported changes, review large changes and bound hits, apply the exact weights to `src/eval/parameters.hpp` in an isolated current-baseline worktree, and retain one engine-only local commit.

Build that commit and run `verify <fit directory> --engine <candidate binary>`. Retain its JSON output with the fit and exact candidate/base SHAs. Correct only mechanical application errors via a fresh revision; behavior changes return to **Issues and leads**. Keep interrupted or unresolved verification in **Candidates for local testing**, naming what remains, and preserve old revisions when refreshing stale work.

Move a locally verified candidate to **Candidates for local testing** with its claim, tree classification, compact fit result, and retained evidence; hand it to `test-latrunculi-candidate`. Verification is not a complete offline pass. Preserve fit inputs and reports needed for later checks, remove temporary worktrees, and report workspace state.

Create only the retained local candidate commit; leave bookkeeping uncommitted. Do not run formal offline testing, submit a strength test, push, or integrate.
