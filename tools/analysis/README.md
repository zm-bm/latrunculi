# Analysis

Use the search benchmark to compare saved runs, or `uci_probe.py` to inspect
one position through UCI.

Profiling locates CPU cost; fixed-depth node counts describe the search tree.
Fewer nodes alone do not establish faster search or stronger play. Fresh paired
timing measures speed, while [OpenBench games](../../docs/openbench.md#strength-tests)
measure playing strength. A promising position or agreement with a reference
engine alone cannot establish a gain.

Install the Python dependencies once in a virtual environment:

```bash
python3 -m venv .cache/analysis-venv
.cache/analysis-venv/bin/python -m pip install -r tools/analysis/requirements.txt
source .cache/analysis-venv/bin/activate
```

Use that interpreter for the commands below. Collection and legality checking
use the pinned chess dependency; the existing node/timing comparisons use only
the standard library.

Apply the shared [measurement rules](../../.agents/references/measurement-rules.md)
when collecting local evidence.

## Benchmark and compare

Build the benchmark, then save a run from the appropriate revision:

```bash
cmake --preset release-dev
cmake --build --preset release-dev --target latrunculi-search-bench
./build/release-dev/latrunculi-search-bench --depth 10 > run.tsv
```

The default [search.epd](search.epd) contains 200 Arasan positions. Each
starts with cleared transposition table and search heuristics, without game
history. Choose `--suite EPD` or `--case ID`, and set `--depth N`,
`--nodes N`, or `--movetime MS`. Defaults are depth 5, one thread, and 32 MiB Hash;
`--threads` and `--hash` override the latter two.
The suite retains positions and annotations from Git blob
`93cbe97d9ee40790eafc984e59cbce3c02a5d7ea`, with normalized IDs;
its `bm` and `am` labels provide context.

Compare matching baseline and candidate files, including a repeat of the
candidate:

```bash
python3 tools/analysis/compare_search.py nodes baseline.tsv candidate.tsv \
  --repeat candidate-repeat.tsv
```

The `search_measurement_v5` TSV records each position and request, completed
depth, root-perspective scores, nodes, best move, principal variation (PV), and
search nanoseconds from `start_search()` through `wait()`. Setup and output
time are excluded. The comparison tool requires matching cases, positions, and
settings. It checks repeat signatures; `--exact-tree` also requires
baseline/candidate signatures to match.

The output's `geometric_mean_node_ratio` (`R_node_g`) averages per-position
candidate/baseline node ratios geometrically; `total_node_ratio`
(`R_node_total`) divides total candidate nodes by total baseline nodes.
The tool accepts only current `search_measurement_v5` output. A
`release-stats` build reports counters on stderr; use a normal Release build
for timing.

### Paired timing

Give each baseline/candidate pair to the timing comparison tool with its run
order. Alternating the order on the same machine helps limit machine-speed
drift:

```bash
python3 tools/analysis/compare_search.py timing \
  --pair BC pair-1-baseline.tsv pair-1-candidate.tsv \
  --pair CB pair-2-baseline.tsv pair-2-candidate.tsv
```

Add more pairs with `--pair`. The tool compares summed search time and reports
`median_balanced_search_time_ratio` (`R_time_balanced`): the median of geometric
means of adjacent `BC,CB` candidate/baseline time ratios. Lower ratios mean
faster candidate search; a ratio of 1 means equal measured search time.

## Collect validation runs

`run_search_checks.py` collects evidence from already-built, recorded revisions.
It does not build, modify Git, or decide candidate acceptance. Give each command
a new output directory; an existing directory is refused, including one from
an interrupted attempt. Failed runs retain stdout and stderr for diagnosis.
Resume by running the missing phase into a new directory with applicable saved
references, rather than overwriting evidence.

Collect two fresh corpus passes, checking the complete suite, settings, legal
PVs, and repeat signatures:

```bash
python3 tools/analysis/run_search_checks.py corpus \
  --bench ./build/release-dev/latrunculi-search-bench \
  --output tools/analysis/output/candidate-corpus
```

Defaults are the standard 200-position suite, depth 10, one thread, 32 MiB
Hash, and two repetitions. Override `--suite`, `--depth`, `--threads`, `--hash`,
or `--repeats` for a diagnostic run. `--reference FILE.tsv` additionally requires
exact signatures against that retained run. For nondeterministic multithreaded
legality checks, collect individual passes with `--repeats 1` and no reference;
this does not replace single-thread repeatability checks.

Collect repeated fingerprints or check an integration against retained evidence:

```bash
python3 tools/analysis/run_search_checks.py fingerprint \
  --engine ./build/release-dev/latrunculi \
  --output tools/analysis/output/candidate-fingerprint
```

The engine runs `bench` twice by default. `--reference FILE.txt` checks its node
count against saved bench output; `--repeats 1` collects a single integration
check. Expected counts belong to the evidence, not the runner's source.

After required correctness checks pass, collect fresh paired timing using both
revisions' benchmark binaries and their own validated corpus references:

```bash
python3 tools/analysis/run_search_checks.py timing \
  --baseline-bench /path/to/baseline/build/release-dev/latrunculi-search-bench \
  --candidate-bench /path/to/candidate/build/release-dev/latrunculi-search-bench \
  --baseline-reference /path/to/baseline/corpus-1.tsv \
  --candidate-reference /path/to/candidate/corpus-1.tsv \
  --output tools/analysis/output/candidate-timing
```

Timing warms each binary once, then runs six alternating BC/CB pairs. It checks
every run against its revision's reference before producing `timing.txt` with
the existing comparison statistics. `--exact-tree` also requires both revisions
to match. An even `--pairs` value of at least two supports shorter diagnostic
runs; the [formal offline policy](../../.agents/skills/test-latrunculi-candidate/references/offline-checks.md#paired-timing)
requires six pairs.

Timing refuses tracing/ptrace. Optional `--cpu ID` uses `taskset` and discovers
the CPU's SMT siblings. `timing-load.jsonl` records a five-second idle sample
and available per-CPU busy fractions for every run; `--idle-seconds` changes the
sample duration. Inspect load and competing work before claiming a speed result.
Collection success alone does not establish an idle environment or a gain.
No machine-specific affinity or universal load threshold is assumed.

All three commands accept `--timeout SECONDS` per child process (default 600).
Their raw stdout and separate stderr files remain available on failure or
timeout. Search settings and references must agree. Dependency setup and shared
helpers are versioned here; experiment-specific mechanism fixtures stay with
their retained evidence or focused engine tests.

Check legality of existing corpus outputs independently:

```bash
python3 tools/analysis/compare_search.py legality baseline.tsv candidate.tsv
```

Use the existing CMake/CTest presets for Release and sanitizer suites, with
exit status determining success rather than a hardcoded test count:

```bash
cmake --preset debug-asan-ubsan
cmake --build --preset debug-asan-ubsan --parallel 4
ctest --preset debug-asan-ubsan
```

The sanitizer presets enable their required checks, including leak detection.
Use an environment meeting the shared
[sanitizer requirements](../../.agents/references/measurement-rules.md#sanitizers).
The `release-dev` and `debug-tsan` presets support their respective checks.
Select additional risks according to the candidate's change.

## CPU profiling

Use Linux `perf` to locate expensive functions and call paths before attempting
a speed optimization. Build a separate Release benchmark with debug symbols,
retaining optimization and LTO with search statistics disabled:

```bash
cmake --preset release-dev -B build/profile \
  -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -g' \
  -DLATRUNCULI_SEARCH_STATS=OFF
cmake --build build/profile --target latrunculi-search-bench --parallel 4
```

Record userspace samples and DWARF call stacks over the standard depth-10
corpus. This requires `perf` with DWARF unwinding support. Inspect the saved
profile in a text report or optionally open it in Hotspot:

```bash
PROFILE_DIR=tools/analysis/output/profile-baseline
mkdir -p "$PROFILE_DIR"
perf record -e cycles:u -F 199 --call-graph dwarf \
  -o "$PROFILE_DIR/perf.data" -- \
  ./build/profile/latrunculi-search-bench --depth 10 \
  > "$PROFILE_DIR/search.tsv"
perf report --stdio -i "$PROFILE_DIR/perf.data" > "$PROFILE_DIR/report.txt"
hotspot "$PROFILE_DIR/perf.data"  # Optional GUI
```

Inspect both a function's own cost and the cost of functions it calls. The
profile includes benchmark setup and TT clearing; distinguish those from search
work. Use `--case ID` or a deeper search for focused follow-up. Keep the matching
binary with debug symbols available while inspecting the profile.

Profiling identifies optimization opportunities. Verify any resulting speed
claim with [paired timing](#paired-timing) using ordinary Release builds without
the profiler.

## Probe one position

Install [requirements.txt](requirements.txt). Supply a six-field starting
Forsyth–Edwards Notation (FEN) and subsequent moves so clocks and known
repetition history survive:

```bash
python3 tools/analysis/uci_probe.py \
  --fen 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' \
  --moves g1f3 g8f6 --nodes 500000 -- ./build/release-dev/latrunculi
```

Options include `--multipv`, `--root-moves`, repeatable `--option NAME=VALUE`,
and `--timeout`; the engine command follows `--`. Defaults are MultiPV 1,
Threads 1, Hash 32 MiB, and 90 seconds. The `uci_probe_v1` JSON distinguishes
complete unbounded reports from partial or bounded ones, mate from centipawn
scores, and terminal roots from unavailable scores. Scripts can reuse
`UCI(command, options=None, timeout_s=90)`.

```bash
python3 -m unittest tools.analysis.test_compare_search tools.analysis.test_uci_probe \
  tools.analysis.test_run_search_checks
```
