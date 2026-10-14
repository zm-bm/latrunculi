# OpenBench

Latrunculi uses OpenBench for strength tests, tuning corpora, and release
stability checks. The [development board](../docs/engine-development.md)
records candidate decisions; the [workload policy](../.agents/skills/manage-latrunculi-openbench/references/workloads-and-decisions.md)
defines test settings and acceptance rules.

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
  --output tools/tuning/output/eng-043-corpus
```

For ENG-043, verify 42,000 games and revision
`4a4cc39c81c9b5dafe18032d7b42e4916e5fa55b` on both sides. Retain the archive,
checksum, and workload metadata together. Follow the
[tuning guide](tuning/README.md#prepared-data) to validate and prepare the games;
completion and the workload score do not establish corpus usability.

## Submit and stop

The [management skill](../.agents/skills/manage-latrunculi-openbench/SKILL.md)
owns authorization, offline eligibility, and board/PR updates. Submission
requires applicable [offline evidence](../.agents/skills/test-latrunculi-candidate/SKILL.md).
Build both revisions through the shared adapter to obtain their fingerprints:

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
```

Prepare explicit settings using the examples below and the workload policy.
For an authorized submission:

```bash
tools/.venv/bin/python -m openbench_client submit settings.json --dry-run
tools/.venv/bin/python -m openbench_client submit settings.json \
  --output tools/analysis/output/eng-XXX-openbench
```

Retain the action record alongside the candidate's evidence. For stopping a
specifically requested workload or reconciling an uncertain action, follow the
[client's command and recovery instructions][client].

### Explicit settings

These complete examples describe the earlier #45 strength comparison and #46
corpus settings. They are references for preparing a new authorized workload,
not instructions to recreate those tests.

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
  "info": "ENG-042 STC: historical example; replace with the authorized experiment description."
}
```

Fixed-game corpus settings (workload size 875 is specific to the recorded
single runner with concurrency 12; apply the corpus policy for other workers):

```json
{
  "dev_engine": "Latrunculi",
  "dev_repo": "https://github.com/zm-bm/latrunculi",
  "dev_branch": "4a4cc39c81c9b5dafe18032d7b42e4916e5fa55b",
  "dev_bench": 4353365,
  "dev_network": "",
  "dev_options": "Threads=1 Hash=32",
  "dev_time_control": "4+0.04",
  "base_engine": "Latrunculi",
  "base_repo": "https://github.com/zm-bm/latrunculi",
  "base_branch": "4a4cc39c81c9b5dafe18032d7b42e4916e5fa55b",
  "base_bench": 4353365,
  "base_network": "",
  "base_options": "Threads=1 Hash=32",
  "base_time_control": "4+0.04",
  "test_mode": "GAMES",
  "test_bounds": "N/A",
  "test_confidence": "N/A",
  "test_max_games": 42000,
  "book_name": "UHO_Lichess_4852_v1.epd",
  "upload_pgns": "COMPACT",
  "priority": 0,
  "throughput": 1000,
  "workload_size": 875,
  "syzygy_wdl": "DISABLED",
  "syzygy_adj": "DISABLED",
  "win_adj": "movecount=3 score=400",
  "draw_adj": "movenumber=40 movecount=8 score=10",
  "scale_method": "DEV",
  "scale_nps": 2050000,
  "info": "ENG-043 corpus: historical example; replace with the authorized experiment description."
}
```

## Strength tests

OpenBench plays the candidate as Dev against the pre-change Base, swapping
colors across paired games. A sequential probability ratio test (SPRT)
continues until it accepts or rejects under a profile chosen before play.
Stopping earlier is inconclusive. Concurrent tests are allowed; check for an
existing requested workload before submitting another. Starting or stopping a
test requires an explicit request. Keep its link and terminal evidence on the
associated candidate PR.

## Tuning corpus

A fixed baseline-versus-baseline workload produces PGNs for evaluation
fitting; its score is not a strength result. The
[tuning guide](tuning/README.md#prepared-data) explains how the PGNs
become a usable dataset.

## Release stability

Before a public release with engine changes, a fixed candidate-versus-itself
workload checks for crashes, hangs, time losses, illegal moves, protocol
failures, and incomplete games. Its score does not decide stability.

## Retained evidence

Keep the identity, revisions, termination rule, result, and server PGN
location for a finished workload. A strength result measures play under the
tested revisions and conditions. For applicability after candidate or
baseline changes, follow the shared
[evidence-reuse rules](../.agents/references/candidate-rules.md#evidence-reuse).

[client]: https://github.com/zm-bm/OpenBench/blob/89010f054df2713a2a5c6762bdf907a69b5d0a31/client-cli/README.md
