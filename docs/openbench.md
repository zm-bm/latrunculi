# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance for strength and
release-stability testing.

## Deployment

- Fork: `zm-bm/OpenBench`
- Checkout: `~/code/tools/OpenBench`
- Runtime: dedicated Python 3.11 virtual environment and one Gunicorn process
- State: SQLite database and server PGNs inside the OpenBench checkout
- Remote access: Tailscale Serve over HTTPS
- Services: `openbench-server`, `openbench-worker`, and
  `openbench-backup.timer` as lingering user systemd units
- Worker limit: 12 threads, 8 GiB memory, and 512 tasks
- Backups: `~/.local/share/openbench/backups/`

Host configuration and generated credentials live in
`~/.config/openbench/openbench.env` with mode `0600`; never commit them. The
server listens on loopback and the private IPv4 address configured there. It is
plain HTTP for a trusted LAN only: do not forward port 8000 to the Internet.
Direct `http://<private-ip>:8000` access is a diagnostic fallback, not the
normal client endpoint.

Use Tailscale Serve as the canonical client path, including from the LAN:
`https://server.<tailnet>.ts.net`. Tailscale terminates HTTPS and forwards to
OpenBench; do not append port 8000. Keep this private with Tailscale Serve, not
Funnel. `tailscale status` reports the exact hostname.

Set `OPENBENCH_SERVER` to that MagicDNS URL in the private host configuration
and use it for submissions and status requests. Do not guess a LAN address when
it is unset. Loopback is valid only on the server host, even when its environment
file was copied for the OpenBench credentials.

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

### Strength tests

Compare the candidate as Dev against the pre-change revision as Base. Play
paired games with the engines swapping colors. Use:

- `UHO_Lichess_4852_v1.epd`
- `10+0.1`, normalized to worker speed
- `Threads=1 Hash=32`
- resign at 400 cp for three moves
- draw after move 40 with eight evaluations within 10 cp
- `max_games = 8000` unless the active task predeclares another positive even cap
- a predeclared normalized-Elo SPRT profile with `alpha = beta = 0.05`:
  `[0, 5]` when screening for a larger gain, or `[0, 3]` for an incremental
  candidate or confirmation

Choose one profile before games begin according to the task's expected effect
and acceptance policy. An upper-bound result is conclusive for that predeclared
test; a second confirmation is not automatic. Require confirmation when the
task predeclares it, when the candidate was selected from several tested
variants, or when risk or a result that conflicts with other evidence warrants
it.
Do not use confirmation to retry or override a lower-bound result.

The game cap bounds resource use; it is not a third statistical decision.
OpenBench may finish a few in-flight games beyond it. If neither SPRT bound has
been crossed at the cap, retain the candidate as capped and inconclusive until
the user explicitly continues, replaces, accepts, or rejects the test.
Apply this default to new submissions; do not retrofit a cap onto an active test
unless the user explicitly requests that mutation.

Use `Smoke` for plumbing, `STC` for a candidate test, and `Confirm` for a
separately justified confirmation. The normal worker runs games concurrently;
use a temporary one-thread worker when a smoke PGN must contain exactly one
color-reversed pair.

Record the test ID, profile, both revisions, OpenBench revision, decision, and
server PGN location for retained claims.

After submitting a test, fetch status once to confirm its identity, revisions,
settings, cap, and running state. Record the test URL and return control; do not
hold an agent turn open with recurring polling or sleeps. Managed OpenBench
workers continue independently. Inspect status and collect terminal artifacts
when the user resumes the task.

### Release stability test

Before a public release with engine changes, run the pushed candidate as both
Dev and Base in a fixed, non-SPRT test with `max_games = 2000` (1,000 pairs)
and compact PGNs. Use the book, time control, options, and adjudication above.
Require no crashes, hangs, time losses, illegal moves, protocol failures, or
incomplete games. Ignore the score. Record the test ID, candidate revision,
OpenBench revision, and PGN location.

OpenBench may finish a few in-flight games beyond the target. Its fixed-test
pass/fail flag follows the score, not stability.

## Operations

```bash
systemctl --user status openbench-server openbench-worker openbench-backup.timer
journalctl --user -u openbench-server -u openbench-worker -f
systemctl --user restart openbench-server openbench-worker
systemctl --user start openbench-backup.service
tailscale serve status
```

From a tailnet client, verify the peer and application separately:

```bash
tailscale ping server
curl --fail https://server.<tailnet>.ts.net/
```

Use systemd to stop or restart OpenBench so PGN and database writes shut down
cleanly. If the workstation's private address changes, update
`OPENBENCH_BIND` and `OPENBENCH_ALLOWED_HOSTS` in the host environment, then
restart both services. Keep the actual LAN address and MagicDNS hostname in
that environment file rather than this repository.
