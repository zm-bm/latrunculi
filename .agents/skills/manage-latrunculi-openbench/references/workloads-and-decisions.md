# OpenBench workloads and decisions

Use the configured HTTPS endpoint from `~/.config/openbench/openbench.env`. Keep the shared build adapter at `bench/Makefile` for both revisions. The `bench` node count is a compatibility fingerprint; NPS normalizes time controls. The [offline checks](../../test-latrunculi-candidate/references/offline-checks.md) own fingerprint requirements before submission.

## Termination

Let a strength sequential probability ratio test (SPRT) run until its log-likelihood ratio (LLR) reaches either predeclared boundary. It has no prescribed `max_games`. A manual stop before a boundary is inconclusive; an infrastructure interruption supplies no decision. A confirmation run cannot overturn an SPRT rejection at its boundary.

Every non-SPRT workload needs a positive, predeclared game count. Paired-match budgets must be even. Plumbing smoke uses `max_games = 2` (one color-reversed pair); release stability uses `max_games = 2000`. Other fixed samples and gauntlets need a predeclared positive even `max_games`. OpenBench may finish a few in-flight games beyond a fixed target. Do not use fixed-game mode to impose an arbitrary ceiling on a strength SPRT.

## Strength test

Compare the candidate as Dev against the pre-change revision as Base in paired games with colors reversed. Use `UHO_Lichess_4852_v1.epd`, `10+0.1` normalized to worker speed, `Threads=1 Hash=32`, resign at 400 cp for three moves, and draw after move 40 with eight evaluations within 10 cp. Use normalized-Elo SPRT with `alpha = beta = 0.05`. Predeclare `[0, 5]` for larger gains, `[0, 3]` for incremental work or confirmation, or `[-3, 0]` only if the [acceptance policy](../../../references/candidate-rules.md#acceptance) explicitly allows a small strength tradeoff. The upper boundary accepts under that profile; the lower boundary rejects.

Require confirmation if declared in advance, variants were selected using game results, or a concrete unresolved risk or conflicting evidence calls for it. State the question that confirmation will resolve; it is not an automatic longer-time-control step. Use `Smoke` for plumbing, `STC` for a candidate test, and `Confirm` for separately justified confirmation.

## Tuning corpus

Run the pinned baseline as both Dev and Base in a fixed workload of 42,000 games (21,000 color-reversed pairs), with compact PGNs and `4+0.04`. Use the strength book, options, and adjudication above. Ignore the score and fixed-test pass/fail flag; this workload supplies data, not a strength decision.

Use one worker with one match runner. Set **Workload Size** to `ceil(21000 / (2 * worker concurrency))`; the assignment may slightly exceed the target. Multiple assignments or runners in the current OpenBench version overlap opening ranges. Retain workload identity and PGN location. Dataset preparation under [fitting checks](../../tune-latrunculi-evaluation/references/fitting-checks.md) decides whether the games are usable, including after a stopped or incomplete workload.

## Release stability

Before a public release with engine changes, run the pushed candidate as both Dev and Base in a fixed, non-SPRT test with `max_games = 2000` (1,000 pairs) and compact PGNs. Use the strength book, time control, options, and adjudication. Require no crashes, hangs, time losses, illegal moves, protocol failures, or incomplete games. Ignore the score and fixed-test pass/fail flag; they measure score, not stability.

## Evidence and candidate decision

For a terminal test, retain its ID/URL, engine revisions, OpenBench revision, termination rule, games, decision, and server PGN location. For a strength SPRT, also retain profile, terminal LLR, and Elo interval. Running snapshots and ad hoc comparisons do not belong on the development board.

Keep every result tied to the revisions actually tested. When the live candidate or baseline differs, use the [candidate evidence rules](../../../references/candidate-rules.md#evidence-reuse) to assess applicability and record the required local combination checks or fresh games. A recorded reuse decision may support acceptance; unresolved applicability does not advance the candidate. Never relabel an old workload as testing a new revision.
