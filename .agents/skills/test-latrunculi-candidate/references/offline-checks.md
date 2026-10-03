# Offline checks and timing

Use this policy for one recorded candidate commit against the current operational baseline. A change is **tree-preserving** only when its code or build cannot change search decisions and its corpus and fingerprint signatures exactly match the baseline. Treat every other change as **tree-changing**.

## First checks

The standard corpus is all 200 Arasan positions in `tools/analysis/search.epd`. Search them cold at depth 10 with one thread, 32 MiB Hash, and one process pass. The OpenBench compatibility fingerprint uses six positions at depth 13.

An optional local preflight may search the corpus at depth 8; all 200 positions must complete. It does not replace the depth-10 pass.

| Change | Start with |
|---|---|
| Tree-changing | Check the proposed mechanism and objective, then complete one corpus pass in a fresh process and a fingerprint. |
| Tree-preserving | Establish the code- or build-level equivalence argument and verify exact baseline corpus and fingerprint signatures. |

After these first checks, finish correctness and repeatability checks, run the complete Release suite, and add applicable risk and sanitizer checks. For a tree-changing candidate, rerun the fingerprint to confirm the first result. Run paired corpus timing only after these checks pass. Run the candidate corpus twice in fresh processes; the first timing run may count as one. A subset or exploratory run cannot replace the complete corpus. A fitted evaluation also needs grouped fitting evidence and verification of its compiled weights and schema.

## Repeatability and correctness

Repeat deterministic depth- or node-limited searches with one thread, each in a fresh process. Runs of the same build must agree on completed depth, static and searched scores, actual nodes, best move, and principal variation (PV). Time and NPS may differ. A tree-changing candidate may differ from the baseline without failing this same-build check.

For mate or material correctness cases, record the source and expected solution before testing. Illegal moves, crashes, or a lost or delayed recorded solution fail correctness. If PV and non-principal-variation (NonPV) searches of the same build disagree, hold the offline pass until the discrepancy is understood; it does not by itself prove incorrect chess.

Main-search futility must preserve checking quiet moves. Time-control changes must honor explicit `movetime` requests.

## Extra risks

The cold corpus can miss faults in state reused across searches, clock or limit handling, shared state, and worker lifecycle. Add focused checks when a change affects those areas. Merely reading history, the transposition table (TT), or a clock does not call for extra checks. Record the risk, initial sequence, pass condition, repetitions, and sanitizer need; use the smallest adequate existing test.

## Paired timing

Build both revisions with the same compiler and preset. On an otherwise idle machine, collect six baseline/candidate pairs, alternating run order `BC`, then `CB` in three blocks. Use a fresh process for each run. `R_time_balanced` is the median of the geometric means of adjacent `BC,CB` candidate/baseline time ratios. The [analysis guide](../../../../tools/analysis/README.md#paired-timing) gives the command.

Use these as decision targets, not automatic rejection rules:

- Smaller fixed-depth tree claim: `R_node_g <= 0.9900`.
- Speed claim: `R_time_balanced <= 0.9925`.
- Strength claim: `R_time_balanced <= 1.0100` is a slowdown warning.

A missed target needs an explanation of the measured tradeoff and whether the evidence justifies continuing. Do not claim a speed gain without repeatable timing evidence. If a small result is sensitive to noise, repeat under steadier conditions, such as a warm-up per binary and a fixed physical core, before deciding. A single run, old baseline timing compared with a fresh candidate run, an incomplete run, or unisolated timing is diagnostic evidence only.

## Decision

A candidate fails if it violates a required check or is nondeterministic. Leave the result unresolved when evidence is missing, collection fails, or a required condition is unavailable. Repeat an unchanged measurement only after identifying a setup or collection problem. Report no more precision than run-to-run variation supports.
