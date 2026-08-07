# Benchmarks

`bench/` contains local measurement and regression tooling. It complements the
unit tests; it is not an engine-match runner, tuning framework, or permanent
result store.

## Architecture

The C++ `benchmark` executable measures engine operations directly. It currently
owns the perft cases and timing loop. `bench.py` and `benchlib/` build binaries,
drive UCI searches, capture artifacts, render summaries, and compare compatible
runs.

Machine-readable C++ output is written to stdout without commentary. Usage and
diagnostics go to stderr. The Python entry point prints the generated artifact
path on success. Its search harness captures the engine's combined stdout and
stderr in raw logs while parsing only recognized UCI result lines.

Developer presets build the benchmark target:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --target benchmark latrunculi
```

## Running benchmarks

Run the quick or complete perft profile:

```bash
python3 bench/bench.py run perft --label baseline --profile smoke
python3 bench/bench.py run perft --label baseline --profile standard
```

Run the default six-position UCI search suite at a fixed depth or movetime:

```bash
python3 bench/bench.py run search \
  --label baseline --depth 5 --threads 1 --repeats 3
python3 bench/bench.py run search \
  --label baseline --movetime 1000 --threads 1 --repeats 3
```

`--positions` accepts `suite` or a comma-separated list of IDs such as
`startpos,arasan20-01,arasan20-16`. Passing `--engine /path/to/latrunculi`
uses that binary without building it.

Compare two compatible run directories:

```bash
python3 bench/bench.py compare \
  scratch/bench-runs/<baseline> scratch/bench-runs/<candidate>
```

`arasan20.epd` supplies the search positions. It is not an evaluation training
set or the diagnostic evaluation corpus planned in
[`docs/eval-roadmap.md`](../docs/eval-roadmap.md).

## Run artifacts

Runs are written beneath the ignored `scratch/bench-runs/` directory:

- `manifest.json` records the schema, suite settings, binary, revision, dirty
  state, and build context needed to judge comparability.
- `results.tsv` is the stable machine-readable result.
- `summary.md` is the human-readable report.
- `raw/` preserves original benchmark or engine output.

Comparisons require the current manifest/schema version and identical
suite-defining settings. Search runs must agree on position selection, limit,
repeats, threads, and hash size; perft runs must use the same profile. Revisions,
binaries, dirty states, and build presets are recorded but may differ because
measuring those differences is the purpose of the tool.

Timing comparisons require controlled conditions and repeated runs. Wall-clock
results are never unit-test thresholds. Fixed-depth search also exposes
deterministic tree-shape signals such as nodes, score, best move, and PV.

## Extending the tooling

Add a suite-specific `benchlib` module, reuse the common manifest and artifact
helpers, define an exact TSV schema and compatibility fields, and add comparison
support only when its metrics have a meaningful interpretation. Keep
suite-specific policy out of `common.py`.

Deterministic snapshots and timed measurements have different lifecycles.
Snapshots should be checked-in integer-valued baselines with separate verify and
explicit regeneration commands. Timed runs remain disposable scratch artifacts.

INFRA-001 in the evaluation roadmap may add the second direct C++ operation. At
that point, `benchmark.cpp` can become a small explicit dispatcher with separate
perft and evaluation implementations. It should not grow a registry, plugin
system, callback framework, or generic benchmark object model. INFRA-002 should
add timing and checksums to that same evaluation path rather than introducing a
second orchestration system.

Engine matches, parameter optimization, Texel tuning, large datasets,
databases, plugins, and NNUE tooling remain outside this benchmark harness. See
the [evaluation roadmap](../docs/eval-roadmap.md) for their staged treatment.
