# Analysis

Use one native search workload with `bench.py` to collect and compare measurements.
Use `perf` to investigate CPU cost, and `uci_probe.py` for individual positions
with move history or restricted root moves. Games on
[OpenBench](../../docs/openbench.md#strength-tests) measure playing strength.

| File | Responsibility |
|---|---|
| `bench.py` | Command interface and collection of benchmark runs and fingerprints |
| `bench_results.py` | Reading, checking and summarizing benchmark results |
| `search_bench.cpp` | Native search workload and search-only timing |
| `search.epd` | Default position suite |
| `uci_probe.py` | Position investigations with history, restricted roots and UCI reports |
| `test_*.py` | Regressions for the corresponding Python modules |

Install the Python dependency in a virtual environment:

```bash
python3 -m venv .cache/analysis-venv
.cache/analysis-venv/bin/python -m pip install -r tools/analysis/requirements.txt
source .cache/analysis-venv/bin/activate
```

Build each recorded revision with the same compiler and settings:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --target latrunculi-search-bench latrunculi
```

Apply the shared [measurement rules](../../.agents/references/measurement-rules.md).
Only one local CPU-sensitive task should run at a time. The tools take already-built
binaries; they do not build, modify Git or decide candidate acceptance.

## Quick comparison

For exploratory viability, compare the full suite at a shallower depth with two
alternating pairs:

```bash
python3 tools/analysis/bench.py compare /path/to/baseline-bench /path/to/candidate-bench \
  --depth 8 --pairs 2 --output tools/analysis/output/quick-comparison
```

Add `--exact-tree` when the candidate must preserve the search tree. Otherwise,
baseline and candidate signatures may differ, but each binary must repeat its own
completed depth, static/searched scores, nodes, best move and PV. Both modes check
complete position sets, settings and PV legality independently with python-chess.
A quick comparison can expose clear regressions; small or noisy effects can remain
unresolved. Fewer nodes alone establish neither faster search nor stronger play.

## Paired timing

After required correctness and risk checks, use the same command with its standard
defaults: all 200 positions, depth 10, one thread, 32 MiB Hash and six pairs.

```bash
python3 tools/analysis/bench.py compare /path/to/baseline-bench /path/to/candidate-bench \
  --cpu 2 --output tools/analysis/output/candidate-timing
```

Choose an available CPU, preferably with little activity on its SMT sibling;
CPU 2 is only an example. `--cpu` is optional and uses Linux `taskset` to pin the
benchmark. Avoid substantial competing CPU work, such as builds or other
benchmarks; ordinary background desktop activity does not by itself prevent
collection. The runner refuses tracing/ptrace; collect timings without profiling
or instrumentation.

Each binary runs one warmup, which becomes its signature reference. Measured runs
use fresh processes in alternating `BC,CB` order; warmups do not enter the timing
statistics. No separately collected reference files are needed. `--pairs` accepts
an even count of at least two; shorter runs do not replace formal timing.

The single `summary.txt` includes search settings, node totals and ratios,
root-move/score changes, individual time ratios, variation and balanced timing:

- `geometric_mean_node_ratio` (`R_node_g`) geometrically averages per-position node ratios.
- `total_node_ratio` divides candidate total nodes by baseline total nodes.
- `median_balanced_search_time_ratio` (`R_time_balanced`) is the median of geometric
  means of adjacent `BC,CB` candidate/baseline search-time ratios.

Lower ratios mean fewer nodes or less search time. Formal sample counts and
acceptance targets belong to the [offline policy](../../.agents/skills/test-latrunculi-candidate/references/offline-checks.md),
not the runner. Successful collection is not an acceptance decision.

`timing-load.jsonl` records a five-second initial idle sample and available per-CPU
busy fractions for each process, with affinity and SMT siblings when selected.
`--idle-seconds` changes that initial sample. Assess recorded load and variation
across alternating blocks relative to the claimed effect; small or noisy
differences can remain unresolved. The runner imposes no universal load threshold
or retries.

## Corpus and fingerprints

Collect a single corpus pass, or request repeats and an optional exact reference:

```bash
python3 tools/analysis/bench.py run ./build/release-dev/latrunculi-search-bench \
  --repeats 2 --output tools/analysis/output/candidate-corpus
```

`run` defaults to one pass. `--reference FILE.tsv` checks each pass against retained
signatures. Both `run` and `compare` accept `--suite EPD`, `--depth`, `--threads`,
`--hash` and `--cpu`. Use a single `run` without a reference for nondeterministic
multithreaded legality checks; this does not replace single-thread repeatability.

Collect the engine's OpenBench compatibility fingerprint:

```bash
python3 tools/analysis/bench.py fingerprint ./build/release-dev/latrunculi \
  --output tools/analysis/output/candidate-fingerprint
```

This invokes the engine's existing six-position, depth-13 `bench` twice, checking
that its node count repeats. `--repeats 1` selects a single check;
`--reference FILE.txt` compares with a retained fingerprint. This is an identity
check, not another speed benchmark.

All collection commands require a new output directory and accept `--timeout`
seconds per child process (default 600). Raw TSV/stdout and separate stderr remain
available on failure. A failed comparison writes no final summary. Resume a
missing check into a new directory; do not overwrite earlier measurements.

CMake/CTest presets own the Release and applicable sanitizer suites. Use their exit
status rather than a hardcoded test count. See the shared
[sanitizer requirements](../../.agents/references/measurement-rules.md#sanitizers).

## Summarize saved runs

Recompute comparisons from existing `search_measurement_v5` files without running
engines:

```bash
python3 tools/analysis/bench.py summarize \
  --pair BC pair-1-baseline.tsv pair-1-candidate.tsv \
  --pair CB pair-2-baseline.tsv pair-2-candidate.tsv
```

Use one pair for node statistics and an explicitly diagnostic time ratio. Multiple
pairs must alternate `BC,CB` in complete blocks and use distinct files. The same
settings, legality, repeatability and optional `--exact-tree` checks apply.
Saved comparisons retain their original conditions; re-summarizing cannot turn
uncontrolled or historical timings into fresh paired evidence.

## Native workload

The default [search.epd](search.epd) contains 200 Arasan positions, retaining the
positions and annotations from Git blob `93cbe97d9ee40790eafc984e59cbce3c02a5d7ea`
with normalized IDs. Each search starts with cleared TT and search heuristics,
without game history. The `bm` and `am` annotations are context, not acceptance tests.

The native benchmark remains available directly:

```bash
./build/release-dev/latrunculi-search-bench --depth 10 > search.tsv
```

It accepts `--suite EPD`, `--case ID`, and one of `--depth N`, `--nodes N` or
`--movetime MS`, plus `--threads` and `--hash`. Its direct default is depth 5,
one thread and 32 MiB Hash; the Python runner explicitly selects depth 10.

The TSV records positions, requests, completed depth, root-perspective scores,
nodes, best move, PV and search nanoseconds from `start_search()` through `wait()`.
Allocation, clearing, setup and output are outside that timer. A `release-stats`
build emits search diagnostics to stderr; use ordinary Release builds for timing.

## CPU profiling

Build a separate optimized benchmark with debug symbols and statistics disabled:

```bash
cmake --preset release-dev -B build/profile \
  -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -g' -DLATRUNCULI_SEARCH_STATS=OFF
cmake --build build/profile --target latrunculi-search-bench --parallel 4
```

Use Linux `perf` directly on the same workload. DWARF stacks require a `perf` build
with unwinding support:

```bash
PROFILE_DIR=tools/analysis/output/profile-baseline
mkdir -p tools/analysis/output
mkdir "$PROFILE_DIR"
perf record -e cycles:u -F 199 --call-graph dwarf \
  -o "$PROFILE_DIR/perf.data" -- \
  ./build/profile/latrunculi-search-bench --depth 10 > "$PROFILE_DIR/search.tsv"
perf report --stdio -i "$PROFILE_DIR/perf.data" > "$PROFILE_DIR/report.txt"
perf stat -e cycles:u,instructions:u -o "$PROFILE_DIR/counters.txt" -- \
  ./build/profile/latrunculi-search-bench --depth 10 > "$PROFILE_DIR/counter-search.tsv"
```

Inspect function and call-path costs. These profiles and counter totals include
setup and clearing; they are not search-only measurements. Use `--case ID` or a
deeper search for focused investigation, and retain the matching debug binary
while inspecting profiles. Hotspot can optionally open `perf.data`.

Profiling helps locate cost or substantiate a mechanism. Confirm a speed claim
with separate paired timing using ordinary Release builds without the profiler.
Experiment-specific instrumentation stays with its retained evidence.

## Probe one position

Use a full six-field starting FEN and subsequent moves to preserve clocks and
known repetition history:

```bash
python3 tools/analysis/uci_probe.py \
  --fen 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' \
  --moves g1f3 g8f6 --nodes 500000 -- ./build/release-dev/latrunculi
```

Options include `--multipv`, `--root-moves`, repeatable `--option NAME=VALUE` and
`--timeout`. Defaults are MultiPV 1, Threads 1, Hash 32 MiB and 90 seconds. Other
engine defaults, including analysis mode, are preserved unless explicitly set.
The probe supports standard, non-pondering analysis; conflicting managed options
are rejected. Each probe sends a fresh-game reset.

The `uci_probe_v1` JSON distinguishes complete iterations from partial/bounded
reports, mate from centipawn scores, and terminal positions from missing scores.
Raw report semantics and independent legality checks are retained while
python-chess handles UCI transport. Scripts can reuse
`UCI(command, options=None, timeout_s=90)`.

Run the tool regressions in an environment supporting asyncio subprocesses. Use
untraced execution for UCI probes and tests when the environment is known to stall
python-chess's background event loop:

```bash
python3 -m unittest tools.analysis.test_bench_results tools.analysis.test_bench \
  tools.analysis.test_uci_probe
```
