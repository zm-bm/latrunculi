# Latrunculi tuning

These offline tools run joint Texel tuning for the handcrafted evaluation. See
the [workflow](workflow.md) for the method and decision rules.

Install the pinned dependencies with Python 3.12 or newer:

```bash
python3 -m pip install -r tools/tuning/requirements.txt
```

Copy `experiment.example.json`, fill in the corpus details, and run or resume:

```bash
WORK=tools/tuning/output/experiment-name

python3 tools/tuning/tune.py run \
  --experiment tools/tuning/experiment.json \
  --engine build/release/latrunculi \
  --output "$WORK" \
  /path/to/games.pgn.tar

python3 tools/tuning/tune.py status "$WORK"
python3 tools/tuning/tune.py status --json "$WORK"
python3 tools/tuning/tune.py validate "$WORK"
```

Review `candidate.json`. If cross-validation supports it, apply the weights,
rebuild, and verify the compiled engine before its OpenBench strength test:

```bash
python3 tools/tuning/tune.py verify "$WORK" \
  --engine build/candidate/latrunculi
```

Record the result as `upper`, `lower`, or `inconclusive` with a test ID:

```bash
python3 tools/tuning/tune.py close "$WORK" \
  --result upper \
  --reason "SPRT reached the upper boundary" \
  --openbench-test TEST_ID
```

Use `--result offline` without a test ID when no candidate passes offline
review. `close` appends the decision to tracked `results.jsonl`.

Inputs may be `.pgn`, `.pgn.bz2`, or OpenBench `.pgn.tar`. Generated state
belongs under ignored `tools/tuning/output/`. The tools do not edit engine
source, submit workloads, or depend on OpenBench internals.
