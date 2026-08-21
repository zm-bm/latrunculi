# OpenBench

Latrunculi uses a private, self-hosted OpenBench instance for retained strength
testing.

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

OpenBench fetches revisions from GitHub, so commit and push both test revisions
before submitting a workload. Use the earlier revision as Base and the candidate
as Dev.

The Latrunculi defaults follow the [strength-validation
policy](roadmap.md#strength-validation-policy):

- `UHO_Lichess_4852_v1.epd`
- `10+0.1`, normalized to worker speed
- `Threads=1 Hash=32`
- resign at 400 cp for three moves
- draw after move 40 with eight evaluations within 10 cp
- normalized-Elo SPRT `[0, 5]`, or `[0, 3]` for confirmation

Use `Smoke` for plumbing, `STC` for candidate screening, and `Confirm` for a
retained batch. The normal worker runs games concurrently; use a temporary
one-thread worker when a smoke PGN must contain exactly one color-reversed pair.

The worker downloads the pinned book and source revisions, builds through
`bench/Makefile`, verifies the deterministic node count, runs the games, and
uploads results and PGNs. Record the test ID, both revisions, OpenBench revision,
decision, and server PGN location for retained claims.

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
