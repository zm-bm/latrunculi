# Fitting checks and evidence

The [tuning guide](../../../../tools/tuning/README.md) provides commands and output paths. The exact numerical settings are in `tools/tuning/tune.py` (`FIT_POLICY`); use the versioned method rather than ad hoc thresholds or parameter variants.

## Prepare a dataset

Use a recorded corpus. Preparation requires 40,000 valid games and 20,000 retained opening groups. Sample at most six fixed quantiles per game from ply 8; settle captures and check evasions with `features --settle`, and discard terminal positions. Deduplicate settled four-field FENs: keep one copy when results agree, drop all copies when they conflict.

Starting FENs identify opening groups across archives; games without one get their own group. Give each group total weight one. Every retained position must reconstruct the exported evaluation exactly. The dataset header carries schema 2 and baseline coefficients; the manifest records provenance and filtering.

## Fit and select weights

Use five deterministic folds with opening groups kept disjoint. In each fold, calibrate the Texel scale and fit middlegame and endgame weights together on the other four folds using L-BFGS-B and regularization. A variable needs support from at least 128 groups in every training complement. Select the rounded optimizer checkpoint with the lowest exact training loss plus penalty; the unchanged baseline remains eligible.

Keep pawn middlegame value and PSQT/mobility anchors fixed. Non-pawn PSQTs retain file symmetry; coefficient changes are bounded by 100 cp and material values by piece-specific ranges. Regularization counts every underlying coefficient in a tie. Nonlinear king danger, phase/scaling formulas, tempo, and search parameters remain outside the fit.

Evaluate only on excluded folds. Eligibility requires a positive overall 90% lower confidence bound and no negative upper bound in supported phase buckets, using opening-group bootstrap comparisons. Select the strongest eligible penalty within one standard error of the best mean fold improvement, then refit all data with the same constrained variables. If none qualify, retain the baseline.

## Review, verify, and resume

Review `cross-validation.json` and `candidate.json` before applying weights. They record selection evidence, numerical support, integer weights, large changes, and bound hits. Large changes and bound hits invite review; they are not automatic rejection rules. Numerical support does not establish playing strength.

`verify` compares all compiled coefficients and evaluation invariants with the proposed weights. Keep its JSON result with the candidate and baseline SHAs. A fitted candidate still needs the full offline pass and strength evidence under the shared [acceptance rules](../../../references/candidate-rules.md#acceptance).

`run.json` pins the fit's schema, policy, inputs, tools, and dependencies; final reports mark completion. Rerun `prepare` to validate an existing dataset or `fit` to reuse completed matching checkpoints. Interrupted units restart. Changed inputs, evaluation, schema, tools, dependencies, or policy require fresh output. A search-only baseline change may preserve numerical evidence, but needs a new current-baseline candidate and checks; retain the original fit and revision.
