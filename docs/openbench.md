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
(NPS) is used to normalize time controls. A candidate must pass
[offline testing](engine-testing.md#offline-testing) before submission.

## Strength tests

OpenBench plays the candidate as Dev against the pre-change Base, swapping
colors across paired games. A sequential probability ratio test (SPRT)
continues until it accepts or rejects under a profile chosen before play.
Stopping earlier is inconclusive.

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
location for a finished workload. A result advances a candidate only when its
revisions match the live board entry and the baseline is still current.
