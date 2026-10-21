# Development scripts

- [`./bin/baseline`](#baseline): build, test, and retain a named baseline.
- [`./bin/benchmark`](#benchmark): run a position suite or compare two builds.
- [`./bin/tune`](#tune): fit evaluation weights from games and check them in the compiled engine.
- [`./bin/openbench`](#openbench): prepare settings, submit tests, and inspect OpenBench workloads.
- [`./bin/uci-probe`](#uci-probe): search one position and capture engine output.
- [`./bin/profile`](#profile): record CPU profiles and hardware counters.

Examples assume the repository root. See the [development manual](engine-development.md)
for the development workflow.

## Setup

The Python tools require Python 3.12+.

```bash
python3 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

## `baseline`

`./bin/baseline NAME` builds the current clean checkout with `release-dev`, runs
CTest and `latrunculi bench`, and saves the build in a new `build/NAME/` directory
for later comparisons:

```bash
./bin/baseline baseline
```

The directory retains the engine and benchmark binaries, build settings, and
`bench.txt`; `revision.txt` is written on success. Failed runs leave partial
builds for inspection; remove them or choose a new name before retrying.
Keep saved baselines unchanged when switching revisions.

## `benchmark`

`./bin/benchmark` measures search time, node counts, and chosen moves over a
position suite.

### Usage

To measure the `release-dev` build:

```bash
./bin/benchmark run release-dev --name release-dev-run
```

To compare `release-dev` against a [saved baseline](#baseline):

```bash
./bin/benchmark compare baseline --name release-dev-vs-baseline
```

### Commands

- `run BUILD`: search the position suite with one build, checking search settings
  and the legality of each principal variation (PV).
- `compare BASELINE [CANDIDATE]`: measure a candidate against a baseline.
  The candidate defaults to `release-dev`. Checks each build's repeatability
  and PV legality; a separate `run` is unnecessary.

### Options

Both commands require exactly one of `--name` or `--output`. The destination
directory must not already exist.

| Option | Command | Meaning |
| --- | --- | --- |
| `--name NAME` | `run`, `compare` | Save under `tools/output/NAME/`. |
| `--output PATH` | `run`, `compare` | Explicit output directory instead of `--name`. |
| `--suite FILE` | `run`, `compare` | EPD position suite; default: [200-position suite](../tools/resources/search.epd). |
| `--depth N` | `run`, `compare` | Search depth; default 10. |
| `--threads N` | `run`, `compare` | Search threads; default 1. |
| `--hash MB` | `run`, `compare` | Hash-table size in MiB; default 32. |
| `--cpu N` | `run`, `compare` | Pin runs to one logical CPU using Linux `taskset`. |
| `--repeats N` | `run` | Passes; default 1. Repeated search results must match. |
| `--reference FILE.tsv` | `run` | Require matching results against a saved run, for example after rebuilding an accepted candidate. |
| `--pairs N` | `compare` | Measured runs per build, excluding warmups; default 6. |
| `--exact-tree` | `compare` | Require matching search results between builds, apart from timing. |

Build names select existing directories under `build/`; `--name` selects an output
directory within this checkout. Relative file paths use the current working directory.

For a quick comparison, use `--depth 8 --pairs 2`.
For multithreaded legality checks, use `run` with one pass and no reference.

### Results

```text
Candidate search time: +2.87% (median balanced ratio)
Candidate total nodes: -1.87%
Changed best moves: 47 / 200
Repeatability and PV legality checks: passed
```

`compare` reports changes relative to the baseline and saves the detailed results
to `summary.txt`. The output directory also holds TSV measurements: `corpus-N.tsv`
for `run`, or `pair-N-baseline.tsv` and `pair-N-candidate.tsv` for `compare`.

Ratios divide candidate measurements by baseline measurements.

| Ratio | Meaning |
| --- | --- |
| `geometric_mean_node_ratio` | Geometric mean of the per-position node ratios, giving each position equal weight. |
| `total_node_ratio` | Compares total nodes over the whole suite. |
| `median_balanced_search_time_ratio` | Median search-time ratio, combining opposite run orders to reduce ordering effects. |

Each position starts with cleared search state and no game history. Reported
times cover search only, excluding allocation, setup, clearing, and output.

For timing comparisons, use ordinary Release builds with the same compiler and
build settings; `release-stats` adds diagnostics. Avoid competing CPU activity,
tracing, and profiling. With `--cpu N`, keep other logical CPUs on the same
physical core idle.
Check the per-pair timings for variation between runs. The CPU load recorded in
`timing-load.jsonl` can help identify competing activity that affected the measurements.

## `tune`

`./bin/tune` fits linear evaluation weights from PGNs and checks them against a
compiled engine. See [tuning guidelines](engine-development.md#tune-evaluation).

### Usage

Prepare games with `release-dev`, then fit weights:

```bash
./bin/tune prepare --engine release-dev --name pawns-data games.pgn.tar
./bin/tune fit --dataset pawns-data --name pawns-fit
```

After reviewing a supported fit, copy its exact weights into
[`src/eval/parameters.hpp`](../src/eval/parameters.hpp), then rebuild and verify:

```bash
cmake --build --preset release-dev
./bin/tune verify pawns-fit --engine release-dev
```

### Commands

- `prepare`: create a dataset from an engine built with the starting weights and
  one or more `.pgn`, `.pgn.bz2`, or `.pgn.tar` files.
  Requires at least 40,000 valid games and 20,000 retained opening groups.
  The engine and PGNs must remain unchanged during preparation.
- `fit`: fit weights from a prepared dataset; the original engine and PGNs are
  no longer needed.
- `verify FIT`: check that the compiled weights and evaluation invariants match
  a supported fit. Requires only the fit directory and engine binary.

### Options

`prepare` and `fit` require exactly one of `--name` or `--output`.

| Option | Command | Meaning |
| --- | --- | --- |
| `--engine BUILD` | `prepare`, `verify` | Engine: `build/BUILD/latrunculi`. |
| `--dataset DATA` | `fit` | Prepared dataset: `tools/tuning/output/DATA/`. |
| `--name NAME` | `prepare`, `fit` | Save under `tools/tuning/output/NAME/`. |
| `--output PATH` | `prepare`, `fit` | Explicit output directory instead of `--name`. |

`FIT` names also select `tools/tuning/output/FIT/`. Names resolve within this
checkout. Engine, dataset, and fit arguments containing `/` are explicit paths
(for example `./prepared`).

### Results

- Prepared data: `development.jsonl`, with input and filtering records in `manifest.json`.
- Fit results: `cross-validation.json` for validation and `candidate.json` for
  integer weights and review flags.

If `cross_validation_supported` is false in `candidate.json`, the fit retains
the baseline weights.

Rerun the same command to reuse matching outputs: `prepare` validates the existing
dataset, while `fit` reuses completed checkpoints and reruns interrupted work.

Changed data, policy, tools, or dependencies require a fresh fit directory.
Tool changes affecting only fitting or the CLI leave matching prepared datasets
reusable and completed fits verifiable.
Unsupported output formats require fresh preparation or fitting as applicable.

## `openbench`

`./bin/openbench` prepares and submits engine tests, inspects workloads, and
downloads games. See
[OpenBench guidelines](engine-development.md#test-strength) for test selection
and interpretation.

### Usage

Server commands read `~/.config/openbench/openbench.env`, containing assignments
for `OPENBENCH_SERVER` (the HTTPS URL), `OPENBENCH_USERNAME`, and `OPENBENCH_PASSWORD`.
Preparation needs no credentials or server connection.

Supply binaries matching the tested revisions, and push those revisions before
submission. OpenBench builds the engines on its workers.

Prepare a strength comparison, then submit the saved settings:

```bash
./bin/openbench prepare sprt \
  --candidate HEAD --candidate-engine build/release-dev/latrunculi \
  --baseline "$(cat build/baseline/revision.txt)" \
  --baseline-engine build/baseline/latrunculi \
  --description "STC: Describe the change and expected benefit" \
  --output tools/output/my-change-openbench

./bin/openbench submit tools/output/my-change-openbench/settings.json \
  --output tools/output/my-change-openbench/submission
```

Inspect a workload and download its completed PGNs:

```bash
./bin/openbench status 46
./bin/openbench download-pgns 46 --output tools/output/workload-46
```

### Commands

`prepare` resolves Git references to full SHAs and runs each supplied engine's
`bench` once to collect node counts. These check build consistency, not speed or
source identity; OpenBench uses measured NPS to scale time controls.

`prepare sprt` creates strength-comparison settings with `[0,3]` bounds at `10+0.1`.

`prepare games` creates fixed-game comparison or self-play settings at `10+0.1`:

```bash
./bin/openbench prepare games \
  --candidate HEAD --candidate-engine build/release-dev/latrunculi \
  --self-play --games 2000 \
  --description "Release stability self-play" \
  --output tools/output/release-stability
```

`prepare corpus` creates self-play settings for tuning, defaulting to 42,000 games
at `4+0.04`:

```bash
./bin/openbench prepare corpus \
  --candidate HEAD --candidate-engine build/release-dev/latrunculi \
  --worker-concurrency 12 \
  --description "Self-play corpus for evaluation tuning" \
  --output tools/output/my-corpus-workload
```

Corpus collection requires one worker with one match runner; see
[corpus guidelines](engine-development.md#self-play-corpus).

Server commands:

- `submit SETTINGS`: validate JSON settings, check for equivalent active workloads,
  and submit once.
- `list`: list workloads.
- `config [ENGINE]`: show available engines/books or one engine's configuration.
- `status ID`: inspect a workload's settings, progress, and result.
- `download-pgns ID`: download a finished workload; workers and uploads must also
  be ready.

Use the web UI's **Stop** action, then check `status ID`. The pinned client's
`stop` request is incompatible with this server. In-flight results may arrive later.

### Options

Preparation options use executable paths for engines:

| Option | Mode | Meaning |
| --- | --- | --- |
| `--candidate REV`, `--candidate-engine FILE` | All | Required candidate revision and matching binary. |
| `--baseline REV`, `--baseline-engine FILE` | `sprt`, `games` | Baseline revision and matching binary; required for comparisons. |
| `--self-play` | `games` | Use the candidate for both sides instead of baseline arguments. |
| `--games N` | `games`, `corpus` | Positive, even game count; required for `games`, default 42,000 for `corpus`. |
| `--worker-concurrency N` | `corpus` | Required concurrency of the single worker. |
| `--description TEXT` | All | Required workload description. |
| `--output DIR` | All | Required new directory for settings and bench output. |
| `--timeout SECONDS` | All | Timeout per bench run; default 600. |

For server commands, place `--config FILE` before the command to use another
credential file. Output requirements vary:

- `submit` requires either a new `--output DIR` or `--dry-run` to run preflight
  without creating files or a workload.
- `download-pgns` requires `--output DIR`; existing download filenames are refused.
- `status` optionally accepts `--output FILE` to save JSON to a new file.

See command-specific `--help` and the [pinned client guide][openbench-client-guide]
for complete settings and recovery details.

### Results

Preparation saves `settings.json`, `candidate-bench.txt`,
and, for comparisons, `baseline-bench.txt`, with matching `.stderr.txt` files.
Bench failures leave diagnostics but no settings.

Downloads retain `ID.pgn.tar`, its `.sha256` checksum, and `ID.workload.json`.
Submissions retain `submission.json` in the chosen output directory.

If submission fails or is interrupted, inspect `submission.json` if present and
the server state using `list`, `status`, or the web UI before another attempt.
Reuse the saved settings; do not blindly repeat submission.

For custom tests, edit the generated `settings.json` before submitting.
The [templates](../tools/resources/openbench/) are incomplete until preparation
fills in revisions, bench counts, and workload details.

## `uci-probe`

`./bin/uci-probe BUILD` runs a single search to inspect an engine's scores
and move selection. This example uses the `release-dev` build in `build/` to search
the position after `g1f3 g8f6`:

```bash
./bin/uci-probe release-dev \
  --fen 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' \
  --moves g1f3 g8f6 --nodes 500000
```

- `--fen FEN`: required starting position with all six FEN fields.
- `--moves MOVE...`: optional move history in UCI notation, played from the FEN.
  Include known history for repetition detection.
- `--nodes N`: required node limit.
- `--timeout SECONDS`: timeout for the entire session, in whole seconds (default: 90).

The output is a text transcript: `>` marks UCI commands sent to the engine, and
`<` marks output received from it, including search depth, scores, principal
variations, and the final `bestmove`. Redirect it to a file or use `tee` to save
it. Position and move legality are left to the engine.

## `profile`

`./bin/profile` records where the benchmark spends CPU time and collects cycle
and instruction counts. Requires Linux `perf` with DWARF unwinding support.

### Usage

Build with Release optimization and debug symbols, then profile the build:

```bash
cmake --preset profile
cmake --build --preset profile
./bin/profile profile --name pawn-eval
```

### Options

`BUILD` selects `build/BUILD/latrunculi-benchmark`; arguments containing `/` select
an executable directly. Names resolve within this checkout, and relative paths
use the caller's working directory.

| Option | Meaning |
| --- | --- |
| `--name NAME` | Save under `tools/output/NAME/`. |
| `--output PATH` | Explicit output directory instead of `--name`. |
| `-- ARG...` | Forward benchmark options, such as `--case ID`, `--depth N`, or `--suite FILE`. |

`profile` requires exactly one output option and a new destination directory.
Without forwarded options, the benchmark uses depth 10, one thread, and Hash=32.

### Results

`report.txt` contains sampled hotspots; `counters.txt` contains cycle and
instruction counts. The output directory also retains `perf.data`, both search
outputs, and the profiled executable.

The workload runs twice: once for sampling and once for hardware counters.
Measurements include setup and clearing as well as search; use an unprofiled
Release build for speed comparisons. Failed runs retain partial outputs.

## Tests

Run the [tool tests](../tools/tests/) after changing the tools or their feature-export interface:

```bash
tools/.venv/bin/python -m unittest discover -s tools/tests -t . -p 'test_*.py'
```

Tests use temporary fixtures without contacting OpenBench or running engine games.
Run UCI probes and tests untraced if their subprocesses stall under tracing.

[openbench-client-guide]: https://github.com/zm-bm/OpenBench/blob/89010f054df2713a2a5c6762bdf907a69b5d0a31/client-cli/README.md
