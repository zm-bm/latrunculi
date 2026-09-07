# Component Measurements

`latrunculi-measure` measures three engine components:

| Command | Component | Primary signals |
|---|---|---|
| `perft` | Move generation and make/unmake | exact nodes, time, nodes/second |
| `eval` | Handcrafted evaluation | checksum, nanoseconds/evaluation, evaluations/second |
| `search` | Integrated search | static/search scores, depth, nodes, time, best move, PV |

Tests cover correctness. Component measurements cover deterministic work and
local performance; playing strength requires paired engine games.

## Build

The `release-dev` preset enables the optional tool with release optimizations:

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

Search runs an embedded fourteen-position suite containing six controls,
five positions from the 1.0 release pilots, and three objective guards. The TT
and search heuristics are cleared before every position so each row starts cold.

```bash
./build/release-dev/latrunculi-measure search
./build/release-dev/latrunculi-measure search \
  --case pilot14-g171-abrupt --nodes 524288 --hash 32 \
  --threads 1 --repetitions 1 --format tsv
```

Defaults are depth 5, one thread, one repetition, and 32 MB Hash. Prefer one
thread for deterministic algorithm comparisons; use larger counts for coarse
scaling measurements. Choose at most one of `--depth`, `--nodes`, or
`--movetime`; `--case` selects a single embedded position. Run one case and one
repetition per process when fresh-process isolation matters.

Case IDs are `startpos`, `arasan20-01`, `arasan20-08`, `arasan20-16`,
`arasan20-21`, `arasan20-30`, `pilot14-g171-abrupt`,
`pilot18-g154-abrupt`, `pilot14-g061-gradual`, `pilot18-g093-gradual`,
`pilot15-g078-secondary`, `objective-mate-1`, `objective-mate-2`, and
`objective-rook-capture`.

TSV output uses `search_measurement_v3` and reports the requested limit, static
and searched scores, completed depth, actual nodes, timing, best move, and PV.
Scores are centipawns from the root side-to-move perspective. Search-statistics
builds write the existing instrumentation report to stderr, labeled with the
case and run configuration, so stdout remains machine-readable:

```bash
./build/release-stats/latrunculi-measure search \
  --case arasan20-01 --nodes 524288 --format tsv \
  > tools/measurements/output/arasan20-01.tsv \
  2> tools/measurements/output/arasan20-01.stats
```

## Comparing Results

Run identical commands against baseline and candidate builds made with the
same compiler and options, on the same machine under quiet, controlled
conditions. Keep retained output under the ignored `tools/measurements/output/`
directory:

```bash
mkdir -p tools/measurements/output
./build/baseline/latrunculi-measure search --format tsv \
  > tools/measurements/output/baseline.tsv
./build/candidate/latrunculi-measure search --format tsv \
  > tools/measurements/output/candidate.tsv
```

Exact nodes, scores, moves, PVs, and evaluation checksums reveal behavioral
changes. Timing and throughput require repeated runs and should never become
unit-test thresholds.

The machine-readable formats are `perft_measurement_v1`,
`evaluation_throughput_v1`, and `search_measurement_v3`. Increment the relevant
format or workload version whenever its columns, semantics, or embedded
workload change.
