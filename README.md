# latrunculi

*uci chess engine written in c++23*

*not yet tuned for strength*

---

## at a glance

* **bitboard core** with magic bitboards
* **PVS search**
* **iterative deepening** with aspiration windows
* **transposition table**
* **multi-threaded search**
* **null-move pruning, LMR, and quiescence**
* **hand-crafted eval**

## getting started

### prerequisites

* g++ 13+ or clang++ 18
* cmake ≥3.23
* git (for submodules)

Tested on linux with g++ 13.3 / 14.2 and clang++ 18.

### build

```bash
git clone --recursive https://github.com/zm-bm/latrunculi.git
cd latrunculi
cmake --preset release
cmake --build --preset release
```

if you want a specific compiler:

```bash
cmake --preset release -D CMAKE_CXX_COMPILER=g++-13
cmake --build --preset release
```

if you forgot `--recursive`, run:

```bash
git submodule update --init --recursive
```

### run

```bash
./build/release/bin/latrunculi   # opens uci prompt
```

point any uci‑capable gui (cutechess, arena, etc.) at the binary, or try a quick terminal handshake like:

```text
uci
isready
ucinewgame
position startpos moves e2e4 e7e5
go depth 10
quit
```

set options if needed before `isready` / `go`:

```text
setoption name Threads value 4
setoption name Hash value 64
isready
go depth 20
```

## tests & benchmarks

```bash
ctest --preset release
./build/release/bin/benchmark --movetime 250 --limit 5
```

`benchmark` is a smoke benchmark: it is meant to stay cheap enough for routine iteration.
It reports whether short runs complete cleanly with plausible `bestmove` output plus raw observed
search metrics such as depth, nodes, time, and NPS. Treat it as a quick sanity/perf check, not as
the project's full engine-comparison framework.

For direct old-vs-new engine comparisons, use the fixed direct-UCI suite instead:

```bash
python3 bench/direct_uci_bench.py --threads 1,4 --movetime 1000 > direct-uci.tsv
```

The default suite lives in `bench/direct_uci_suite.tsv` and keeps a modest fixed set of positions.
For a minimal verification pass, run a single suite position at two thread counts, for example:
`python3 bench/direct_uci_bench.py --positions startpos --threads 1,4 --movetime 1000`
Capture the TSV columns exactly as emitted: `position_id`, `threads`, `movetime_ms`, `depth`,
`seldepth`, `score_type`, `score_value`, `nodes`, `time_ms`, `nps`, `bestmove`, and `pv`.
These direct-UCI runs are the reusable source of truth for apples-to-apples revision comparisons.

## license

This project is licensed under the GNU General Public License v3.0.
