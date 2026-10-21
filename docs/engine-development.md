# Engine Development

This guide describes how to develop changes to Latrunculi and assess whether
they are ready to merge or release. The checks depend on what the change affects.

See [Development scripts](scripts.md) for command usage and the
[roadmap](roadmap.md) for improvement ideas.

- [Branch and baseline](#branch-and-baseline): Create a branch and save a comparison baseline.
- [Build and test](#build-and-test): Build the engine and run applicable tests and sanitizers.
- [Benchmark and profile](#benchmark-and-profile): Compare runtime and search behavior, inspect positions, and locate CPU costs.
- [Tune evaluation](#tune-evaluation): Select games, review fitted weights, and verify the compiled evaluation.
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

Changes to search state, time controls, or thread lifetime need tests with
repeated searches in one engine session. For changes to Python tools or engine
feature export, run the [tool tests](scripts.md#tests).

### Sanitizers

Use `debug-asan-ubsan` for arithmetic, memory, or object lifetime changes and
`debug-tsan` for threading changes. Run both when applicable.

```bash
cmake --preset debug-asan-ubsan
cmake --build --preset debug-asan-ubsan
ctest --preset debug-asan-ubsan
```

Keep leak detection enabled and run LeakSanitizer without a debugger or tracer.

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

## Tune evaluation

Use [OpenBench self-play](#self-play-corpus) with varied openings to generate the
tuning games, and record the starting engine revision and evaluation. Download
the completed PGNs with [openbench download-pgns](scripts.md#openbench).
Prepare the games with the starting engine. Changes to evaluation definitions
require fresh preparation:

```bash
./bin/tune prepare --engine release-dev --name pawns-data games.pgn.tar
./bin/tune fit --dataset pawns-data --name pawns-fit
```

Review `cross-validation.json` and `candidate.json` before applying weights:

- Grouped held-out validation must support the proposed weights, with related
  openings kept together. Lower training loss alone is insufficient.
- Investigate large coefficient changes and bound hits; they are review flags,
  not automatic failures.
- Do not change the [preparation](../tools/tuning/dataset.py) or
  [fitting](../tools/tuning/fit.py) policies merely to obtain a passing fit.

Apply the exact supported weights to [parameters.hpp](../src/eval/parameters.hpp),
then rebuild and verify the compiled evaluation:

```bash
cmake --build --preset release-dev
./bin/tune verify pawns-fit --engine release-dev
```

Complete the [engine checks](#build-and-test), [local comparison](#compare-locally),
and [strength test](#test-strength). Retain the games, dataset, complete fit, and
verification result as [reproducible evidence](#merge).

### Self-play corpus

Self-play uses one revision for both sides. The [corpus template](../tools/resources/openbench/corpus.json)
defaults to 42,000 games at `4+0.04`. Before submission, check that one worker runs
one match runner; multiple workers or runners in the pinned OpenBench version
overlap opening ranges. The final game count may exceed the target.

Check the completed workload's revisions and game count, and retain its PGNs,
checksum, and metadata. Scores and fixed-test pass/fail flags do not establish
tuning suitability; preparation checks the dataset requirements.

## Test strength

Evaluation tuning and strength-oriented search changes need game evidence.
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
test the packaged binary on its supported platform, and document its CPU and
runtime requirements. Publish an annotated `vX.Y.Z` tag at
the verified revision and a GitHub release containing the archive, binary, and
`SHA256SUMS`. Download the published assets to check their checksums and confirm
the binary's UCI version and bench count against the verified build.
