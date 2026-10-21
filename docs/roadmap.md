# Engine Roadmap

Possible improvements and investigations, grouped by area rather than priority.

## Search

### Reduce LMR verification cost

Investigate whether some late-move-reduction fail-highs can be verified at a
shallower depth without accepting false cutoffs or causing more re-searches later.

### Extend quiescence capture pruning

Explore additional pruning beyond the existing SEE filter, accounting for capture
safety and potential gain beyond the captured piece's value. Reduce search work
without discarding tactical refutations.

### Add singular extensions

Explore a one-ply extension when excluding a reliable transposition-table move
shows that all alternatives are substantially worse. Assess whether the extra
search improves play enough to cover the verification cost.

## Evaluation and endgames

### Syzygy tablebases

Add optional WDL/DTZ probing using user-supplied tablebase files.

### Diagnose endgame errors with tablebases

Compare engine choices with external tablebase WDL/DTZ results to identify
recurring errors in low-material positions, accounting for the fifty-move rule.

### Recognize candidate passed pawns

Explore a bonus for pawns that become passed after another friendly pawn
exchanges away their only enemy blocker. Distinguish feasible breakthroughs
from unsupported pawns or multiple blockers.

### Evaluate rooks behind passed pawns

Explore evaluation terms for friendly and enemy rooks behind passed pawns,
accounting for intervening pieces and whether the rook supports or restrains
advancement.

### Add a KPK bitbase

Add a compact built-in bitbase for king-and-pawn versus king endings, with
correct side-to-move, promotion, and fifty-move handling.

### Recognize wrong-bishop rook-pawn draws

Recognize drawn king, bishop, and rook-pawn versus king positions using king
placement, bishop color, side to move, and promotion threats.

### Improve evaluation of drawish minor-piece endings

Investigate scaling for minor-piece endings with blocked pawns or few pawns on
one flank. Treat bishop and knight endings separately, account for other draw
effects, and preserve winning positions.

### NNUE

Explore a neural evaluation backend while retaining the handcrafted evaluation.

## Performance and parallelism

### Cache pawn and king evaluation

Cache reusable pawn-structure and king-shelter terms per worker. Include all
relevant pawn, king, and castling state in the key and preserve exact scores.

### Reduce SEE cost and unnecessary calls

Reduce static exchange evaluation (SEE) work in capture ordering:

- Avoid rescoring the already-searched TT move and update only slider rays that
  can change after each exchange. Preserve scores and move order, including ties.
- Try cheap capture ordering with thresholded SEE as each move is selected,
  rather than calculating numerical SEE scores for every capture up front. Use
  material bounds to resolve easy cases without generating attackers. Initially
  retain the zero threshold and existing promotion and TT handling.
- Try cheap victim-based ordering for capture evasions, where SEE affects order
  but does not exclude legal moves.

The latter two change move ordering and need strength testing. Preserve en
passant and king-recapture handling when simplifying the exchange calculation.

### Reduce move-picker scanning

Reduce repeated scanning when selecting quiet moves and unfavorable captures,
while preserving move order.

### Store evaluation-only TT entries

Explore storing static evaluations without searched bounds. Weigh saved
evaluation work against extra table traffic and displacement of search entries;
evaluation-only records must never justify search cutoffs.

## Interfaces and development

### UCI features

Add MultiPV, score-bound and current-move reporting, Chess960, and optional
strength controls.
