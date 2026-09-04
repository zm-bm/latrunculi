# Evaluation tuning workflow

This is the repeatable process for tuning Latrunculi's linear handcrafted
evaluation. The [README](README.md) lists the commands.

Each experiment starts from one engine revision and produces at most one
candidate. Grouped cross-validation selects the candidate. A fresh OpenBench
match decides whether it becomes engine source.

## Experiment

Copy `experiment.example.json`, choose a unique name, and record the baseline
revision, benchmark, and corpus. The runner combines this input with its fixed
version-3 policy and writes the complete policy to the output directory.

Review the definition before running it. Once started, the configuration,
engine, tools, dependencies, and PGNs are fixed by hash. A change requires a
new experiment and output directory.

OpenBench owns the source PGNs. The tools receive their paths explicitly and
do not read OpenBench state or storage on their own.

## Corpus and dataset

Generate 42,000 baseline self-play games from 21,000 openings, each played with
colors reversed. Use compact PGNs, `UHO_Lichess_4852_v1.epd`, `4+0.04`,
`Threads=1 Hash=32`, and the
[standard adjudication](../../docs/openbench.md#strength-tests). Strength tests
use `10+0.1`.

Set **Workload Size** to `21,000 / (2 * worker concurrency)`: `875` on the
current 12-thread worker. This keeps all openings in one assignment. Smaller
assignments in the current OpenBench version reuse part of the opening range.

The runner requires 40,000 valid games and 20,000 retained opening groups. It:

1. Samples at most six fixed quantiles from each game after ply 8.
2. Runs `latrunculi features --settle`. Stateless quiescence search follows
   captures and check evasions; terminal positions are discarded.
3. Deduplicates settled four-field FENs. It keeps the first source when results
   agree and drops every occurrence when they conflict.
4. Writes every retained record to `development.jsonl`.

An explicit starting FEN identifies an opening across archives. A game without
one receives its own group. Each group has total weight one, divided among its
retained positions.

The manifest records input hashes, filtering, results, phases, and duplicates.
Every position must reconstruct the engine evaluation exactly.

## Cross-validation and fit

The runner assigns every opening group to one of five deterministic folds.
Groups never cross folds. A variable is fitted only when its tied feature set
appears in at least 128 groups in every fold's training complement.

For each excluded fold, the runner:

1. Calibrates the Texel scale on the other four folds.
2. Fits all eligible middlegame and endgame coefficients together with
   L-BFGS-B.
3. Tries penalties `1e-9`, `3e-9`, `1e-8`, `3e-8`, `1e-7`, `3e-7`, `1e-6`,
   `3e-6`, and `1e-5`.
4. Rounds every optimizer checkpoint under the final constraints and retains
   its lowest exact training loss plus penalty. The unchanged baseline remains
   eligible.
5. Scores each retained candidate only on its excluded fold.

The fixed constraints are:

- `material.pawn.mg` is fixed.
- Each declared PSQT and mobility reference is fixed.
- Non-pawn PSQTs retain their file mirrors.
- A coefficient moves by at most 100 centipawns.
- Material values remain within their piece-specific ranges.

The penalty counts every underlying coefficient, including both sides of a
tie. Nonlinear king danger, phase and scaling formulas, tempo, and search
parameters are outside the fit.

A penalty is eligible only when its pooled out-of-fold comparison has a
positive 90% lower confidence bound and no supported phase bucket has a
negative upper bound. Reports use 2,000 fixed-seed, opening-group bootstrap
samples and baseline phase buckets `0-31`, `32-63`, `64-95`, and `96-128`.

The runner selects the strongest eligible penalty within one standard error of
the best mean fold improvement. It then recalibrates and refits that penalty on
the complete dataset. If none pass, it retains the baseline.

`candidate.json` records the exact integer weights, support, large changes, and
bound hits. The last two invite review; they are not automatic rejection rules.

## Strength decision

For a supported candidate, apply the weights to `src/eval/parameters.hpp`,
build the engine, and run `verify`. Verification compares every compiled
coefficient and evaluation invariant with the candidate artifact.

Review, commit, and push the verified patch before starting OpenBench. Run one
normalized-Elo `[0, 3]` SPRT against the pinned baseline with the
[standard settings](../../docs/openbench.md#strength-tests). Let it reach a
boundary or its game limit.

- The upper boundary accepts the candidate.
- The lower boundary or an inconclusive limit retains the baseline.
- `offline` records a rejection before match play.

Use `close` to append the decision and OpenBench test ID to tracked
`results.jsonl`. Generated output under `tools/tuning/output/<experiment>/` may
then be deleted.

Fresh match games are the independent acceptance test. An untouched external
corpus may be used for an occasional release audit, but it is not part of the
routine tuning command.

Other development may continue while fitting, but evaluation, parameter, or
feature-schema changes invalidate the experiment. Intervening search changes
require a fresh comparison against the current baseline. Unrelated changes
need normal verification.

Background: [Automated Tuning](https://www.chessprogramming.org/Automated_Tuning),
[Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method),
and [Ethereal's tuning paper](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf).
