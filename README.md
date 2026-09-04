# Latrunculi

Latrunculi is a free and open-source UCI chess engine written in C++23.

## Features

- Bitboard move generation with magic sliding attacks
- Multi-threaded iterative-deepening PVS with aspiration windows and quiescence search
- Transposition table, staged move ordering, and alpha-beta pruning and reductions
- Tapered handcrafted evaluation
- UCI search limits, `searchmoves`, infinite search, and pondering

## Build

Requires GCC 13+ or Clang 18+, CMake 3.23+, and Git.

```bash
git clone --recurse-submodules https://github.com/zm-bm/latrunculi.git
cd latrunculi
cmake --preset release
cmake --build --preset release
```

On x86-64, POPCNT is enabled by default. Disable it for older processors with
`-DLATRUNCULI_USE_POPCNT=OFF`.

## Releases

Each release provides `latrunculi-<version>-source.tar.gz`, a
`latrunculi-<version>-linux-x86_64` binary, and `SHA256SUMS`. The binary is
built with GCC using the `release` preset and targets x86-64 Linux with POPCNT.
Release notes record the exact revision, compiler, platform, benchmark, and
validation results. Other platforms should build from source.

## Run

Latrunculi communicates over standard input and output using UCI:

```bash
./build/release/latrunculi
```

A UCI-compatible GUI normally manages the session. For a quick terminal test:

```text
uci
setoption name Threads value 4
setoption name Hash value 64
isready
ucinewgame
position startpos moves e2e4 e7e5
go depth 10
quit
```

## Development

Configure, build, and run the complete test suite:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

CTest runs:

- `unit_tests` — deterministic board, move generation, evaluation, search, and
  UCI tests.
- `randomized_stress` — reproducible legal playouts, move round trips,
  evaluation checks, and short multi-threaded searches.

Use `debug-asan-ubsan` or `debug-tsan` instead of `debug` to run the same
suite with sanitizers.

See the [measurement guide](tools/measurements/README.md) for perft, evaluation,
and search workflows, and the [tuning workflow](tools/tuning/workflow.md) for
handcrafted-evaluation optimization.

## Documentation

- [Architecture](docs/architecture.md)
- [OpenBench operations](docs/openbench.md)
- [1.0.0 release evidence](docs/releases/1.0.0.md)
- [Roadmap](docs/roadmap.md)
- [UCI protocol reference](docs/uci-protocol-specification.txt)

## License

Latrunculi is licensed under the [GNU General Public License v3.0](LICENSE.txt).
