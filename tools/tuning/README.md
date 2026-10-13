# Latrunculi tuning

The tuning tool fits the linear handcrafted evaluation from recorded games.
It prepares a grouped dataset, fits candidate weights, and verifies that an
engine contains those weights. [OpenBench testing](../openbench.md#strength-tests)
measures playing strength.

Set up the [shared Python environment](../README.md#setup), then run from the
repository root:

```bash
DATA=tools/tuning/output/prepared
FIT=tools/tuning/output/fit

tools/.venv/bin/python tools/tuning/tune.py prepare \
  --engine build/release/latrunculi --output "$DATA" /path/to/games.pgn.tar
tools/.venv/bin/python tools/tuning/tune.py fit --dataset "$DATA" --output "$FIT"
tools/.venv/bin/python tools/tuning/tune.py verify "$FIT" --engine build/candidate/latrunculi
```

Inputs may be `.pgn`, `.pgn.bz2`, or OpenBench `.pgn.tar`. Keep generated files
under ignored `tools/tuning/output/`.

For an existing OpenBench corpus whose PGNs are not local, use
`tools/.venv/bin/python -m openbench_client` with the
[status and download commands](../openbench.md#inspect-and-download)
to retain its PGN archive, checksum, and workload provenance together. Verify
the workload revisions against the intended baseline. Pass the downloaded
`.pgn.tar` directly to `prepare`; extraction is unnecessary. Preparation
validates the games; workload completion and pass/fail flags do not establish
corpus usability.

## Prepared data

`prepare` writes `manifest.json` and `development.jsonl`. It groups games by
starting position so related examples stay together during validation. The
[OpenBench tuning workload](../openbench.md#tuning-corpus) describes
how to collect fresh games. Once prepared, `fit` needs the dataset without
the original PGNs or exporter.

## Fitting and verification

`fit` compares candidate weights on held-out opening groups, then writes
`cross-validation.json` and `candidate.json` with the selection evidence and
exact integer weights. Its numerical policy is in
[`FIT_POLICY`](tune.py). Numerical support alone does not establish playing
strength; the [tuning skill](../../.agents/skills/tune-latrunculi-evaluation/SKILL.md)
defines candidate requirements and the next action.

`verify` checks the compiled coefficients and evaluation invariants against
the proposed weights and prints JSON. It can run again after rebuilding
without the dataset. `run.json` records the inputs and policy used for a fit;
matching completed checkpoints can be reused after an interruption.

Background: [Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
and [Ethereal's tuning paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf).
