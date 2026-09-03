# Evaluation tuning workflow

This is the repeatable process for tuning Latrunculi's linear handcrafted
evaluation. The [README](README.md) lists the commands.

An experiment starts from one engine revision and may nominate one candidate.
Offline loss decides whether that candidate deserves a match. OpenBench decides
whether it becomes engine source.

## Experiment record

Copy `experiment.example.json`, choose a unique name, and record the baseline
revision, benchmark, and corpus details. The runner combines this small input
with its fixed version-1 tuning policy and stores the complete resolved policy
in the experiment output.

Commit this definition before fitting. Once a run starts, its configuration,
engine, scripts, and PGN hashes are fixed. A change creates a new experiment.
Generated state is kept in `tools/tuning/output/<experiment>/`. Closing an
experiment adds its identity, candidate, decision, and OpenBench test ID to the
tracked `results.jsonl` ledger. After any planned held-out check, the ignored
output may be deleted.

OpenBench owns the source PGNs. The tools receive their paths explicitly and
do not read its database, settings, or storage directories on their own.

## Dataset

Create a fixed, non-SPRT OpenBench workload for 42,000 games: 21,000 openings,
each played twice with colors reversed. Use the pinned baseline on both sides,
compact PGNs, `UHO_Lichess_4852_v1.epd`, `4+0.04`, `Threads=1 Hash=32`, and the
[standard adjudication](../../docs/openbench.md#strength-tests). This shorter
time control keeps corpus generation practical; strength tests still use
`10+0.1`. The extra 2,000 games leave room for filtering. The runner requires
40,000 valid games and 20,000 retained opening groups.

Set **Workload Size** to `21,000 / (2 * worker concurrency)`: `875` on the
current 12-thread worker. This keeps the 42,000 games in one assignment.
Smaller assignments in the current OpenBench version reuse half their opening
range and do not produce enough distinct groups.

Independent openings matter more than extra positions from the same games. If
many features lack support, collect more games before sampling more positions
from each game.

The builder performs these steps in order:

1. From each game, choose no more than six positions from ply 8 onward. The choices
   are fixed quantiles spread across the remaining game.
2. The builder invokes `latrunculi features --settle`. A stateless one-worker
   quiescence search follows captures and check evasions to a stable position;
   terminal positions are discarded. Plain `latrunculi features` does not
   settle its input.
3. Remove duplicates by the settled four-field FEN. Keep the lexicographically
   first source when results agree; discard every copy when results conflict.
4. Assign complete opening groups to fixed 80/5/5/10 training, selection,
   validation, and held-out splits.

A settled position keeps its source game's result. Do not mix positions settled
by a different method in the same experiment.

An explicit starting FEN identifies an opening across archives. A game without
one forms its own group. Such games remain usable, but the corpus should include
explicit FEN headers so color-reversed games share a group.

Each opening group receives total weight one, divided equally among its
positions. This prevents a long game or repeated opening from dominating the
fit. Feature support is also counted by distinct groups. A coefficient seen in
fewer than 128 training groups is frozen.

The manifest records inputs, hashes, filtering counts, WDL counts, phase
counts, duplicates, conflicts, and split integrity. The calibration artifact
records feature support. Every record must reconstruct the engine's exported
evaluation exactly.

## Joint linear fit

First calibrate the Texel scale from group-weighted training loss. The scale
maps an evaluation to an expected game result and remains fixed for the whole
experiment. The run stops if the optimum reaches the calibration search bound.
The fitting objective is group-weighted mean squared error.

Fit every exported middlegame and endgame coefficient together. Keep these
constraints:

- `material.pawn.mg` is fixed.
- The declared PSQT and mobility reference values are fixed.
- Knight, bishop, rook, queen, and king PSQTs retain their file mirrors.
- Every change is limited to 100 centipawns in either direction.
- Material values also remain inside their declared piece-specific ranges.

The nonlinear king-danger result, phase formula, endgame scaling, tempo, and
search parameters are not part of this fit.

Run independent L-BFGS-B fits with penalties `1e-9`, `1e-8`, and `1e-7`.
Penalties apply to every changed coefficient, including both coefficients in a
tie. Training data drives optimization. Each checkpoint is rounded under the
final constraints and scored with exact engine arithmetic. Selection loss
chooses the checkpoint and penalty. The unchanged baseline is always a valid
result. Validation is not examined until that choice is final.

## Offline decision

Compare the selected integer candidate with its parent on the independent
validation split, using 2,000 fixed-seed opening-group bootstrap samples and a
90% confidence interval. A candidate is qualified only when:

- the lower bound of its overall prediction improvement is above zero; and
- no phase bucket with at least 128 groups has an upper improvement bound below
  zero.

The phase buckets are `0-31`, `32-63`, `64-95`, and `96-128`, assigned using
the parent evaluation. Also review sparse features, changes using at least 80%
of their allowed range, and weights on a bound.

This result is qualification for a match, not evidence of playing strength.
The validation gate is used once. A failure closes the experiment; do not tune
another candidate against the same validation split. If either validation or a
match result changes the next fit, start again with fresh games.

## Strength decision

Apply the qualified weights, build the engine, and use `tune.py verify` to
compare every exported value with the candidate artifact. Review, commit, and
push the patch before starting OpenBench.

Run one normalized-Elo `[0, 5]` SPRT against the pinned baseline with the
[standard settings](../../docs/openbench.md#strength-tests). Let the test reach
an SPRT boundary or its maximum game count. Do not decide from an intermediate
win/loss record.

- The upper boundary accepts the candidate.
- The lower boundary or an inconclusive maximum retains the baseline.
- Either result closes the experiment.

Record the boundary and OpenBench test ID with `tune.py close`. Use its
`offline` result when no match was run. The command derives acceptance or
rejection from that result.

## Held-out data

Routine fitting and validation never load held-out records. After an experiment
is closed, `tune.py reveal` scores the retained engine on them. Use this for a
release or another major evaluation change, not for ordinary candidate
selection. The command requires the tuner and dependency versions recorded by
the experiment.

Once viewed, held-out data is no longer blind. If its result affects another
fit, the next release check needs fresh held-out data.

## Parallel work

Other development may continue while an experiment runs, but the fit remains
tied to its original revision.

- Evaluation, parameter, or feature-schema changes invalidate it.
- Search changes require a new match against the current baseline before the
  candidate can be accepted.
- Unrelated changes require the normal tests and compiled-weight verification.

Nonlinear evaluation or search tuning may reuse this lifecycle, but not this
linear fitting method.

Background: [Automated Tuning](https://www.chessprogramming.org/Automated_Tuning),
[Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method),
and [Ethereal's Evaluation & Tuning in Chess Engines](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf).
