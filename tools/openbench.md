# OpenBench

Latrunculi uses OpenBench for strength tests, tuning corpora, and release
stability checks. Use the [development manual](../docs/engine-development.md)
for the local workflow; this guide covers server operations and workload settings.

## Setup

Set up the [shared tools environment](README.md#setup) to install the OpenBench
client pinned in [requirements.txt](requirements.txt). Run it directly with
`tools/.venv/bin/python -m openbench_client`.
No local OpenBench checkout is required. The [pinned client guide][client]
documents all commands, settings fields, output records, and error recovery.

Credentials belong in `~/.config/openbench/openbench.env`: `OPENBENCH_SERVER`
(the HTTPS URL), `OPENBENCH_USERNAME`, and `OPENBENCH_PASSWORD`.

When updating the dependency pin, reinstall the requirements, check
`tools/.venv/bin/python -m openbench_client --help`, and make one requested
read-only status check. Client tests and fixtures live in the OpenBench fork.

## Inspect and download

Run commands from the repository root:

```bash
tools/.venv/bin/python -m openbench_client status 46
tools/.venv/bin/python -m openbench_client download-pgns 46 \
  --output tools/tuning/output/corpus-46
```

These commands use #46 as an example; substitute your workload ID and a new output
directory. Verify the expected revisions and game count. Retain the archive,
checksum, and workload metadata together. Follow the
[tuning guide](tuning/README.md#prepared-data) to validate and prepare the games;
completion and workload scores alone do not establish corpus usability.

## Submit and stop

Before strength testing, complete applicable [local validation](analysis/README.md#validation).
Commit and push both tested revisions, then pin their full SHAs in the settings.
Check `list` for an existing comparison and `config` for engine/book availability.
A PR is optional; keep the result alongside its exact candidate and baseline.
Build both revisions through the shared adapter to obtain their fingerprints:

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
./bench/latrunculi bench
```

Repeat the fingerprint for each revision; node counts must match within a build.
The fingerprint identifies engine behavior, while NPS normalizes time controls.
Use the same adapter/compiler/settings on both sides. Prepare explicit settings
using the example and profiles below:

```bash
tools/.venv/bin/python -m openbench_client submit settings.json --dry-run
tools/.venv/bin/python -m openbench_client submit settings.json \
  --output tools/analysis/output/my-change-openbench
```

Retain the action record and check its readback: ID, revisions, fingerprints,
settings, termination rule, and active/pending state. If submission is uncertain,
inspect the saved record and reconcile using `list` and `status` before another
attempt; don't blindly repeat a submission.

**Stopping:** use the workload's Stop action in the web UI for now. The pinned
client's `stop` command omits a trailing slash required by this server, so its
request can fail before stopping anything. After stopping in the UI, use
`status ID` to verify the outcome. A boundary result reached before the stop still
counts; otherwise the comparison is inconclusive. In-flight results may arrive
later. See the [client guide][client] for other commands and recovery details.

### Explicit settings

Copy this example into `settings.json`, replacing both revisions, fingerprints,
repository URLs as needed, and the description. The supplied SHAs/counts are
illustrative first-pass values, not a request to rerun a historical workload.
The profiles below define how to adapt it for other workloads.

SPRT settings:

```json
{
  "dev_engine": "Latrunculi",
  "dev_repo": "https://github.com/zm-bm/latrunculi",
  "dev_branch": "4a4cc39c81c9b5dafe18032d7b42e4916e5fa55b",
  "dev_bench": 4353365,
  "dev_network": "",
  "dev_options": "Threads=1 Hash=32",
  "dev_time_control": "10+0.1",
  "base_engine": "Latrunculi",
  "base_repo": "https://github.com/zm-bm/latrunculi",
  "base_branch": "42d57fdaf35504ba706918e742a1b4ad7339420a",
  "base_bench": 3184133,
  "base_network": "",
  "base_options": "Threads=1 Hash=32",
  "base_time_control": "10+0.1",
  "test_mode": "SPRT",
  "test_bounds": "[0, 3]",
  "test_confidence": "[0.05, 0.05]",
  "test_max_games": "N/A",
  "book_name": "UHO_Lichess_4852_v1.epd",
  "upload_pgns": "COMPACT",
  "priority": 0,
  "throughput": 1000,
  "workload_size": 4,
  "syzygy_wdl": "DISABLED",
  "syzygy_adj": "DISABLED",
  "win_adj": "movecount=3 score=400",
  "draw_adj": "movenumber=40 movecount=8 score=10",
  "scale_method": "DEV",
  "scale_nps": 2050000,
  "info": "STC: replace with the change, expected benefit, and termination rule."
}
```

## Strength tests

OpenBench plays the candidate as Dev against the pre-change Base, swapping colors
across paired games. Standard conditions are `UHO_Lichess_4852_v1.epd`, `10+0.1`,
`Threads=1 Hash=32`, resignation at 400 cp for three moves, and draws after move 40
with eight evaluations within 10 cp. Use matching time controls and a common
speed-scaling method for both engines; the example uses DEV scaling at 2,050,000
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

Use the same pinned baseline for Dev and Base. Starting from the example, set
`test_mode` to `GAMES`, `test_bounds` and `test_confidence` to `N/A`,
`test_max_games` to 42000, and both time controls to `4+0.04`. Keep the strength
book, options, adjudication, and compact PGNs. Set both fingerprints to that
baseline's repeated count. Scores and fixed-test pass/fail flags are irrelevant
to fitting; [preparation](tuning/README.md#prepared-data) decides data usability.

Use one worker with one match runner. Set `workload_size` to
`ceil(21000 / (2 * worker concurrency))`; for concurrency 12 this is 875.
Multiple assignments/runners in this OpenBench version overlap opening ranges.
The assignment and final game count may slightly exceed the target. Retain the
workload identity and download provenance before preparing data.

## Release stability

Before a public release with engine changes, test the pushed candidate against
itself using `GAMES`, `test_max_games: 2000`, and `N/A` bounds/confidence. Use the
strength conditions and compact PGNs. Require no crashes, hangs, time losses,
illegal moves, protocol failures, or incomplete games; ignore the score and
fixed-test pass/fail flag. A plumbing smoke check instead uses two games.
Other fixed workloads require a positive, even game count chosen before play;
workers may finish a few in-flight games beyond the target.

## Retained evidence

Keep the workload ID/URL, engine revisions, server revision if known, actual
settings, termination rule, result, and server PGN location with the measurements
or PR. For strength SPRTs, include games, terminal LLR, profile, and Elo interval.
Don't relabel an old test as evidence for a new engine. Apply the
[evidence-reuse guidance](../docs/engine-development.md#reusing-evidence) when
code, build inputs, or the baseline changes.

[client]: https://github.com/zm-bm/OpenBench/blob/89010f054df2713a2a5c6762bdf907a69b5d0a31/client-cli/README.md
