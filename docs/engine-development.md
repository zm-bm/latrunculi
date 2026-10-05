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

Suggested TT order: ENG-035, ENG-036, then ENG-033; ENG-037/038 are lower-priority
follow-ups. The TT audit at `d3116b1`, including reference-engine revisions and
reproductions, is in `tools/analysis/output/tt-design-audit-d3116b1/report.md`.

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

### ENG-033 — Compare TT layouts and miss-rejection cost at a fixed memory budget

- **Question:** do shorter scans, cheaper miss rejection, or greater retention
  improve search efficiency? Starting proposals against the current four 16-byte
  entries per 64-byte cluster are two existing entries per aligned 32-byte cluster
  (same capacity, shorter scans), and five entries per aligned 64-byte cluster
  (25% more capacity; separate atomic 64-bit payload and 32-bit signature arrays).
  Consider independent rejection tags alongside these proposals; choose at most
  two concrete alternatives after reviewing their storage and verification costs.
- **Evidence:** TT probes account for 17.29% of sampled cycles, concentrated around
  the payload load. The 200-position depth-10 profile at `cc5a972` has unchanged
  engine code and exact signatures against baseline `7cb8603`; artifacts:
  `tools/analysis/output/profile-baseline-verified/`. Capacity pressure is unproven.
  The `d3116b1` audit confirms payload-dependent XOR checks with two signature
  loads; Release code already defers most decoding until a match. This absorbs
  ENG-005's independent-tag lead.
- **Next:** compare the selected alternatives with the current baseline at an equal
  actual 32 MiB table budget on the standard depth-10 corpus, with a refreshed
  profile. Keep payload fields, replacement scoring and store-time slot selection,
  aging, prefetch placement, and search policy fixed. Measure entries examined,
  payload loads, replacements, and useful hits separately for main search and
  quiescence; screen nodes and fresh paired baseline/candidate time without
  instrumentation. Assess each complete design, including capacity and verification.
- **Risk:** shorter signatures weaken collision verification. Specify key/payload
  binding and publication ordering; an early rejection tag must not bypass final
  snapshot validation. Check full keys diagnostically and test collisions and
  concurrent different-key writers. Stop a variant whose tradeoff is unjustified.
- **Outcome:** retain one promising candidate, preserve a second credible contender
  for follow-up, or record a null/unresolved result. Capacity/cluster changes are
  tree-changing; establish whether a rejection-only variant preserves signatures.
  Local screening selects what merits offline testing and games, not a strength winner.

References: CPW's [buckets](https://chessprogramming.org/Transposition_Table#Bucket_Systems),
[collisions](https://chessprogramming.org/Transposition_Table#Collisions), and
[shared-table verification](https://chessprogramming.org/Shared_Hash_Table#Xor).

### ENG-035 — Make TT reuse account for the halfmove clock

- **Defect:** keys omit the halfmove clock, allowing a TT cutoff to reuse a score
  from before a fifty-move draw became imminent. Checking the current position
  for a draw before probing does not prevent this.
- **Evidence:** at `d3116b1`, Threads=1, Hash=32 MiB, depth=6,
  `7k/8/8/8/8/8/6Q1/6K1 w - - 98 1` scores 0 cp with an empty TT, but +1950 cp
  after searching the same board with clock 0. Clearing only the TT restores 0 cp
  in all three repetitions. Legal-move enumeration confirms the expected draw;
  the audit retains `rule50-probe.py`, raw results, and the enumeration. Practical
  frequency and strength impact are unmeasured.
- **Next:** preserve this as a regression and investigate a targeted clock-aware
  key or cutoff policy. Cover main search, qsearch, low/high-clock reuse in both
  directions, and mates near the limit. Keep layout and other search policy fixed;
  avoid a general draw-handling rewrite.

### ENG-036 — Test huge-page backing for the TT

- **Question:** can huge-page allocation reduce TT address-translation cost while
  preserving the search tree?
- **Evidence:** the `d3116b1` audit found 4 KiB pages and no huge-page backing for
  fresh 32/64 MiB tables on `gazelle` with THP in `madvise` mode. The allocator
  makes no explicit huge-page request; several reference engines do. A speedup
  and TLB pressure have not been established.
- **Next:** try suitable allocation alignment and huge-page advice with a normal
  allocation fallback. Verify actual backing before fresh paired timings against
  the current baseline at the same table capacity; check exact search signatures.
  Hold layout, replacement, and search policy fixed. Exercise resize, clear, and
  fallback behavior; do not require system-wide huge-page settings.

### ENG-037 — Investigate TT aging and same-key retention

- **Question:** does retaining old deep entries over newer shallow results reduce
  useful TT reuse across searches?
- **Evidence:** at `d3116b1`, age refreshes only on successful stores, and shallow
  non-exact same-key writes can be rejected regardless of entry age. References
  vary: some refresh on hits or permit aged same-key replacement. No weakness in
  the current policy has been measured.
- **Next:** measure entry age, rejected updates, and useful hits over repeatable
  search sequences that retain the TT. Use the findings to select one change to
  age refresh or same-key replacement, holding layout and other policy fixed.
  Compare fresh baseline/candidate sequences and the standard corpus; cold-table
  runs alone cannot establish the aging benefit. Treat policy changes as tree-changing.

### ENG-038 — Investigate evaluation-only TT records

- **Question:** can caching static evaluation without a searched bound save enough
  evaluation work to repay extra TT traffic and displacement?
- **Evidence:** at `d3116b1`, `TTBound::None` is invalid, so the table cannot retain
  evaluation-only records. Stockfish and Ethereal support them; their benefit to
  Latrunculi is unmeasured.
- **Next:** measure repeated evaluations and select one insertion site for a
  bounded prototype. Evaluation-only hits must never authorize score cutoffs.
  Keep entry size, cluster layout, and evaluation values fixed; account for saved
  evaluations, displaced useful bounds, nodes, and fresh paired elapsed time.
  Changed table occupancy can change the search tree and requires strength testing.

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
