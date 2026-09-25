# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance for strength and
release-stability testing.

## Access

The canonical private endpoint is
`https://workstation-01.<tailnet>.ts.net`. Access requires the tailnet; do not
append `:8000` or expose the service publicly. Client automation may set
`OPENBENCH_SERVER` to that URL and use private credentials, which must not be
committed.

Verify the peer and returned application separately:

```bash
tailscale ping workstation-01
curl --fail https://workstation-01.<tailnet>.ts.net/
```

A reachable peer or an HTTP response alone is insufficient: confirm that the
response is the OpenBench application. Deployment, services, persistent state,
backups, worker capacity, and NixOS integration belong to the OpenBench fork
and its `Deploy/README.md`, not this repository.

## Testing

OpenBench fetches revisions from GitHub, so commit and push each tested revision
before submitting a workload. The worker builds through `bench/Makefile`, checks
the deterministic node count, runs the games, and uploads results and PGNs.

The `latrunculi bench` command searches six fixed positions at depth 13 with one
thread and a 32 MiB transposition table. Its node count is the compatibility
signature; its NPS normalizes time controls across workers. Keep
`bench/Makefile` at this path because OpenBench uses one build path for both
revisions in a test. Build through the same adapter with:

```bash
make -C bench EXE=latrunculi CXX=g++
./bench/latrunculi bench
```

Benchmark nodes are always a signature gate: reproduce the candidate fingerprint
for tree-changing work and preserve the baseline fingerprint for tree-preserving work.
Neither their difference nor benchmark NPS is an offline performance or strength
metric; use the paired corpus timing policy in
[Engine Development](engine-development.md#paired-timing).

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
| SPSA | Predeclare `2 * pairs_per * iterations` games |
| Datagen | Predeclare positive `max_games` and any storage limit |

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
- a predeclared normalized-Elo SPRT profile with `alpha = beta = 0.05`:
  `[0, 5]` when testing for a larger gain, or `[0, 3]` for an incremental
  candidate or confirmation; a task may instead predeclare `[-3, 0]` when its
  acceptance policy explicitly tolerates a small strength tradeoff

Choose one profile before games begin. The upper boundary accepts the candidate
under that profile; the lower boundary rejects it. A second confirmation is not
automatic. Require one only when predeclared, when selecting among tested
variants, or when risk or conflicting evidence warrants it. Never use
confirmation to retry or override a lower-bound result.

Use `Smoke` for plumbing, `STC` for a candidate test, and `Confirm` for a
separately justified confirmation.

Record the test ID, profile, both revisions, OpenBench revision, decision, and
server PGN location for retained claims.

After submitting a test, fetch status once to confirm its identity, revisions,
settings, mode, applicable SPRT bounds or fixed-game count, and running state.
Record the test URL and return control; do not hold an agent turn open
with recurring polling or sleeps. OpenBench ends an SPRT at an LLR boundary and
a fixed test at its game count while managed workers continue independently.
Inspect status and collect terminal artifacts when the user resumes the task.

### Release stability test

Before a public release with engine changes, run the pushed candidate as both
Dev and Base in a fixed, non-SPRT test with `max_games = 2000` (1,000 pairs)
and compact PGNs. Use the book, time control, options, and adjudication above.
Require no crashes, hangs, time losses, illegal moves, protocol failures, or
incomplete games. Ignore the score. Record the test ID, candidate revision,
OpenBench revision, and PGN location.

Ignore OpenBench's fixed-test pass/fail flag here; it reflects score, not
stability.
