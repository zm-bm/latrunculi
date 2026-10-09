# Latrunculi

Latrunculi is a free and open-source UCI chess engine written in C++23.

## Features

- Bitboards with magic sliding attacks
- Iterative-deepening PVS with aspiration windows and quiescence search
- Lazy SMP parallel search with a shared transposition table
- Staged move ordering, late move reductions, and null-move and futility pruning
- Handcrafted tapered evaluation with tunable weights

See [Architecture](docs/architecture.md) for how these fit together.

## Build

[Releases](https://github.com/zm-bm/latrunculi/releases) include a Linux x86-64
binary (requires POPCNT), a source archive, and checksums.

To build from source, you need GCC 13+ or Clang 18+, CMake 3.23+, and Git.

```bash
git clone --recurse-submodules https://github.com/zm-bm/latrunculi.git
cd latrunculi
cmake --preset release
cmake --build --preset release
```

On x86-64, POPCNT is enabled by default. For older processors, add
`-DLATRUNCULI_USE_POPCNT=OFF` to the configure command.

## Run

Add `build/release/latrunculi` to a UCI-compatible chess GUI, or run it directly:

```bash
./build/release/latrunculi
```

For a terminal test, enter the following commands. This uses four search
threads and 64 MiB for the transposition table:

```text
uci
setoption name Threads value 4
setoption name Hash value 64
isready
ucinewgame
position startpos moves e2e4 e7e5
go depth 10
```

Wait for `bestmove`, then enter `quit` to exit.

- `go movetime 1000` sets a one-second search limit; `go infinite` searches
  until you send `stop`.
- `help` lists console commands, including `d` to display the board, `eval`
  to score the position, and `perft <depth>` to count legal move sequences.

From the shell, `./build/release/latrunculi bench` runs the built-in benchmark.

## Development

Configure, build, and run the complete test suite:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

This runs unit tests and randomized stress tests. Use `debug-asan-ubsan` or
`debug-tsan` instead of `debug` to run the same suite with sanitizers.

See the [analysis guide](tools/analysis/README.md) for search benchmarks
and UCI probing, and the [tuning guide](tools/tuning/README.md) for evaluation
tuning.

## Documentation

- [Architecture](docs/architecture.md)
- [OpenBench testing](docs/openbench.md)
- [Current engine work](docs/engine-development.md)
- [Engine workflow skills](.agents/skills/)
- [1.1.0 release notes](docs/releases/1.1.0.md)
- [Roadmap](docs/roadmap.md)
- [UCI protocol reference](docs/uci-protocol-specification.txt)

## License

Latrunculi is licensed under the [GNU General Public License v3.0](LICENSE.txt).
