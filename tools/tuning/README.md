# Latrunculi tuning

Requires Python 3.12 or newer with the dependencies in
`tools/tuning/requirements.txt`.

```bash
python3 -m pip install -r tools/tuning/requirements.txt
```

Build a deterministic WDL-labeled feature dataset:

```bash
python3 tools/tuning/dataset.py build \
  --config tools/tuning/dataset.json \
  --engine build/release/latrunculi \
  --output data/tuning/latrunculi-hce-v2 \
  /path/to/OpenBench/Media/PGNs/8.pgn.tar
```

The offline builder accepts `.pgn`, `.pgn.bz2`, and OpenBench `.pgn.tar`
exports from any instance. It does not depend on OpenBench internals.

The builder samples quiet, nonterminal positions; deduplicates them; keeps each
paired game group in one split; and writes training, validation, held-out, and
endgame-validation JSONL files. Generated data under `data/` is ignored.

Validate an existing dataset with:

```bash
python3 tools/tuning/dataset.py validate data/tuning/latrunculi-hce-v2
```

Calibrate the Texel scale once at the start of a tuning series. The current
series carries the scale stored in `math-002b-material.json`; changing corpora
does not recalibrate it.

Fit the current stage from its parent artifact:

```bash
TUNING_STAGE=math-002c-pawns
TUNING_PARENT=data/tuning/runs/math-002b-material.json
TUNING_OUTPUT="data/tuning/runs/v2/$TUNING_STAGE.json"

python3 tools/tuning/tune.py fit \
  --config tools/tuning/tune.json \
  --stage "tools/tuning/stages/$TUNING_STAGE.json" \
  --dataset data/tuning/latrunculi-hce-v2 \
  --parent "$TUNING_PARENT" \
  --output "$TUNING_OUTPUT"
```

The stage chain is:

| Stage | Parent | Dataset | Role |
| --- | --- | --- | --- |
| `math-002b-material` | `math-002a` | v1 | candidate |
| `math-002c-pawn-psqt` | `math-002b-material` | v2 | diagnostic |
| `math-002c-pawn-structure` | `math-002b-material` | v2 | diagnostic |
| `math-002c-pawns` | `math-002b-material` | v2 | candidate |

Diagnostic stages test individual families. Candidate stages feed later
accepted fits. Dataset v1 comes from test #4; v2 comes from test #8. A parent
may cross that boundary only when every exported baseline weight matches its
candidate. Its calibrated scale is carried unchanged.

The stage records its bounds, regularization, and acceptance guard. Fitting
never rewrites engine source.

Test #4 supplied the scale and material fit. Test #8 is the fitting corpus for
MATH-002C onward. The tuner uses White-relative evaluation and per-position
mean squared error. Staged fits select checkpoints by validation loss and
report the fixed endgame slice separately as a regression guard. Calibration
and staged fitting never load held-out data. Features seen in fewer than 32
training positions are frozen; mirror ties use their combined support.
Artifacts record the full provenance, result, and weights without rewriting
engine source.
