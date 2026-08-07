# Benchmarks

`bench/` contains local measurement, regression, and paired engine-match
tooling. It complements the unit tests; it is not a tuning framework or
permanent result store.

## Architecture

The C++ `benchmark` executable performs engine operations directly. It owns the
perft cases and deterministic evaluation-corpus execution. `bench.py` and
`benchlib/` build binaries, drive UCI searches, manage evaluation snapshots,
run Cute Chess matches, capture artifacts, render summaries, and compare
compatible benchmark runs.

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

Measure isolated evaluation throughput over the checked-in corpus:

```bash
python3 bench/bench.py run eval --label baseline
```

The defaults use 50,000 warmup corpus repetitions followed by seven samples of
100,000 repetitions. Override them with `--warmup`, `--repetitions`, and
`--samples`; pass `--benchmark /path/to/benchmark` to use an existing binary.
The direct `benchmark eval throughput` form writes the sample TSV to stdout.

Run a paired smoke match against an archived baseline:

```bash
mkdir -p scratch/baselines
cp build/release-dev/bin/latrunculi \
  scratch/baselines/latrunculi-<revision>

python3 bench/bench.py run match \
  --label smoke --profile smoke \
  --baseline scratch/baselines/latrunculi-<revision> \
  --baseline-revision <revision>
```

Archive a clean baseline before making the candidate change. Add
`--baseline-dirty` only when the archived binary came from a dirty tree. The
smoke profile plays one depth-one opening pair and validates orchestration; it
is not strength evidence. A standard strength run uses 1,000 pairs by default:

```bash
python3 bench/bench.py run match \
  --label eval-change --profile standard \
  --baseline scratch/baselines/latrunculi-<revision> \
  --baseline-revision <revision> \
  --concurrency 4
```

Standard matches use `10+0.1`, one thread, 32 MB Hash, fixed adjudication, and
sequential color-swapped openings. `--pairs` and `--concurrency` are the only
setting overrides. Match runs compare both engines directly and therefore do
not use `bench.py compare`.

Matches require `cutechess-cli` and the local, ignored opening file
`data/book-ply4-unifen-Q-0.0-0.25.pgn`. Its required SHA-256 is
`a9c223edf1592cddca3ac20c62374b1f8b1d18a2ae6270de9042155bd3764d17`.
The file is not checked in because its redistribution provenance is not
documented.

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

Snapshots are deterministic behavior evidence, not performance measurements.
The timed evaluation suite measures the normal `eval::evaluate()` hot path over
preloaded Boards and reports median nanoseconds/evaluation and
evaluations/second with min–max ranges. It intentionally reflects a small,
hot-cache working set. Use fixed-depth search separately to observe downstream
nodes, scores, PVs, best moves, and NPS. Only engine matches can establish a
playing-strength change.

## Run artifacts

Runs are written beneath the ignored `scratch/bench-runs/` directory:

- `manifest.json` records the schema, suite settings, binary, revision, dirty
  state, and build context needed to judge comparability.
- `results.tsv` is the stable machine-readable result.
- `summary.md` is the human-readable report.
- `raw/` preserves original benchmark or engine output.

Match runs additionally preserve the complete PGN and Cute Chess stdout and
stderr. Their summaries report candidate W/D/L, score, and pentanomial counts
for pairs scoring 0, 0.5, 1, 1.5, or 2 points. A smoke result is never a
strength claim. For a standard match, retain a candidate only when Cute
Chess's reported Elo interval excludes zero in the candidate's favor; an
interval crossing zero is inconclusive. Engine crashes, stalls, illegal moves,
invalid claims, and time forfeits invalidate the match even when Cute Chess can
recover and finish it.

Comparisons require the current manifest/schema version and identical
suite-defining settings. Search runs must agree on position selection, limit,
repeats, threads, and hash size; perft runs must use the same profile; evaluation
runs must use the same corpus, warmup, repetitions, and sample count. Revisions,
binaries, dirty states, compiler/build modes, and build presets are recorded but
may differ because measuring those differences is the purpose of the tool.

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
callback framework, or generic benchmark object model. Evaluation snapshots
and timed throughput share the same corpus loader and executable path.

Parameter optimization, Texel tuning, large datasets, databases, plugins, and
NNUE tooling remain outside this benchmark harness. See the
[evaluation roadmap](../docs/eval-roadmap.md) for their staged treatment.
