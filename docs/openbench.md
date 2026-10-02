# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance for strength and
release-stability testing.

## Access

`~/.config/openbench/openbench.env` contains `OPENBENCH_SERVER`,
`OPENBENCH_USERNAME`, and `OPENBENCH_PASSWORD`. Use the configured HTTPS endpoint.

## Testing

Keep the shared build adapter at `bench/Makefile` for both revisions. Build and
check the benchmark fingerprint with:

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
```

The node count is a compatibility fingerprint; NPS normalizes time controls. Fingerprint
requirements and performance gates are defined in
[Engine Development](engine-development.md#how-candidates-are-tested).

### Test termination

Let an SPRT run until its LLR reaches either predeclared boundary. It has no
prescribed `max_games`. A manual stop before a boundary is inconclusive; an
infrastructure interruption supplies no decision.

Every non-SPRT workload needs a positive, predeclared game count. Paired-match
budgets must be even.

| Workload | Termination |
|---|---|
| Plumbing smoke (fixed) | `max_games = 2`, one color-reversed pair |
| Release stability (fixed) | `max_games = 2000` |
| Other fixed sample or gauntlet | Predeclared positive even `max_games` |

OpenBench may finish a few in-flight games beyond a fixed target. Do not use
fixed-game mode merely to impose an arbitrary ceiling on a strength SPRT.

### Strength tests

Compare the candidate as Dev against the pre-change revision as Base. Play
paired games with the engines swapping colors. Use:

- `UHO_Lichess_4852_v1.epd`
- `10+0.1`, normalized to worker speed
- `Threads=1 Hash=32`
- resign at 400 cp for three moves
- draw after move 40 with eight evaluations within 10 cp
- normalized-Elo SPRT with `alpha = beta = 0.05` and a predeclared profile:
  `[0, 5]`, `[0, 3]`, or `[-3, 0]`

The upper boundary accepts the candidate under its profile; the lower boundary
rejects it. Profile selection and confirmation policy belong to
[Engine Development](engine-development.md#game-acceptance).

Use `Smoke` for plumbing, `STC` for a candidate test, and `Confirm` for a
separately justified confirmation.

### Release stability test

Before a public release with engine changes, run the pushed candidate as both
Dev and Base in a fixed, non-SPRT test with `max_games = 2000` (1,000 pairs)
and compact PGNs. Use the book, time control, options, and adjudication above.
Require no crashes, hangs, time losses, illegal moves, protocol failures, or
incomplete games. Ignore the score and fixed-test pass/fail flag; they measure
score, not stability.

### Retained evidence

Record the test ID/URL, engine revisions, OpenBench revision, termination rule,
games, decision, and server PGN location. For strength SPRTs, also retain the
profile, terminal LLR, and Elo interval.
