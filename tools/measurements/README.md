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
repetitions. Output is TSV. This is a steady-state microbenchmark of the raw
evaluator, not a model of a complete search. The checksum is an order-sensitive
behavior fingerprint: exact optimizations must preserve it, while intentional
evaluation changes may not. Throughput measures component speed; it does not
establish correctness, evaluation quality, realistic cache behavior, integrated
engine speed, or playing strength.

## Search

Search loads the complete 200-position Arasan 20 tactical corpus from
`search.epd`. The TT and search heuristics are cleared before every position so
each row starts cold. One suite pass is one process; `--repetitions` repeats
inside that process and does not provide fresh-process reproducibility.

```bash
./build/release-dev/latrunculi-measure search
./build/release-dev/latrunculi-measure search \
  --suite tools/measurements/search.epd --depth 10 --hash 32 \
  --threads 1 --repetitions 1 --format tsv
./build/release-dev/latrunculi-measure search \
  --case arasan20-01 --depth 10 --hash 32 \
  --threads 1 --repetitions 1 --format tsv
```

Defaults are depth 5, one thread, one repetition, and 32 MiB Hash. Prefer one
thread for deterministic algorithm comparisons; use larger counts for coarse
scaling measurements. Choose at most one of `--depth`, `--nodes`, or
`--movetime`; `--suite` selects another EPD file and `--case` selects one ID
from the loaded file. Use separate process invocations when fresh-process
isolation matters. Strength experiments use the explicit complete-corpus
options above rather than the tool's depth-5 default. A second command
invocation provides fresh-process reproducibility; `--repetitions 2` does not,
and it does not increase the number of distinct positions.

The corpus preserves every position and source annotation from historical Git
blob `93cbe97d9ee40790eafc984e59cbce3c02a5d7ea`; only its IDs are normalized to
`arasan20-NN`. Its `bm` and `am` operations are metadata, not correctness or
playing-strength criteria. The optional `search-sentinels.epd` contains the
four focused trajectory cases retained from the completed audit;
[Search Knowledge](../../docs/search.md) defines their interpretation:

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
Timing requires repeated runs and must not become a unit-test threshold. Keep
raw output under the ignored `tools/measurements/output/` directory.
[Search Knowledge](../../docs/search.md) owns the default search panel, metric interpretation,
and paired timing policy.
[Playing Strength Development](../../docs/playing-strength.md) owns lifecycle and each active
task's frozen thresholds and deviations.

For corpus timing, aggregate measured search time with `sum(total_ns)`. This
excludes setup before `start_search()`, process startup, and output. See
[Search Knowledge](../../docs/search.md) for collection and decision rules.

The fixed `BC, CB, BC, CB, BC, CB` panel forms three adjacent balanced
`B-C-C-B` blocks. The helper reports each block's geometric mean ratio and
`median_balanced_search_time_ratio`, the decision metric. It retains the
overall and order-separated summaries as diagnostics.

Use `compare_search.py` for deterministic aggregation. It validates the current
200 case IDs, the canonical request profile, comparable case sets, and required
signatures, but it does not decide whether a task passes:

```bash
python3 tools/measurements/compare_search.py nodes \
  baseline.tsv candidate.tsv --repeat candidate-repeat.tsv --details
python3 tools/measurements/compare_search.py timing \
  --pair BC pair-1-baseline.tsv pair-1-candidate.tsv \
  --pair CB pair-2-baseline.tsv pair-2-candidate.tsv \
  --pair BC pair-3-baseline.tsv pair-3-candidate.tsv \
  --pair CB pair-4-baseline.tsv pair-4-candidate.tsv \
  --pair BC pair-5-baseline.tsv pair-5-candidate.tsv \
  --pair CB pair-6-baseline.tsv pair-6-candidate.tsv
```

Add `--exact-tree` only when baseline and candidate search signatures must
match. Without it, the helper requires each candidate timing pass to match the
other candidate passes while allowing baseline-to-candidate static-score,
searched-score, best-move, PV, and node differences. The flag validates sampled
signature equality; it does not prove semantic equivalence, classify a
candidate, or authorize skipping games. The timing command checks the declared
panel shape and guards against reusing a path accidentally; the artifact
manifest remains the record of suite and input provenance, binary identity,
affinity, and actual execution order.

Record compiler provenance from the configured build, not from the shell's
default compiler. Capture `CMAKE_CXX_COMPILER`, `CMAKE_CXX_COMPILER_ID`, and
`CMAKE_CXX_COMPILER_VERSION` from the build's generated
`CMakeCXXCompiler.cmake`; retain the configured compiler path, ID, and version
in the artifact manifest.

Run the helper's focused tests with:

```bash
python3 -m unittest tools.measurements.test_compare_search
```

The machine-readable formats are `perft_measurement_v1`,
`evaluation_throughput_v1`, and `search_measurement_v3`. These labels version
output schemas, not external workloads. Record the suite revision and SHA-256
with retained artifacts.
