---
name: check-latrunculi-openbench
description: Read one Latrunculi OpenBench test status or terminal result without mutating the test. Use for a requested status check, terminal-result retrieval, result analysis, or an ad hoc comparison involving already-integrated revisions.
---

# Check a Latrunculi OpenBench Test

Read `docs/openbench.md` and use `docs/engine-development.md` for current candidate state. A request
to check a test authorizes one read only; never poll, sleep, monitor repeatedly, stop, modify,
replace, submit, or integrate.

## Check Once

Fetch the named test once. Verify its identity, revisions, settings, and termination rule before
interpreting its state.

- **Running:** report the current games, score or Elo interval, and LLR when applicable. Do not put
  an intermediate snapshot in `docs/engine-development.md`.
- **Accepted:** for a candidate on the board, record the test ID, short revisions, terminal games,
  LLR, Elo interval, decision, and PGN location; keep it in **Candidates** with `Next: integrate`.
- **Rejected:** preserve the same terminal evidence and move the candidate to
  **Recent results**.
- **Inconclusive or interrupted:** preserve terminal evidence and keep the candidate with its exact
  next decision; do not call it accepted or rejected.

An ad hoc comparison, including a test between already-integrated revisions, does not change the
state board. Default settings need not be repeated. Report it directly and distill a later durable
methodology lesson only when requested.
