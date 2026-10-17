# Latrunculi tuning

The tuning tool fits the linear handcrafted evaluation from recorded games.
It prepares a grouped dataset, fits candidate weights, and verifies that an
engine contains those weights. [OpenBench testing](../openbench/README.md#strength-tests)
measures playing strength.

Set up the [shared Python environment](../README.md#setup), then run from the
repository root:

```bash
DATA=tools/tuning/output/prepared
FIT=tools/tuning/output/fit

./bin/tune prepare \
  --engine build/release/latrunculi --output "$DATA" /path/to/games.pgn.tar
./bin/tune fit --dataset "$DATA" --output "$FIT"
./bin/tune verify "$FIT" --engine build/candidate/latrunculi
```

Inputs may be `.pgn`, `.pgn.bz2`, or OpenBench `.pgn.tar`. Keep generated files
under ignored `tools/tuning/output/`.

For an existing OpenBench corpus whose PGNs are not local, use
`./bin/openbench-client` with the
[status and download commands](../openbench/README.md#inspect-and-download)
to retain its PGN archive, checksum, and workload provenance together. Verify
the workload revisions against the intended baseline. Pass the downloaded
`.pgn.tar` directly to `prepare`; extraction is unnecessary. Preparation
validates the games; workload completion and pass/fail flags do not establish
corpus usability.

## Prepared data

`prepare` writes `manifest.json` and `development.jsonl`. It groups games by
starting position so related examples stay together during validation. The
[OpenBench tuning workload](../openbench/README.md#tuning-corpus) describes
how to collect fresh games. Once prepared, `fit` needs the dataset without
the original PGNs or exporter.

## Fitting and verification

`fit` compares candidate weights on held-out opening groups, then writes
`cross-validation.json` and `candidate.json` with the selection evidence and
exact integer weights. Its numerical policy is in
[`FIT_POLICY`](fit.py). Numerical support alone does not establish playing
strength; fitted evaluation changes also need [local validation](../analysis/README.md#validation)
and [strength testing](../openbench/README.md#strength-tests).

`verify` checks the compiled coefficients and evaluation invariants against
the proposed weights and prints JSON. It can run again after rebuilding
without the dataset. `run.json` records the inputs and policy used for a fit;
matching completed checkpoints can be reused after an interruption.

### Dataset requirements

The current policy requires at least 40,000 valid games and 20,000 retained
opening groups. Preparation samples at most six fixed quantiles per game from
ply 8, settles captures/check evasions, and discards terminal positions. Settled
four-field FENs are deduplicated: agreeing results retain one copy; conflicting
results discard all copies. Every retained evaluation must reconstruct exactly.

Starting FENs identify groups across archives; games without one get their own
group. Each group has total weight one. The schema-2 dataset header records
baseline coefficients; the manifest records provenance and filtering.

### Selection and review

The implemented `FIT_POLICY` is authoritative; do not adjust it to rescue a fit.
It uses five deterministic, opening-disjoint folds, calibrating the Texel scale
and fitting MG/EG weights on the other four. Variables need support from at least
128 groups in every training complement. L-BFGS-B fits regularized weights;
rounded checkpoints compete on exact training loss plus penalty, including the
unchanged baseline.

Pawn MG and PSQT/mobility anchors stay fixed, non-pawn PSQTs retain file symmetry,
and changes are bounded by 100 cp plus material-specific ranges. Regularization
counts all tied coefficients. Nonlinear king danger, phase/scaling formulas,
tempo, and search parameters remain outside the fit.

Eligibility uses held-out opening-group bootstrap comparisons: a positive overall
90% lower improvement bound and no negative upper bound in supported phase buckets.
Choose the strongest eligible penalty within one standard error of the best mean
fold improvement, then refit all data. If none qualify, retain the baseline.

Review large changes and bound hits in `candidate.json`; they invite inspection,
not automatic rejection. Apply the exact selected coefficients to
`src/eval/parameters.hpp`, rebuild, and run `verify`. Preserve its JSON output and
the candidate/base revisions. Verification checks the fit, not playing strength.

### Resume and reuse

The current dataset manifests and fit artifacts use format version 5. Start a
fresh `prepare` and `fit` after this tool reorganization; older outputs are retained
as evidence and are not migrated. The engine feature schema is separate from the
artifact format and remains version 2.

`run.json` pins schema, policy, inputs, tools, and dependencies; final reports mark
completion. Rerunning `prepare` validates an existing dataset; rerunning `fit`
reuses completed matching checkpoints and restarts interrupted units. Changed
inputs, evaluation, schema, tools, dependencies, or policy require fresh output.
A search-only change may preserve numerical evidence, but the resulting engine
still needs relevant validation against its current baseline. Keep original
provenance and use the [evidence guidance](../../docs/engine-development.md#reusing-evidence).

## Implementation

- `dataset.py` prepares and validates PGNs, exported features, and provenance.
- `model.py` loads sparse data and calculates evaluations, loss, and gradients.
- `fit.py` owns the numerical policy, constraints, optimization, and grouped validation.
- `artifacts.py` reads and writes checkpoints, provenance, weights, and reports.
- `tune.py` handles commands and verifies the compiled engine.

Fit provenance hashes all tuning modules and their package initializers. Modules
use package imports; use `./bin/tune` to run the CLI. Run the
[tool tests](../README.md#tests) after changing the implementation.

Background: [Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
and [Ethereal's tuning paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf).
