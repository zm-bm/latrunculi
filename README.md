# Latrunculi

Latrunculi is a free and open-source UCI chess engine written in C++23.

**Estimated strength: ~2,950 CCRL Blitz Elo (v1.2.0).**
Unofficial estimate from 12,032 games against six reference engines at `10+0.1`;
see the [results and test conditions](docs/releases/1.2.0.md#ccrl-blitz-estimate).

## Features

- Bitboards with PEXT or magic sliding attacks
- Iterative-deepening PVS with aspiration windows and quiescence search
- Lazy SMP parallel search with a shared transposition table
- Staged move ordering, late move reductions, and null-move and futility pruning
- HalfKP NNUE evaluation with scalar and AVX2 inference

See [Architecture](docs/architecture.md) for how these fit together.

## Build

[Releases](https://github.com/zm-bm/latrunculi/releases) include a Linux x86-64
binary (requires POPCNT), a source archive, and checksums.

Default NNUE builds target Linux and need GCC 13+ or Clang 18+, CMake 3.23+, and Git.

```bash
git clone --recurse-submodules https://github.com/zm-bm/latrunculi.git
cd latrunculi
cmake --preset release
cmake --build --preset release
```

The build copies the pinned Stockfish 12 HalfKP network beside the executable.
Distribute `latrunculi` and `nn-82215d0fd0df.nnue` together in the same directory.
The engine loads that fixed file once; it does not search the working directory,
embed weights, or download a network. `AUTO` selects AVX2 when supported and
scalar inference otherwise. No Python is needed to build the engine. See
[NNUE](docs/nnue-halfkp.md) for network identity and verification.

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

## Documentation

- [Architecture](docs/architecture.md)
- [NNUE evaluation and verification](docs/nnue-halfkp.md)
- [OpenBench testing](docs/engine-development.md#test-strength)
- [Development manual](docs/engine-development.md)
- [Development scripts](docs/scripts.md)
- [Roadmap](docs/roadmap.md)
- [1.2.0 release notes](docs/releases/1.2.0.md)
- [UCI protocol reference](docs/uci-protocol-specification.txt)

## License

Latrunculi is licensed under the [GNU General Public License v3.0](LICENSE.txt).
The bundled network is distributed under [CC0](networks/LICENSE).
