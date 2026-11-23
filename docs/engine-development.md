# Engine Development

This guide describes how to develop changes to Latrunculi and assess whether
they are ready to merge or release. The checks depend on what the change affects.

See [Development scripts](scripts.md) for command usage and the
[roadmap](roadmap.md) for improvement ideas.

- [Branch and baseline](#branch-and-baseline): Create a branch and save a comparison baseline.
- [Build and test](#build-and-test): Build the engine and run applicable tests and sanitizers.
- [Benchmark and profile](#benchmark-and-profile): Compare runtime and search behavior, inspect positions, and locate CPU costs.
- [Test strength](#test-strength): Choose OpenBench tests and interpret their results.
- [Merge](#merge): Review the candidate, validate the merged result, and retain evidence.
- [Release](#release): Check stability, update release information, and publish verified artifacts.

## Branch and baseline

From a clean checkout, create a branch at the starting revision, usually `main`:

```bash
git switch -c my-change main
```

For runtime or search comparisons, save a [baseline](scripts.md#baseline)
before editing:

```bash
./bin/baseline baseline
```

## Build and test

For engine changes, the full Release suite and applicable sanitizer test runs
must pass before timing comparisons or merging:

```bash
cmake --preset release-dev
cmake --build --preset release-dev
ctest --preset release-dev
```

All builds use the pinned HalfKP NNUE network with automatic backend selection.
Every executable embeds the same frozen weights; ship the executable alone.
Build presets use two jobs; CTest presets run sequentially and fail if no tests
are found. The six configure/build presets cover the regular workflows:

| Preset | Contents |
| --- | --- |
| `release` | Optimized engine only |
| `release-dev` | Optimized engine, tests, and benchmark; compilation database for clangd |
| `release-tuning` | Development build with SPSA parameters exposed |
| `debug-asan-ubsan` | Debug engine and tests with address/undefined behavior sanitizers |
| `debug-tsan` | Debug engine and tests with the thread sanitizer |
| `profile` | Release benchmark with debug symbols, without test targets |

For plain Debug or search statistics, override the development preset in a
separate build directory:

```bash
cmake --preset release-dev -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --parallel 2
ctest --preset release-dev --test-dir build/debug

cmake --preset release-dev -B build/release-stats -DLATRUNCULI_SEARCH_STATS=ON
cmake --build build/release-stats --parallel 2
ctest --preset release-dev --test-dir build/release-stats
```

Keep `build/release-dev/compile_commands.json` as the development compilation
database. Diagnostic settings, tuning, sanitizers, and slider backend options
remain explicit CMake settings; extra combinations do not need their own preset.

Changes to search state, time controls, or thread lifetime need tests with
repeated searches in one engine session. For changes to Python tools, run the
[tool tests](scripts.md#tests).

### Sanitizers

Use `debug-asan-ubsan` for arithmetic, memory, or object lifetime changes and
`debug-tsan` for threading changes. Run both when applicable.

```bash
cmake --preset debug-asan-ubsan
cmake --build --preset debug-asan-ubsan
ctest --preset debug-asan-ubsan
```

Keep leak detection enabled and run LeakSanitizer without a debugger or tracer.
To include SPSA code in sanitizer checks, add the tuning flag in a separate
build directory:

```bash
cmake --preset debug-asan-ubsan -B build/asan-tuning -DLATRUNCULI_TUNING=ON
cmake --build build/asan-tuning --parallel 2
ctest --preset debug-asan-ubsan --test-dir build/asan-tuning

cmake --preset debug-tsan -B build/tsan-tuning -DLATRUNCULI_TUNING=ON
cmake --build build/tsan-tuning --parallel 2
ctest --preset debug-tsan --test-dir build/tsan-tuning
```

## Benchmark and profile

### Compare locally

Compare the `release-dev` candidate with the saved baseline to assess runtime
and search changes. Start with a quick exploratory comparison:

```bash
./bin/benchmark compare baseline --depth 8 --pairs 2 --name quick-comparison
```

Before claiming a search or performance improvement, run the standard depth-10,
six-pair comparison:

```bash
./bin/benchmark compare baseline --name comparison
```

Use matching compiler and build settings and avoid competing CPU activity.
Add `--exact-tree` for changes intended to preserve the search tree.
These rough reference points help interpret the [benchmark results](scripts.md#benchmark):

| Result | Metric | Approximate change |
| --- | --- | --- |
| Smaller search tree | `geometric_mean_node_ratio` | 1% fewer nodes |
| Faster search | `median_balanced_search_time_ratio` | 0.75% less time |
| Slowdown worth reviewing | `median_balanced_search_time_ratio` | 1% more time |

These are not acceptance thresholds: timing variation and the intended benefit
matter. Fewer nodes need not mean faster search, and a slowdown may be justified
by [playing-strength evidence](#test-strength).

### Investigate a position

Position analysis can explain changed moves or scores and help diagnose search
or evaluation errors. Compare baseline and candidate with
[uci-probe](scripts.md#uci-probe), using the same starting position, known move
history, and node budget, and retain both transcripts.

### Profile CPU costs

[Profiling](scripts.md#profile) can locate runtime costs and optimization
opportunities. Start with the standard position suite; use a focused workload to
investigate a specific cost. Confirm improvements with the standard comparison
using unprofiled Release builds.

```bash
cmake --preset profile
cmake --build --preset profile
./bin/profile profile --name cpu-profile
```

## Test strength

Changes to neural weights and strength-oriented search changes need game evidence.
A narrow correctness fix may be justified by a regression test and local checks.
Use a two-game `Smoke` test when build or match integration needs checking.

Complete applicable [local checks](#build-and-test) and
[comparisons](#compare-locally), then commit and push the tested revisions.
Prepare and submit with [OpenBench](scripts.md#openbench) using binaries matching
those revisions, then check the workload against the saved settings. Prefix
descriptions with `STC`, `Confirm`, or `Smoke` for strength, confirmation, or
plumbing tests.

### SPRT tests

The candidate (Dev) plays the pre-change baseline (Base), with paired colors.
The [SPRT template](../tools/resources/openbench/sprt.json) defines Latrunculi's
default conditions: `10+0.1`, `Threads=1 Hash=32`, UHO openings, DEV speed scaling,
and adjudication settings. Keep conditions consistent across related comparisons.

Declare the intended benefit, acceptable cost, and bounds before play, and keep
them fixed. Tests use normalized-Elo SPRT with alpha=beta=0.05:

| Purpose | Bounds |
| --- | --- |
| Incremental change or confirmation | `[0,3]` |
| Larger expected gain | `[0,5]` |
| Explicitly justified small strength tradeoff | `[-3,0]` |

SPRT has no fixed game count. The upper LLR boundary accepts under the declared
profile; the lower rejects. Stopping before either boundary is inconclusive.

Confirmation is required when declared in advance, after selecting variants from
game results, or to resolve conflicting evidence. State the question being
tested; longer time controls are optional. Do not rerun a rejected candidate
merely to obtain a pass.

### Self-play corpus

Self-play uses one revision for both sides. The [corpus template](../tools/resources/openbench/corpus.json)
defaults to 42,000 games at `4+0.04`. Before submission, check that one worker runs
one match runner; multiple workers or runners in the pinned OpenBench version
overlap opening ranges. The final game count may exceed the target.

Check the completed workload's revisions and game count, and retain its PGNs,
checksum, and metadata. Scores and fixed-test pass/fail flags do not establish
whether the games are suitable for training.

## Merge

For engine changes, the proposed merged result must be covered by the full
Release tests and standard position suite. Completed checks remain applicable
when engine/build inputs are unchanged. If integration changes those inputs,
rebuild, repeat affected checks, and compare search results with the accepted
candidate. Combining engine changes requires fresh games when strength
interactions remain unresolved.

Record the change, validation, and reasons for reusing evidence in the commit or
PR. Retain measured revisions, inputs, settings, and results in the existing
ignored output directories, including OpenBench workload links and final results.

## Release

Update the version in [CMakeLists.txt](../CMakeLists.txt), add
[release notes](releases/), and update their link in [README.md](../README.md).
Commit these changes and complete the [merge checks](#merge) on the final revision.

Releases with engine changes require 2,000 [self-play games](scripts.md#openbench)
with the pushed candidate using the [fixed-game template](../tools/resources/openbench/games.json),
[strength conditions](#sprt-tests), and compact PGNs. The run must complete without
crashes, hangs, time losses, illegal moves, protocol failures, or incomplete games.
The score and fixed-test pass/fail flag do not establish stability.

Package a source archive including pinned submodules and verify its build and
tests. Build the Linux x86-64 binary from that archive with the `release` preset,
test the self-contained executable on its supported platform, and
document its CPU and runtime requirements. Publish an annotated `vX.Y.Z` tag at
the verified revision and a GitHub release containing the archive, executable,
network license, and `SHA256SUMS`. Download the published assets
to check their checksums and confirm the binary's UCI version and bench count
against the verified build.
