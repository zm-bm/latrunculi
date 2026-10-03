# Engine Testing

An engine change starts as an idea, becomes a recorded candidate, and passes
offline testing. If it changes search decisions, it also needs an OpenBench
strength test.

The [development board](engine-development.md) shows the current baseline and
each experiment's stage. This guide explains what the tests help decide.

## Evidence and exploration

Start with a question about the current baseline. For a proposed change, state
what it should improve and how you will tell whether it worked. A profiler can
show where the engine spends CPU time; fixed-depth node counts show how a
change affects the search tree. Paired timing measures speed, while games
measure playing strength. A promising position or reference-engine agreement
alone cannot establish a gain.

Use development positions to choose a variant, then test it on separate
confirmation positions without retuning it. Keep positions from the same game
or opening together so related examples cannot appear on both sides.

## Candidate revisions

A candidate is a specific engine commit based on the current operational
baseline. Its branch and commit identity tie every measurement to the code
that produced it. A fix or rebase creates a new revision that needs fresh
evidence for anything it may affect.

## Offline testing

Offline testing checks the proposed mechanism, correctness, repeatability, and
speed before games or integration. The standard search corpus and OpenBench
fingerprint make results comparable across revisions.

| Change | Meaning | After an offline pass |
|---|---|---|
| **Tree-preserving** | Search decisions cannot change, and corpus and fingerprint signatures match the baseline. | May go straight to integration. |
| **Tree-changing** | Any other change. | Needs OpenBench strength tests. |

The [analysis guide](../tools/analysis/README.md) gives benchmark and
comparison commands.

## Paired timing

Timing alternates baseline and candidate runs on the same machine to limit
the effect of machine-speed drift. The comparison tool reports
`R_time_balanced`, a candidate-to-baseline search-time ratio; lower is faster.
The [analysis guide](../tools/analysis/README.md#paired-timing) shows how to
collect and compare runs.

## Game acceptance

A tree-changing candidate that passed offline testing against the current
baseline needs [OpenBench strength tests](openbench.md#strength-tests) before
integration. OpenBench compares the candidate with the pre-change revision
through color-reversed games and a predeclared statistical test. A result
reached on a different baseline does not decide the live candidate.

## Outcomes and integration

The board distinguishes leads, candidates needing local tests, pending games,
and revisions ready for integration. Keep incomplete or inconclusive work in
its stage with the next action. Integrate only the explicitly approved
revision after its offline pass and any required game acceptance. The
integrated build must match the retained candidate's search
signatures before it becomes the new baseline.
