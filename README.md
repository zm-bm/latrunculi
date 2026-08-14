# Latrunculi

Latrunculi is a UCI chess engine written in C++23. It is under active
development.

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

Developer presets enable tests and component measurements:

```bash
cmake --preset release-dev
cmake --build --preset release-dev
ctest --preset release-dev
```

See the [measurement guide](measurements/README.md) for perft, evaluation, and
search workflows.

## Documentation

- [Architecture](docs/architecture.md)
- [Roadmap](docs/roadmap.md)
- [UCI protocol reference](docs/uci-protocol-specification.txt)

## License

Latrunculi is licensed under the GNU General Public License v3.0.
