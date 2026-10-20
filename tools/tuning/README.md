# Latrunculi tuning

`./bin/tune` fits linear evaluation weights from PGNs and checks them against a
compiled engine. It needs the [shared Python environment](../README.md#setup).
Run commands from the repository root.

## Prepare and fit

Supply an engine built with the starting weights and one or more `.pgn`,
`.pgn.bz2`, or `.pgn.tar` files. Use archives without extracting them. Replace the
engine and PGN paths below as needed:

```bash
DATA=tools/tuning/output/my-pass/prepared
FIT=tools/tuning/output/my-pass/fit

./bin/tune prepare \
  --engine build/release-dev/latrunculi --output "$DATA" \
  /path/to/games.pgn.tar
./bin/tune fit --dataset "$DATA" --output "$FIT"
```

Keep the engine and PGNs unchanged while `prepare` runs. It writes
`manifest.json` and `development.jsonl`. Afterward, `fit` needs only that dataset.
It writes validation results to `cross-validation.json` and selected integer
weights to `candidate.json`.

## Apply and verify

Check `cross_validation_supported` in `candidate.json`. If false, keep the baseline
weights. Otherwise, review the weights and flagged large changes or bound hits
before applying them. Flags do not automatically disqualify a fit.

Copy the exact weights into [`src/eval/parameters.hpp`](../../src/eval/parameters.hpp)
yourself, then rebuild and verify:

```bash
cmake --build --preset release-dev
./bin/tune verify "$FIT" --engine build/release-dev/latrunculi
```

`verify` rejects unsupported candidates and checks the compiled weights and
evaluation invariants against the fit. Save its JSON output with the engine
revision. You can verify again with just the fit directory and engine binary.
This does not measure playing strength.

## Resume and reuse

Use new directories under ignored `tools/tuning/output/` for each experiment.
Rerun the same command to resume. `prepare` validates an existing dataset; `fit`
reuses completed matching checkpoints and restarts interrupted work.

`run.json` records the schema, policy, inputs, tools, and dependencies. Changes to
these or the evaluation require fresh output. Tool identity includes hashes of
every tuning module and package initializer.

Dataset and fit formats must be version 5; older outputs need fresh preparation
and fitting. The engine feature schema is version 2.

## Dataset requirements

Preparation requires at least 40,000 valid games and 20,000 retained opening groups.
It samples at most six fixed quantiles per game from ply 8, settles captures and
check evasions, and drops terminal positions. It deduplicates settled four-field
FENs, keeping one copy when results agree and none when they conflict. Exported
features must reproduce the engine's evaluation exactly.

Games with the same starting FEN share a group across archives. Games without a
starting FEN get their own group. Each group has total weight one. The dataset
header records baseline weights, and the manifest records input provenance and
filtering counts.

## Numerical policy

[`FIT_POLICY`](fit.py) defines the policy. Do not change it to rescue a failed fit.
Five deterministic folds keep opening groups separate. Each round calibrates the
Texel scale and fits middlegame and endgame weights on four folds, then validates
on the fifth. Variables need support from at least 128 groups in every training
split.

Pawn middlegame value and PSQT/mobility anchors stay fixed. Non-pawn PSQTs retain
file symmetry. Changes are limited to 100 cp and the material-specific ranges.
The fit excludes nonlinear king danger, phase/scaling formulas, tempo, and search
parameters.

L-BFGS-B fits regularized weights, counting all tied coefficients in the penalty.
Rounded checkpoints compete with the baseline on exact training loss plus penalty.
Validation uses bootstrap comparisons on held-out opening groups. The overall
improvement's 90% interval must have a positive lower bound. No supported phase
bucket may have a negative upper bound.
The tuner selects the strongest eligible penalty within one standard error of the
best mean fold improvement, then refits all data. If none qualify, it keeps the
baseline.

Run the [tool tests](../README.md#tests) after changing the tuner.

Background: [Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
and [Ethereal's tuning paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf).
