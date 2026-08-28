# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance for strength and
release-stability testing.

## Deployment

- Fork: `zm-bm/OpenBench`
- Checkout: `~/code/tools/OpenBench`
- Runtime: dedicated Python 3.11 virtual environment and one Gunicorn process
- State: SQLite database and server PGNs inside the OpenBench checkout
- Services: `openbench-server`, `openbench-worker`, and
  `openbench-backup.timer` as lingering user systemd units
- Worker limit: 12 threads, 8 GiB memory, and 512 tasks
- Backups: `~/.local/share/openbench/backups/`

Host configuration and generated credentials live in
`~/.config/openbench/openbench.env` with mode `0600`; never commit them. The
server listens on loopback and the private IPv4 address configured there. It is
plain HTTP for a trusted LAN only: do not forward port 8000 to the Internet.
Other machines on the same LAN may use `http://<private-ip>:8000` when peer
traffic is allowed.

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
- normalized-Elo SPRT `[0, 5]` for screening or `[0, 3]` for confirmation,
  with `alpha = beta = 0.05`

Use `Smoke` for plumbing, `STC` for candidate screening, and `Confirm` for a
retained batch. The normal worker runs games concurrently; use a temporary
one-thread worker when a smoke PGN must contain exactly one color-reversed pair.

Record the test ID, both revisions, OpenBench revision, decision, and server PGN
location for retained claims.

### Release stability test

Before a public release with engine changes, run the pushed candidate as both
Dev and Base in a fixed, non-SPRT test with a 2,000-game target (1,000 pairs)
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
```

Use systemd to stop or restart OpenBench so PGN and database writes shut down
cleanly. If the workstation's private address changes, update
`OPENBENCH_BIND` and `OPENBENCH_ALLOWED_HOSTS` in the host environment, then
restart both services.
