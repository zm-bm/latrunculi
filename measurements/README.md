# Component measurements

`measurements/` contains a small C++ tool for measuring the three main engine
components:

| Command | Component | Primary signals |
|---|---|---|
| `perft` | Move generation and make/unmake | exact nodes, time, nodes/second |
| `eval` | Handcrafted evaluation | checksum, nanoseconds/evaluation, evaluations/second |
| `search` | Integrated search | score, nodes, time, nodes/second, best move, PV |

These are local performance measurements, not correctness or playing-strength
tests. Correctness belongs in `tests/`; strength changes require paired engine
games through the tuning workflow.

## Build

Developer presets enable the optional tool:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --target latrunculi-measure
```

Use a release build for timing. The executable is written to
`build/release-dev/bin/latrunculi-measure`.

## Perft

Perft measures production move generation and Board make/unmake behavior. Every
case validates its exact node count and verifies that the Board is restored.

```bash
./build/release-dev/bin/latrunculi-measure perft --profile smoke
./build/release-dev/bin/latrunculi-measure perft --profile standard --format tsv
```

The smoke profile is quick; standard adds the complete embedded perft suite.

## Evaluation

Evaluation measures `eval::evaluate()` over a fixed, ordered 24-position
workload. All Boards are constructed before timing.

```bash
./build/release-dev/bin/latrunculi-measure eval
./build/release-dev/bin/latrunculi-measure eval \
  --warmup 1000 --repetitions 10000 --samples 5
```

The defaults are 50,000 warmup repetitions followed by seven samples of
100,000 repetitions. Output is TSV. The checksum is an order-sensitive compact
behavior fingerprint; it is not a speed or strength metric.

## Search

Search runs a fixed six-position suite at a fixed depth. The TT and search
heuristics are cleared before every position so each row starts cold.

```bash
./build/release-dev/bin/latrunculi-measure search
./build/release-dev/bin/latrunculi-measure search \
  --depth 6 --threads 1 --repetitions 3 --format tsv
```

Defaults are depth 5, one thread, one repetition, and 32 MB Hash. One thread is
preferred for deterministic algorithm comparisons; larger thread counts are
available for coarse scaling measurements.

## Comparing changes

Run the same command against baseline and candidate builds under otherwise
quiet, controlled conditions. Keep retained output beneath the ignored
`measurements/output/` directory:

```bash
mkdir -p measurements/output
./build/baseline/bin/latrunculi-measure search --format tsv \
  > measurements/output/baseline.tsv
./build/candidate/bin/latrunculi-measure search --format tsv \
  > measurements/output/candidate.tsv
```

Exact nodes, scores, moves, PVs, and evaluation checksums reveal behavioral
changes. Timing and throughput require repeated runs and should never become
unit-test thresholds.

The machine-readable formats are `perft_measurement_v1`,
`evaluation_throughput_v1`, and `search_measurement_v1`. Increment the relevant
format or workload version whenever its columns, semantics, or embedded
workload change.

Keep measurement-specific workloads and formatting local to this directory.
The production engine, tuning tools, and match infrastructure should not depend
on `measurements/`.
