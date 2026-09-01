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
  --output data/tuning/latrunculi-hce-v1 \
  /path/to/games.pgn.tar
```

The offline builder accepts `.pgn`, `.pgn.bz2`, and OpenBench `.pgn.tar`
exports from any instance. It does not depend on OpenBench internals.

The builder samples quiet, nonterminal positions; deduplicates them; keeps each
paired game group in one split; and writes training, validation, held-out, and
endgame-validation JSONL files. Generated data under `data/` is ignored.

Validate an existing dataset with:

```bash
python3 tools/tuning/dataset.py validate data/tuning/latrunculi-hce-v1
```

Calibrate the fixed Texel scale and write the baseline candidate:

```bash
python3 tools/tuning/tune.py calibrate \
  --config tools/tuning/tune.json \
  --dataset data/tuning/latrunculi-hce-v1 \
  --engine build/release-dev/latrunculi \
  --output data/tuning/runs/math-002a.json
```

Fit any stage from its parent artifact:

```bash
TUNING_STAGE=math-002c-pawns
TUNING_PARENT=math-002b-material

python3 tools/tuning/tune.py fit \
  --config tools/tuning/tune.json \
  --stage "tools/tuning/stages/$TUNING_STAGE.json" \
  --dataset data/tuning/latrunculi-hce-v1 \
  --parent "data/tuning/runs/$TUNING_PARENT.json" \
  --output "data/tuning/runs/$TUNING_STAGE.json"
```

The stage chain is:

| Stage | Parent | Role |
| --- | --- | --- |
| `math-002b-material` | `math-002a` | candidate |
| `math-002c-pawn-psqt` | `math-002b-material` | diagnostic |
| `math-002c-pawn-structure` | `math-002b-material` | diagnostic |
| `math-002c-pawns` | `math-002b-material` | candidate |

Diagnostic stages test individual families. Candidate stages feed later
accepted fits.

The stage records its bounds, regularization, and acceptance guard. Fitting
never rewrites engine source.

Test #4 is the sole fitting corpus; later archives are external checks. The
tuner fits its scale on training data once, then carries it through the
candidate chain. It uses White-relative evaluation and per-position mean
squared error. Staged fits select checkpoints by validation loss and report the
fixed endgame slice separately as a regression guard. Calibration and staged
fitting never load held-out data. Features seen in fewer than 32 training
positions are frozen; mirror ties use their combined support. Artifacts record
the full provenance, result, and weights without rewriting engine source.
