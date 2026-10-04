# Engine Development

The current work board for improving Latrunculi. [Engine testing](engine-testing.md)
explains what evidence lets an item advance.

Keep one live entry per experiment and move it between stages. A lead names its
observation, baseline evidence, and next test. A candidate adds its claim, branch,
base and candidate revisions, evidence, and next action. Preserve IDs; number
retained candidates and unresolved leads, but leave casual nulls unnumbered.
For each bounded attempt, record its baseline, tested scope, outcome, and why
it stopped; keep live evidence and useful terminal findings. Run one local
CPU-sensitive task and one OpenBench test at a time.

## Baseline

| Field | Current value |
|---|---|
| Engine | `d3116b1` (ENG-034: early child TT prefetch) |
| OpenBench fingerprint | 3,507,960 nodes |
| Search corpus | `tools/analysis/output/search-baseline-d3116b1/`; 200 positions, depth 10, one thread, 32 MiB Hash; 54,039,403 nodes |
| Build | `release-dev`, GCC 15.2.0, x86-64, `gazelle`; refreshed 2026-10-04 |

Source revision: `d3116b1b5d46026055509b1da12dbdb3c066ff2f`.
The two fresh-process corpus runs exactly match the approved candidate's search
signatures; the benchmark fingerprint also matches. Complete Release and
ASan/UBSan suites passed.
Historical experiments retain the revisions that produced their evidence.

## Issues and leads

### ENG-032 — Explain persistent quiet-move disagreements

- **Question:** does the baseline under-search alternative quiet root moves, or
  does the score disagreement persist after focused work?
- **Evidence:** on `ccb718b`, development anchor `deb4939117f9cebe` chooses h3 at 50k, 500k,
  and 5M nodes. One complete reference MultiPV report ranks d4 above h3 by 125 cp. Disabling
  all internal LMR repaired none of the eight development or two confirmation quiet targets
  at 500k nodes; this leaves targeted selectivity and evaluation unresolved.
- **Next:** separately restrict the root to `d2d4` and `h2h3` at equal node budgets,
  preserving the anchor's starting FEN and history. Compare scores and continuations to distinguish
  a ranking changed by focused work from an unresolved score difference, then check whether the
  cause recurs in other development groups.

Pilot report and raw archive: `tools/analysis/output/diagnostic-pilot-ccb718b/`.
Its inspected confirmation groups cannot serve as a fresh holdout for a later
candidate; use new group-disjoint confirmation. This lead replaces ENG-020's old
diagnostic plan; other pruning-family interventions remain untested.

### ENG-033 — Compare TT cluster layouts at a fixed memory budget

- **Question:** do shorter scans or greater retention improve search efficiency?
  Compare the current four 16-byte entries per 64-byte cluster with two variants:
  two existing entries per aligned 32-byte cluster (same capacity, shorter scans),
  and five entries per aligned 64-byte cluster (25% more capacity). The dense
  layout uses separate arrays of five atomic 64-bit payloads and five atomic
  32-bit verification signatures, plus four padding bytes.
- **Evidence:** TT probes account for 17.29% of sampled cycles, concentrated around
  the payload load. The 200-position depth-10 profile at `cc5a972` has unchanged
  engine code and exact signatures against baseline `7cb8603`; artifacts:
  `tools/analysis/output/profile-baseline-verified/`. Capacity pressure is unproven.
- **Next:** compare only these three layouts at 32 MiB on the standard depth-10
  corpus, using the current baseline and a refreshed profile. Keep payload fields,
  replacement scoring, aging, prefetch placement, and search policy fixed. Measure
  scan lengths, replacements, and useful hits separately for main search and quiescence; screen nodes and
  paired time without instrumentation. Assess each complete layout, since density
  also changes signature width and storage arrangement.
- **Risk:** the dense layout weakens collision verification. Specify its key/payload
  binding and publication ordering; check full keys diagnostically and test
  collisions and concurrent snapshots. Stop a variant whose tradeoff is unjustified.
- **Outcome:** retain one promising candidate, preserve a second credible contender
  for follow-up, or record a null/unresolved result. Both layouts are tree-changing;
  local screening selects what merits offline testing and games, not a strength winner.

References: CPW's [buckets](https://chessprogramming.org/Transposition_Table#Bucket_Systems),
[collisions](https://chessprogramming.org/Transposition_Table#Collisions), and
[shared-table verification](https://chessprogramming.org/Shared_Hash_Table#Xor).

## Candidates for local testing

None.

## Ready for OpenBench

None.

## OpenBench tests

None.

## Ready for integration

None.

## Recent results

- **ENG-034:** early child TT prefetch integrated as `d3116b1` from approved
  candidate `466ecd4`. Tree-preserving: corpus and fingerprint signatures match;
  Release and ASan/UBSan suites passed. Six paired runs gave about 2.1% faster
  search (`R_time_balanced = 0.9794`), with the final balanced block effectively
  flat. Cleanup preserved machine code and runtime data; evidence remains in
  `tools/analysis/output/eng-034-offline/` and `tools/analysis/output/eng-034-cleanup/`.
- **ENG-010 — Retired by owner:** depth-4 main-search futility at 1350 cp,
  candidate `4ab957b` on `ccb718b`, was not worth continuing the long test.
  OpenBench #32 (`/test/32/`, PGN `/api/pgns/32/`) stopped inconclusive after
  49,180 games; it did not reach an SPRT rejection. No integration.
  An earlier 700 cp variant on `ccb718b` skipped nine verified beta cutoffs,
  despite reducing nodes; that specific pruning rule was rejected.
- **ENG-031:** connected-pawn links `{MG 13, EG 3}` integrated as `ccb718b` after OpenBench #31 accepted.
- **Capture ordering and picker reuse (earlier prototypes):** CaptureHistory
  reduced nodes but increased elapsed time; late losing-capture pruning slowed
  search by 4.1%. Score/history reuse also failed to give repeatable speed gains,
  with one variant trading fewer instructions for lower IPC and more branch misses.
  These outcomes apply to those implementations, not the whole optimization area.
- **Qsearch capture pruning (earlier prototype):**
  `stand_pat + captured_value + margin <= alpha_before_move` at margins
  200/300/400 skipped real NonPV cutoffs. Any replacement needs evidence that its
  discriminator avoids those lost opportunities.
- **Node accounting (earlier prototypes):** replacing locked per-node increments
  with relaxed single-writer loads/stores was 2.17% slower; worker-local counts
  with periodic publication were neutral. Removing synchronization instructions
  alone did not deliver a speed gain.
