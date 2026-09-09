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

Search loads the complete 200-position Arasan 20 tactical corpus from
`search.epd`. The TT and search heuristics are cleared before every position so
each row starts cold. One suite pass is one process; `--repetitions` repeats
inside that process and does not provide fresh-process reproducibility.

```bash
./build/release-dev/latrunculi-measure search
./build/release-dev/latrunculi-measure search \
  --case arasan20-01 --depth 10 --hash 32 \
  --threads 1 --repetitions 1 --format tsv
```

Defaults are depth 5, one thread, one repetition, and 32 MiB Hash. Prefer one
thread for deterministic algorithm comparisons; use larger counts for coarse
scaling measurements. Choose at most one of `--depth`, `--nodes`, or
`--movetime`; `--suite` selects another EPD file and `--case` selects one ID
from the loaded file. Use separate process invocations when fresh-process
isolation matters.

The corpus preserves every position and source annotation from historical Git
blob `93cbe97d9ee40790eafc984e59cbce3c02a5d7ea`; only its IDs are normalized to
`arasan20-NN`. Its `bm` and `am` operations are metadata, not correctness or
playing-strength criteria. The optional `search-sentinels.epd` contains the
four focused convergence cases retained from the completed audit:

```bash
./build/release-dev/latrunculi-measure search \
  --suite tools/measurements/search-sentinels.epd \
  --case pilot14-g171-abrupt --depth 18 --format tsv
```

TSV output uses `search_measurement_v3` and reports the requested limit, static
and searched scores, completed depth, actual nodes, timing, best move, and PV.
Scores are centipawns from the root side-to-move perspective. Search-statistics
builds write the existing instrumentation report to stderr, labeled with the
case and run configuration, so stdout remains machine-readable:

```bash
./build/release-stats/latrunculi-measure search \
  --case arasan20-01 --depth 10 --format tsv \
  > tools/measurements/output/arasan20-01.tsv \
  2> tools/measurements/output/arasan20-01.stats
```

## Comparing Results

Compare equivalent builds with identical explicit options on the same machine.
Deterministic fields reveal behavior changes; timing requires repeated runs and
must not become a unit-test threshold. Keep raw output under the ignored
`tools/measurements/output/` directory. [Search Development](../../docs/search.md)
owns experiment panels, gates, and aggregation.

The machine-readable formats are `perft_measurement_v1`,
`evaluation_throughput_v1`, and `search_measurement_v3`. These labels version
output schemas, not external workloads. Record the suite revision and SHA-256
with retained artifacts.
