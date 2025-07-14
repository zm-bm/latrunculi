# latrunculi

*a work‑in‑progress uci chess engine written in modern c++23*

---

## at a glance

* **bitboard core** with magic bitboards → O(1) slider attacks
* **principal variation search** (iterative deepening, transposition table, null‑move pruning, late‑move reductions)
* **tapered eval**: material, piece‑square tables, pawn structure, mobility, king safety
* cross‑platform: linux, macos, windows (msvc)

## getting started

### prerequisites

* c++23 compiler (clang ≥16, g++ ≥13, msvc ≥19.38)
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
setoption name Threads value 8
setoption name Hash value 1024
go depth 20
```

## tests & benchmarks

```bash
./bin/tests         # move-gen / correctness
./bin/benchmark     # nps measurements
```

## roadmap

* nnue‑style evaluation
* endgame tablebase probing
* better time‑management heuristics
* ci with per‑commit sprt

## contributing

pull requests & issues welcome. run `clang-format -i` and use conventional commit prefixes (`feat:`, `fix:`, `refactor:`).

## license

This project is licensed under the GNU General Public License v3.0.
