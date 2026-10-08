# Latrunculi tuning

The tuning tool fits the linear handcrafted evaluation from recorded games.
It prepares a grouped dataset, fits candidate weights, and verifies that an
engine contains those weights. [OpenBench testing](../../docs/openbench.md#strength-tests)
measures playing strength.

Install the pinned dependencies with Python 3.12 or newer, then run:

```bash
python3 -m pip install -r tools/tuning/requirements.txt

DATA=tools/tuning/output/prepared
FIT=tools/tuning/output/fit

python3 tools/tuning/tune.py prepare \
  --engine build/release/latrunculi --output "$DATA" /path/to/games.pgn.tar
python3 tools/tuning/tune.py fit --dataset "$DATA" --output "$FIT"
python3 tools/tuning/tune.py verify "$FIT" --engine build/candidate/latrunculi
```

Inputs may be `.pgn`, `.pgn.bz2`, or OpenBench `.pgn.tar`. Keep generated files
under ignored `tools/tuning/output/`.

## Prepared data

`prepare` writes `manifest.json` and `development.jsonl`. It groups games by
starting position so related examples stay together during validation. The
[OpenBench tuning workload](../../docs/openbench.md#tuning-corpus) describes
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
