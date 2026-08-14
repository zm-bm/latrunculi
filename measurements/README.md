# Component Measurements

`latrunculi-measure` measures three engine components:

| Command | Component | Primary signals |
|---|---|---|
| `perft` | Move generation and make/unmake | exact nodes, time, nodes/second |
| `eval` | Handcrafted evaluation | checksum, nanoseconds/evaluation, evaluations/second |
| `search` | Integrated search | score, nodes, time, nodes/second, best move, PV |

Tests cover correctness. Component measurements cover deterministic work and
local performance; playing strength requires paired engine games.

## Build

Developer presets enable the optional tool. Use a release build for timing:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --target latrunculi-measure
```

## Perft

Perft measures production move generation and Board make/unmake behavior. Every
case validates its exact node count and verifies that the Board is restored.

```bash
./build/release-dev/latrunculi-measure perft --profile smoke
./build/release-dev/latrunculi-measure perft --profile standard --format tsv
```

The `smoke` profile is quick; `standard` runs the complete embedded suite.

## Evaluation

Evaluation measures `eval::evaluate()` over a fixed, ordered 24-position
workload. All Boards are constructed before timing.

```bash
./build/release-dev/latrunculi-measure eval
./build/release-dev/latrunculi-measure eval \
  --warmup 1000 --repetitions 10000 --samples 5
```

Defaults are 50,000 warmup repetitions followed by seven samples of 100,000
repetitions. Output is TSV. The checksum is an order-sensitive behavior
fingerprint; the throughput fields measure speed.

## Search

Search runs a fixed six-position suite at a fixed depth. The TT and search
heuristics are cleared before every position so each row starts cold.

```bash
./build/release-dev/latrunculi-measure search
./build/release-dev/latrunculi-measure search \
  --depth 6 --threads 1 --repetitions 3 --format tsv
```

Defaults are depth 5, one thread, one repetition, and 32 MB Hash. Prefer one
thread for deterministic algorithm comparisons; use larger counts for coarse
scaling measurements.

## Comparing Results

Run identical commands against baseline and candidate builds under quiet,
controlled conditions. Keep retained output under the ignored
`measurements/output/` directory:

```bash
mkdir -p measurements/output
./build/baseline/latrunculi-measure search --format tsv \
  > measurements/output/baseline.tsv
./build/candidate/latrunculi-measure search --format tsv \
  > measurements/output/candidate.tsv
```

Exact nodes, scores, moves, PVs, and evaluation checksums reveal behavioral
changes. Timing and throughput require repeated runs and should never become
unit-test thresholds.

The machine-readable formats are `perft_measurement_v1`,
`evaluation_throughput_v1`, and `search_measurement_v1`. Increment the relevant
format or workload version whenever its columns, semantics, or embedded
workload change.
