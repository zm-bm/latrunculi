# latrunculi

*a work‑in‑progress uci chess engine written in c++23*

---

## at a glance

* **bitboard core** with magic bitboards → O(1) slider attacks
* **principal variation search** (iterative deepening, transposition table, null‑move pruning, late‑move reductions)
* **tapered eval**: material, piece‑square tables, pawn structure, mobility, king safety

## getting started

### prerequisites

* c++23 compiler (clang ≥16, g++ ≥13)
* cmake ≥3.26
* git (for submodules)

### build

```bash
git clone --recursive https://github.com/zm-bm/latrunculi.git
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### run

```bash
./bin/latrunculi   # opens uci prompt
```

point any uci‑capable gui (cutechess, arena, etc.) at the binary and set options if needed:

```text
setoption name Threads value 4
setoption name Hash value 64
go depth 20
```

## tests & benchmarks

```bash
./bin/tests         # move-gen / unit tests 
./bin/benchmark     # nps measurements
```

## roadmap

* stack-based state
* phased move generation + ordering
* evaluation hash table
* pawn evaluation hash table
* better pawn structure evaluation
* drawn endgame detection
* syzygy endgame tablebase probing
* better time‑management heuristics
* ci with per‑commit sprt
* nnue‑style evaluation
* cross-platform support

## contributing

pull requests & issues welcome. run `clang-format -i` and use conventional commit prefixes (`feat:`, `fix:`, `refactor:`).

## license

This project is licensed under the GNU General Public License v3.0.
