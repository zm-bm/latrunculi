# Search Stability Notes

This note captures the current search-stability findings around the start
position, hash-size sensitivity, LMR behavior, and opening eval shape. It is a
working investigation note, not a finalized tuning plan.

## Trigger

The motivating symptom was a fixed-depth search from the start position where
`go depth 13` selected `e2e3`:

```text
info depth 13 score cp 34 nodes 859259 time 596 nps 1441709 pv e2e3 ...
bestmove e2e3
```

`e2e3` is legal and playable, but it should not be the preferred start-position
move at this depth unless search finds something concrete. The concern is not
that the move loses by force; the concern is that it beat more principled root
moves such as `e2e4` and `d2d4` in a shallow but already nontrivial search.

## Current Evidence

The static one-ply eval did not appear to be the direct cause. From the starting
position, the eval already preferred central moves over `e2e3`:

- `e2e4` was roughly `+0.86`.
- `d2d4` was roughly `+0.82`.
- `e2e3` was roughly `+0.78`.
- `b1c3` was roughly `+0.59`.

That gap is small, but the ordering is not inverted. The bad root choice looked
more like a search interaction than a pure static-eval preference.

Temporary diagnostics pointed at accumulated iterative-deepening state:

- A fresh full-window child search for `e2e4` at depth 13 scored well.
- After depths 1 through 12 populated TT and quiet-history state, the same
  `e2e4` child score dropped sharply.
- Clearing ordering state after the prefix restored most of the score.
- Clearing both ordering state and TT restored the fresh-search score.

That implicated the interaction between TT, move ordering, history, PVS, and
LMR rather than a single isolated eval term.

## Hash-Size Sweep

With the current LMR threshold change in place, a start-position depth-15 sweep
using fresh latrunculi processes per hash size showed substantial node-count
variation:

| Hash | Score | Nodes | Time | Best |
| ---: | ---: | ---: | ---: | --- |
| 1 MB | `+46` | 8,486,252 | 5,835 ms | `e2e4` |
| 4 MB | `+39` | 4,214,025 | 2,926 ms | `d2d4` |
| 16 MB | `+41` | 9,100,838 | 6,659 ms | `e2e4` |
| 32 MB | `+53` | 4,571,974 | 3,321 ms | `e2e4` |
| 64 MB | `+58` | 8,084,715 | 5,907 ms | `e2e4` |
| 128 MB | `+59` | 8,736,648 | 6,565 ms | `e2e4` |
| 256 MB | `+58` | 8,825,304 | 6,705 ms | `e2e4` |

Node-count variation across hash sizes is expected. TT entries affect hash
moves and bounds; those affect ordering and cutoffs; those affect PVS scout
results; and that changes whether LMR reductions are accepted or re-searched.
There is no requirement that fixed-depth node counts be monotonic with hash
size.

The size of the swing is still a useful signal. The `4M -> 9M -> 4M -> 8M`
pattern suggests that latrunculi is more sensitive to TT retention than a mature
engine should be.

NPS decreased almost monotonically as hash size grew. That is not surprising:
small hash tables are hotter in cache, while larger tables put more pressure on
cache and TLB locality. More hash is usually better for search quality and
retention, but not necessarily faster per node.

## Stockfish Comparison

For comparison, the installed Stockfish
`dev-20230606-54ad9867` was run with the same setup: fresh process per hash,
`Threads=1`, start position, and `go depth 15`.

| Hash | Score | Nodes | Time | Hashfull | Best |
| ---: | ---: | ---: | ---: | ---: | --- |
| 1 MB | `+35` | 107,391 | 132 ms | 642 | `e2e4` |
| 4 MB | `+20` | 189,590 | 196 ms | 302 | `e2e4` |
| 16 MB | `+20` | 146,653 | 153 ms | 58 | `e2e4` |
| 32 MB | `+20` | 146,653 | 154 ms | 27 | `e2e4` |
| 64 MB | `+20` | 146,653 | 158 ms | 11 | `e2e4` |
| 128 MB | `+20` | 146,653 | 158 ms | 5 | `e2e4` |
| 256 MB | `+20` | 146,653 | 159 ms | 2 | `e2e4` |

Stockfish is not hash-insensitive. It changed tree shape at tiny hash sizes.
However, from 16 MB upward this run produced identical nodes, score, root move,
and PV. That makes latrunculi's hash-size sensitivity look meaningfully noisy,
not just a universal property of fixed-depth alpha-beta search.

## Working Issues

### Hash And Search Node Count Behavior

The current hash-size behavior is not a correctness failure by itself, but it is
worth measuring across a small position suite. A large node-count swing can be
fine if the root move and score stay sensible; it becomes more concerning if
small TT changes repeatedly flip the root result or make the engine favor
positionally weak moves.

The main areas to inspect are:

- TT depth and bound validation.
- Mate-score adjustment on store and probe.
- Replacement policy and generation aging.
- Hash move legality and pseudo-legality checks.
- How strongly the move picker trusts hash moves in reduced searches.
- Whether lower/upper bounds are used too eagerly at PV or near-PV nodes.

### LMR-Induced Instability

LMR was the strongest search-side suspect in the `e2e3` case. Disabling LMR
made the start-position root result sensible, but the node cost was too high.
Raising the quiet-move threshold from the third legal move to the fourth legal
move fixed the immediate symptom with much lower cost.

That change should be treated as a stabilizer, not proof that LMR is now tuned.
The larger issue is that reduced quiet moves can become self-reinforcing when
history and TT state push a good root alternative late enough that it only gets
a reduced scout. If the reduced search does not exceed alpha, the move may never
receive a full verification search.

Good follow-up measurements:

- LMR attempts and re-searches by ply.
- Reduced quiet moves that later become PV moves in neighboring hash sizes.
- Root moves whose full-window score differs sharply from their reduced scout.
- Beta cutoffs by move index before and after LMR threshold changes.

### Eval Opening Shape

Eval also needs work. Even if search is the primary cause of the `e2e3` result,
the static gaps around opening moves are probably too small. From startpos,
`e2e3` should trail `e2e4` and `d2d4` by enough that shallow search needs a real
tactical or structural reason to prefer it.

Likely areas to inspect:

- Central pawn space and occupation.
- Development and minor-piece activity.
- Blocked bishop penalties, especially quiet pawn moves that restrict a
  natural development square.
- Tempo and opening-phase scaling.
- Early queen or bishop activity terms that may accidentally compensate for
  passive pawn structure.

Eval should not be used to hide search instability, but it should give the
search a better prior in near-equal opening positions.

### Root Move Verification

The root search behavior deserves separate attention. In the failing case,
`e2e3` stayed in front partly because later root moves only tied or failed to
exceed alpha. Near-equal root moves are common in the opening, so root handling
should be robust around ties and narrow margins.

Questions to answer:

- Should root moves within a small margin get a full-window verification search
  after a reduced or scout path?
- Is strict `value > alpha` correct everywhere at root, or do tie cases need
  clearer deterministic policy?
- Are accepted-depth root results hiding useful instability from failed
  aspiration attempts?

### History Feedback

Quiet history contributed to the observed instability. History is useful only if
it improves ordering without turning previous shallow conclusions into hard
search commitments.

Follow-up areas:

- Whether quiet-history bonuses and maluses are balanced.
- Whether updates are too global for opening positions.
- Whether root moves should receive different treatment from interior cutoffs.
- Whether continuation or countermove history is over-weighted relative to
  direct quiet history.

## Suggested Investigation Order

1. Verify determinism first. Run repeated fresh-process searches with identical
   hash size, thread count, position, and depth. Same-hash node differences
   would indicate a real determinism or initialization bug.
2. Run a small fixed-depth suite across hash sizes. Startpos alone is useful as
   a symptom, but it is too easy to overfit.
3. Add or use search stats for TT hit/cutoff counts, hash move usage, LMR
   attempts, LMR re-searches, and beta cutoffs by move index.
4. Inspect TT/root/LMR interactions before changing eval constants.
5. Tune opening eval only after the search-side instability is understood well
   enough that eval changes are not masking a broken search path.

## Current Read

The `e2e3` result was probably a search instability exposed by near-equal
opening eval, not a pure eval bug. The current LMR threshold change removes the
worst symptom, but the hash-size sweep shows that latrunculi is still more
sensitive to TT retention than Stockfish in this start-position test. The next
benchmark run should measure whether that sensitivity is isolated to startpos or
shows up suite-wide.
