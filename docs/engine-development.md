# Engine Development

Build, test, compare, then decide whether to keep a change. Use the
[research ideas](engine-ideas.md) for experiments and the [roadmap](roadmap.md)
for longer-term features. Neither requires an issue number or a prescribed workflow.

## Save a baseline

Start with a clean checkout (`git status --short` should print nothing). From the
repository root, build the revision you want to compare against:

```bash
git switch main
git pull --ff-only
git submodule update --init --recursive
cmake --preset release-dev
cmake --build --preset release-dev
ctest --preset release-dev
mkdir build/my-change-base
cp build/release-dev/latrunculi build/release-dev/latrunculi-search-bench build/my-change-base/
git rev-parse HEAD > build/my-change-base/revision.txt
git switch -c my-change
```

Choose a new branch and baseline directory for each experiment; don't overwrite
binaries supporting earlier results. Saving both binaries lets you rebuild the
candidate in the same checkout. A separate checkout or worktree is optional.

## Change and test

Make one focused change, then rebuild and run the tests:

```bash
cmake --build --preset release-dev
ctest --preset release-dev
```

Add a regression when it protects the behavior being changed. Run a focused test
while iterating, then the complete suite before merging. For memory/lifetime or
arithmetic changes, also use the `debug-asan-ubsan` configure, build, and test
presets; use `debug-tsan` for affected threading behavior. See
[sanitizer execution](../tools/analysis/README.md#sanitizers).

## Compare locally

Set up the [shared Python environment](../tools/README.md#setup) once. For a quick
look at search or performance changes:

```bash
tools/.venv/bin/python tools/analysis/bench.py compare \
  build/my-change-base/latrunculi-search-bench build/release-dev/latrunculi-search-bench \
  --depth 8 --pairs 2 --output tools/analysis/output/my-change-quick
```

Read `summary.txt`; keep raw results with the revisions and settings they measure.
Quick results help decide whether to continue. Before making a performance or
search claim, complete the [local validation checks](../tools/analysis/README.md#validation)
and run the standard depth-10, six-pair comparison by omitting `--depth` and
`--pairs`, with a fresh output directory. Use the same compiler/settings for both
binaries and run one CPU-sensitive task at a time.

Add `--exact-tree` only when search behavior should remain identical. Fewer nodes
do not necessarily mean faster search, and faster search does not establish
stronger play. Small or noisy differences can remain unresolved.

## Test playing strength when needed

Small documentation or tooling changes need no games. Evaluation tuning and
strength-oriented search changes need game evidence; a narrow correctness fix
may instead be justified by a regression and relevant local checks. Decide the
intended benefit and acceptable tradeoffs before testing.

Commit the candidate before strength testing, then record its SHA with
`git rev-parse HEAD`. Push the tested revision so workers can build it. Follow the
[OpenBench guide](../tools/openbench.md#submit-and-stop) to prepare settings using
that candidate as Dev and the saved baseline revision as Base:

```bash
tools/.venv/bin/python -m openbench_client submit settings.json --dry-run
tools/.venv/bin/python -m openbench_client submit settings.json \
  --output tools/analysis/output/my-change-openbench
```

Inspect results in the web UI or with `status ID`. Let the declared SPRT reach a
boundary; stopping early is inconclusive. The guide describes workload settings,
fingerprints, and the current web-UI stop fallback. For fitting evaluation weights,
use the [tuning guide](../tools/tuning/README.md).

## Review and merge

Review the complete diff and confirm that required checks passed for the revision
you are merging. A short commit or PR explanation should state what changed, why,
and how it was checked. A PR is optional for your own local work.

For an engine change, build the proposed merged result and run the full Release
suite, standard corpus, and fingerprint. Compare with the accepted candidate's
signatures; repeat affected checks if the combination changes behavior. Verify
that the actual merged source/build inputs match what was checked.

For a local branch whose baseline has not moved:

```bash
git diff main...my-change
git switch main
git pull --ff-only
git merge --ff-only my-change
```

If main advanced and the fast-forward fails, combine the changes deliberately
and reassess the affected checks before merging. When using a PR, check its exact
head and target, then squash-merge with a short single-line message.

### Reusing evidence

Results belong to their measured revisions and conditions. Documentation or
test-only changes can reuse engine evidence when relevant inputs are unchanged;
combining engine changes requires local combination checks and fresh games when
strength interactions remain unresolved. Keep a brief reason for reuse with the
results. Reuse avoids unnecessary reruns, not unresolved failures.

Keep useful inputs, results, and the measured SHAs under the existing ignored
output directories. Record an outcome in a PR or a short local note when useful;
there is no separate status board to maintain. Remove obsolete branches and
scratch builds when you no longer need them, preserving reproducible evidence.
