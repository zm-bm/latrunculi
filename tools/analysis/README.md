# Analysis

Use the search benchmark to compare saved runs, or `uci_probe.py` to inspect
one position through UCI.

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
order:

```bash
python3 tools/analysis/compare_search.py timing \
  --pair BC pair-1-baseline.tsv pair-1-candidate.tsv \
  --pair CB pair-2-baseline.tsv pair-2-candidate.tsv
```

Add more pairs with `--pair`. The tool compares summed search time and reports
`median_balanced_search_time_ratio` (`R_time_balanced`): the median of geometric
means of adjacent `BC,CB` candidate/baseline time ratios.

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
python3 -m unittest tools.analysis.test_compare_search tools.analysis.test_uci_probe
```
