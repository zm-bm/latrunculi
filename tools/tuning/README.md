# Latrunculi tuning datasets

Requires Python 3 with the dependencies in `tools/tuning/requirements.txt`.

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
