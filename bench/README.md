# Benchmarks

`bench/` contains local measurement and regression tooling. It complements the
unit tests; it is not an engine-match runner, tuning framework, or permanent
result store.

## Architecture

The C++ `benchmark` executable performs engine operations directly. It owns the
perft cases and deterministic evaluation-corpus execution. `bench.py` and
`benchlib/` build binaries, drive UCI searches, manage evaluation snapshots,
capture artifacts, render summaries, and compare compatible runs.

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
set or the checked-in diagnostic evaluation corpus described below and in
[`docs/eval-roadmap.md`](../docs/eval-roadmap.md).

## Evaluation snapshots

The small corpus in `eval/corpus.tsv` records diagnostic positions for exact
evaluation and trace regression. It is not a tactical suite, training set, or
held-out tuning set. Its order and stable IDs define the order of the generated
snapshot.

Use the separate deterministic command family to validate the corpus, inspect
the current integer-valued snapshot, or compare it with the checked-in
`eval/baseline.tsv`:

```bash
python3 bench/bench.py eval emit
python3 bench/bench.py eval verify
```

After an approved evaluation or corpus change, update the baseline explicitly:

```bash
python3 bench/bench.py eval regenerate
```

Verification never rewrites the baseline, and these commands do not create a
run directory under `scratch/`. Pass `--benchmark /path/to/benchmark` after the
action to use an existing binary without building it.

The corpus TSV has the columns `corpus_version`, `id`, `category`, and canonical
six-field `fen`. It includes side-to-move, file-mirrored, and rotated
color-swapped positions as ordinary diagnostic coverage. File-mirrored scores
may differ because the current piece-square tables are file-asymmetric;
evaluation symmetry is enforced by focused evaluator tests rather than corpus
metadata.

The baseline uses long-form `summary` and `term` records so numeric changes
remain reviewable. Corpus content or ordering changes increment
`corpus_version`; snapshot columns or semantics increment the
`evaluation_snapshot_vN` result format. Intentional parameter changes update
only the reviewed numeric baseline.

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

The executable uses a small explicit dispatcher with separate perft and
evaluation implementations. It should not grow a registry, plugin system,
callback framework, or generic benchmark object model. INFRA-002 should add
timing and checksums to the existing evaluation path rather than introducing a
second orchestration system.

Engine matches, parameter optimization, Texel tuning, large datasets,
databases, plugins, and NNUE tooling remain outside this benchmark harness. See
the [evaluation roadmap](../docs/eval-roadmap.md) for their staged treatment.
