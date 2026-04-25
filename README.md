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

## tests

```bash
ctest --preset release
```

`benchmark` is available for quick smoke checks during development.

## license

This project is licensed under the GNU General Public License v3.0.
