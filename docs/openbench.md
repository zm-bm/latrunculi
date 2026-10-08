# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance to measure playing
strength, collect evaluation-tuning games, and check release stability. The
[development board](engine-development.md) records live candidate decisions.

## Access and build

`~/.config/openbench/openbench.env` contains `OPENBENCH_SERVER`,
`OPENBENCH_USERNAME`, and `OPENBENCH_PASSWORD`. Use its configured HTTPS
endpoint. Both revisions use the shared `bench/Makefile` adapter:

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
```

The benchmark node count is a compatibility fingerprint; nodes per second
(NPS) is used to normalize time controls. Submission requires passing
[offline evidence](../.agents/skills/test-latrunculi-candidate/SKILL.md) applicable
to the candidate and baseline, including any recorded evidence-reuse decision.

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
[tuning guide](../tools/tuning/README.md#prepared-data) explains how the PGNs
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
