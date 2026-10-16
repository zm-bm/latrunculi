# OpenBench

Latrunculi uses OpenBench for strength tests, tuning corpora, and release
stability checks. Use the [development manual](../docs/engine-development.md)
for the local workflow; this guide covers server operations and workload settings.

## Setup

Set up the [shared tools environment](README.md#setup) to install the OpenBench
client pinned in [requirements.txt](requirements.txt). The local
[submission wrapper](openbench-submit) prepares standard tests; use
`./tools/openbench-client` for inspection, downloads, and custom
submissions.
No local OpenBench checkout is required. The [pinned client guide][client]
documents all commands, settings fields, output records, and error recovery.

Credentials belong in `~/.config/openbench/openbench.env`: `OPENBENCH_SERVER`
(the HTTPS URL), `OPENBENCH_USERNAME`, and `OPENBENCH_PASSWORD`.

When updating the dependency pin, reinstall the requirements, check
`./tools/openbench-client --help`, and make one requested
read-only status check. Client tests and fixtures live in the OpenBench fork.

## Inspect and download

Run commands from the repository root:

```bash
./tools/openbench-client status 46
./tools/openbench-client download-pgns 46 \
  --output tools/tuning/output/corpus-46
```

These commands use #46 as an example; substitute your workload ID and a new output
directory. Verify the expected revisions and game count. Retain the archive,
checksum, and workload metadata together. Follow the
[tuning guide](tuning/README.md#prepared-data) to validate and prepare the games;
completion and workload scores alone do not establish corpus usability.

## Submit and stop

Before strength testing, complete applicable [local validation](analysis/README.md#validation).
Commit and push both tested revisions. A PR is optional. Build each revision with
the same adapter/compiler/settings and preserve the baseline binary before
changing revisions:

```bash
make -C bench EXE=latrunculi CXX=g++
```

The wrapper resolves Git references in the active checkout to full SHAs, runs each
supplied engine's `bench` twice, and fills a template with the matching node counts.
You must supply binaries built from the stated revisions: matching fingerprints
check repeatability, but do not prove the binary's provenance. Fingerprint node
counts identify behavior; NPS is used separately for time scaling.

Prepare an ordinary strength comparison from the repository root:

```bash
./tools/openbench-submit sprt \
  --candidate HEAD --candidate-engine bench/latrunculi \
  --baseline "$(cat build/my-change-base/revision.txt)" \
  --baseline-engine build/my-change-base/latrunculi \
  --description "STC: Describe the change and expected benefit" \
  --output tools/analysis/output/my-change-openbench
```

By default this creates files and runs the client's **dry run**, which checks
server availability and equivalent active workloads without creating a test.
Inspect `settings.json`, then use the printed command to submit those saved
settings without rerunning fingerprints:

```bash
./tools/openbench-client submit \
  tools/analysis/output/my-change-openbench/settings.json \
  --output tools/analysis/output/my-change-openbench/submission
```

Alternatively, add `--submit` to the original wrapper command to prepare and submit
once. The client performs preflight in either case. Both paths retain the client's
submission record in the `submission/` child directory. Check its readback: ID,
revisions, fingerprints, settings, termination rule, and active/pending state.

All modes require a new `--output` directory and retain raw fingerprint output in
`candidate/` and, for comparisons, `baseline/`. Paths are relative to your working
directory. `--timeout` sets seconds per fingerprint run (default 600); `--config`
forwards an alternative credential file to the client. The wrapper does not build,
push, or configure workers.

On failure, diagnostic files remain; fingerprint failures produce no settings.
A client failure leaves completed settings available for inspection. Fix a dry-run
error and retry the client's dry run against the saved file. If actual submission
fails or is interrupted, inspect `submission/submission.json` if present and
reconcile using `list`, `status`, or the web UI before another attempt. Do not
rerun the wrapper or blindly repeat submission.

**Stopping:** use the workload's Stop action in the web UI for now. The pinned
client's `stop` command omits a trailing slash required by this server, so its
request can fail before stopping anything. After stopping in the UI, use
`status ID` to verify the outcome. A boundary result reached before the stop still
counts; otherwise the comparison is inconclusive. In-flight results may arrive
later. See the [client guide][client] for other commands and recovery details.

### Explicit settings

The [SPRT](openbench/sprt.json), [fixed-game](openbench/games.json), and
[corpus](openbench/corpus.json) templates contain the project defaults. They omit
fields filled by the wrapper and cannot be submitted directly. For unusual tests,
edit the generated `settings.json`, run the client's dry run again, and submit
that file directly. The [client guide][client] documents every settings field.

## Strength tests

OpenBench plays the candidate as Dev against the pre-change Base, swapping colors
across paired games. Standard conditions are `UHO_Lichess_4852_v1.epd`, `10+0.1`,
`Threads=1 Hash=32`, resignation at 400 cp for three moves, and draws after move 40
with eight evaluations within 10 cp. Use matching time controls and a common
speed-scaling method for both engines; the templates use DEV scaling at 2,050,000
NPS with tablebase probing/adjudication disabled. Keep conditions consistent
across related comparisons and retain their actual values.

Use normalized-Elo SPRT with alpha=beta=0.05. Declare bounds before playing:

| Purpose | Bounds |
|---|---|
| Incremental change or confirmation | `[0,3]` |
| Larger expected gain | `[0,5]` |
| Explicitly justified small strength tradeoff | `[-3,0]` |

A tradeoff profile requires a declared benefit and acceptable cost before results
are known. Upper LLR boundary accepts under that profile; lower rejects. An SPRT
has no game cap: stopping before either boundary is inconclusive, including an
infrastructure interruption. Don't switch criteria after seeing results.

Confirmation is needed when declared in advance, variants were selected using
game results, or conflicting evidence leaves a concrete question unresolved.
State that question; confirmation isn't automatically a longer-time-control run
and cannot overturn a rejection of the same candidate. Use `STC` in descriptions
for ordinary strength tests, `Confirm` for justified confirmation, and `Smoke`
for plumbing checks. Concurrent tests are allowed; avoid duplicate submissions.

## Tuning corpus

Corpus mode uses one revision for both sides, fingerprints it twice, and defaults
to 42,000 games at `4+0.04`. Supply the concurrency of the single worker:

```bash
./tools/openbench-submit corpus \
  --candidate HEAD --candidate-engine bench/latrunculi \
  --worker-concurrency 12 \
  --description "Self-play corpus for evaluation tuning" \
  --output tools/tuning/output/my-corpus-workload
```

Use one worker with one match runner. The wrapper sets `workload_size` to
`ceil(games / (4 * worker concurrency))`; for 42,000 games and concurrency 12 this
is 875. Override the game count with `--games N`, a positive, even number.
Multiple assignments/runners in this OpenBench version overlap opening ranges.
The wrapper does not enforce the worker arrangement; configure it before actual
submission. The assignment and final game count may slightly exceed the target.
Scores and fixed-test pass/fail flags are irrelevant to fitting;
[preparation](tuning/README.md#prepared-data) decides data usability, including
minimum dataset sizes. Retain the workload identity and download provenance.

## Release stability

Before a public release with engine changes, test the pushed candidate against
itself using `GAMES`, `test_max_games: 2000`, and `N/A` bounds/confidence. Use the
strength conditions and compact PGNs:

```bash
./tools/openbench-submit games \
  --candidate HEAD --candidate-engine bench/latrunculi \
  --self-play --games 2000 \
  --description "Release stability self-play" \
  --output tools/analysis/output/release-stability
```

For a fixed-game comparison, replace `--self-play` with `--baseline REV` and
`--baseline-engine FILE`. Require no crashes, hangs, time losses,
illegal moves, protocol failures, or incomplete games; ignore the score and
fixed-test pass/fail flag. A plumbing smoke check instead uses two games.
Other fixed workloads require a positive, even game count chosen before play;
workers may finish a few in-flight games beyond the target.

## Tool tests

Run wrapper and fingerprint-runner regressions without contacting OpenBench or
running real engines:

```bash
tools/.venv/bin/python -m unittest tools.test_launchers tools.openbench.test_submit tools.analysis.test_bench
```

## Retained evidence

Keep the workload ID/URL, engine revisions, server revision if known, actual
settings, termination rule, result, and server PGN location with the measurements
or PR. For strength SPRTs, include games, terminal LLR, profile, and Elo interval.
Don't relabel an old test as evidence for a new engine. Apply the
[evidence-reuse guidance](../docs/engine-development.md#reusing-evidence) when
code, build inputs, or the baseline changes.

[client]: https://github.com/zm-bm/OpenBench/blob/89010f054df2713a2a5c6762bdf907a69b5d0a31/client-cli/README.md
