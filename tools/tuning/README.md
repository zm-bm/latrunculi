# Latrunculi tuning

These offline tools run joint Texel tuning for the handcrafted evaluation. See
the [workflow](workflow.md) for the method and acceptance rules.

With Python 3.12 or newer, install the pinned dependencies:

```bash
python3 -m pip install -r tools/tuning/requirements.txt
```

Copy the example and fill in the experiment and corpus details:

```bash
cp tools/tuning/experiment.example.json tools/tuning/experiment.json
```

Run or resume the experiment:

```bash
WORK=tools/tuning/output/experiment-name

python3 tools/tuning/tune.py run \
  --experiment tools/tuning/experiment.json \
  --engine build/release/latrunculi \
  --output "$WORK" \
  /path/to/games.pgn.tar

python3 tools/tuning/tune.py status "$WORK"
python3 tools/tuning/tune.py status --json "$WORK"
```

Review `$WORK/candidate.json`. If it qualifies, apply its changes, rebuild, and
verify the resulting engine before starting OpenBench:

```bash
python3 tools/tuning/tune.py verify "$WORK" \
  --engine build/candidate/latrunculi
```

Record the completed decision with `upper`, `lower`, or `inconclusive`:

```bash
python3 tools/tuning/tune.py close "$WORK" \
  --result upper \
  --reason "SPRT reached the upper boundary" \
  --openbench-test TEST_ID
```

Use `--result offline` without a test ID when no candidate qualifies or review
rejects it. Closed results are added to the tracked `results.jsonl` ledger.

Validate the complete dataset or reveal held-out performance after closure:

```bash
python3 tools/tuning/tune.py validate "$WORK"
python3 tools/tuning/tune.py reveal "$WORK"
```

Inputs may be `.pgn`, `.pgn.bz2`, or OpenBench `.pgn.tar` files. Generated
state belongs under the ignored `tools/tuning/output/` directory. The tools do
not edit engine source, start OpenBench workloads, or access OpenBench internals.
