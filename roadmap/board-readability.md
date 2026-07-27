# Board Readability

This is the temporary execution plan for improving `src/board` readability
without changing its architecture. Each phase is one reviewable commit. Do not
begin the next phase until the current diff has been reviewed, verified, and
committed. Update each phase's checklist and evidence in that same commit.

## Audit Conclusions

- Keep `board.hpp` combined. Its out-of-class definitions are hot queries,
  templates, or synchronized representation mutations that benefit from
  remaining inline.
- Keep the current production file boundaries. The remaining work is contract
  documentation, narrower helper exposure, precise vocabulary, and local flow
  cleanup rather than another reorganization.
- Remove the unused public no-color `Board::pieces()` query. Keep colorless
  typed piece queries and occupancy-aware attack helpers private.
- Rename `PlyState::captured` to `captured_piece_type`.
- Reject FENs containing more than 32 pieces. Legal play cannot increase the
  initial piece count, and this preserves SEE's compact 32-entry swap list.
- Make Board formatting explicitly accept only the empty format specification.
- Finish with a small test cleanup that removes duplication without weakening
  substantive Board coverage.

## Constraints

- Preserve chess behavior for valid positions, Board and PlyState layouts,
  ownership, representation, Zobrist behavior, history, and tactical caches.
- Keep explicit make/unmake flow, all three legality tiers, raw and legal
  en-passant targets, `PlyStateStack`, and public key recomputation.
- Do not add production files, `.inl` files, compatibility aliases, wrappers,
  speculative abstractions, or redundant tests.
- Keep `PieceSquare`, `ParsedFen`, existing file boundaries, explicit snapshot
  comparisons, and all substantive Board tests.
- Preserve optimized hot-path output where practical. Benchmark only when
  relevant GNU or Clang output changes; investigate a repeatable change over
  2%.

## Phase 0: Record Baseline

Status: complete at `c7f23f9a5c4f1df056c6a05200a412a013fe1581`.

- [x] Confirm a clean worktree.
- [x] Record compiler and CMake versions.
- [x] Configure and build the GNU debug and GNU/Clang release developer builds.
- [x] Run the focused Board coverage.
- [x] Run the complete GNU debug suite.
- [x] Record object-size diagnostics.
- [x] Preserve GNU and Clang release binaries and optimized artifacts for
      Board move, legality, SEE, movegen/perft, and evaluation.
- [x] Reproduce the oversized-position SEE bound failure with sanitizers.

Environment:

- GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, C++23.
- Focused: 103 tests from 16 suites passed.
- Complete GNU debug: 483 tests from 49 suites passed.
- `sizeof(PlyState) == 40`; `sizeof(Board) == 256`.
- Release artifacts use `-O3 -DNDEBUG`.

Focused command:

```sh
build/debug-dev/bin/tests \
  --gtest_filter='AttacksTest.*:MagicAttacksTest.*:MoveGeometryTest.*:Board*:FenParserTest.*:SanTest.*:MoveGenTest.*:PerftTest.*'
```

The current parser accepts this 38-piece FEN:

```text
3q1k1Q/q2Q2q1/1QNQnQ2/1nQqqN2/qqqqQQqq/1nQQQn2/1QNqNQ2/qK1Q2q1 w - - 0 1
```

Calling SEE for the pseudo-legal capture `e4d4` reaches `gain[32]`.
ASan/UBSan reports a stack-buffer overflow at `board_see.cpp:56`.

Ignored baseline artifacts are under `build/board-readability-baseline/`.
`SHA256SUMS` verifies the preserved binaries, objects, normalized disassembly,
and symbol listings. The engine binary hashes are:

- GNU: `9917ac3e9c21fd66560a0cc033d3b65c289ab5519833b55573bfb827e43e5c2a`.
- Clang: `d7236d4dd74c1631138e9b9c4a52f1b948efd74285c54a6a81effe76e17c022d`.

Planning commit: `docs: add board readability plan`

## Phase 1: Refine The Board Interface

Status: complete.

- [x] Correct the `copy_root_from()` root, history, storage-lifetime, and alias
      contract.
- [x] Document raw/legal en-passant, blockers, attack queries, move
      classification, synchronized representation mutation, and key history.
- [x] Remove public no-color `pieces()` and compute `occupancy()` directly from
      the two color aggregates.
- [x] Make colorless typed pieces and occupancy-aware attack queries private;
      rename the both-color helper to `all_attackers_to()`.
- [x] Reorder inline definitions as public queries, private query helpers, then
      private representation mutation.
- [x] Normalize template, blocker, and castling-query vocabulary.
- [x] Rename `root_history` to `copied_history` and document its sequencing.
- [x] Verify consumers, focused/full GNU debug tests, layouts, stale symbols,
      optimized GNU/Clang output, and `git diff --check`.

Evidence: 103 focused and all 483 GNU debug tests passed;
`sizeof(PlyState)` remains 40 bytes and `sizeof(Board)` remains 256 bytes.
Mapped SEE, movegen/perft, evaluation, unmake, and null-move output is
byte-identical under GNU and Clang. Explicit color comparisons changed only
`make()` and pseudo-legality paths. Five alternating GNU standard-perft pairs
preserved all 11,168,869 nodes; median total time changed from 400.773 ms to
398.311 ms (-0.61%).

Commit: `refactor: refine board interface`

## Phase 2: Clarify Board State Flow

Status: pending.

- [ ] Rename `PlyState::captured` to `captured_piece_type`.
- [ ] Normalize dependent snapshot fields to `side_to_move`,
      `captured_piece_type`, and `psq_bonus`.
- [ ] Rename make/unmake's local `type` to `move_type`.
- [ ] Document the partial inheritance performed by `initialize_next_ply()`;
      keep the helper outside `PlyState`.
- [ ] Use descriptive reset-loop indices without changing storage or flow.
- [ ] Verify move application, root copy, invariants, full GNU debug, layouts,
      stale names, make/unmake output, and `git diff --check`.

Commit: `refactor: clarify board state flow`

## Phase 3: Clarify FEN And Notation Support

Status: pending.

- [ ] Normalize parser vocabulary around FEN fields, piece placement, side to
      move, castling rights, en-passant target, and absolute ply conversion.
- [ ] Reject the 33rd piece and add the known overflowing FEN to the existing
      invalid-FEN table.
- [ ] Clarify Board FEN locals and write castling rights with direct
      conditionals.
- [ ] Document `to_san()`'s legal-move precondition and normalize SAN locals.
- [ ] Replace formatter inheritance with an empty-spec `parse()` while
      preserving default output.
- [ ] Verify parser, FEN round-trip, SAN, formatting, full GNU debug, and
      `git diff --check`.

Commit: `refactor: clarify board notation support`

## Phase 4: Clarify Rule Algorithms

Status: pending.

- [ ] Explain en-passant occupancy simulation and rename candidate capturers.
- [ ] Cache side to move once during tactical refresh.
- [ ] Clarify reversible-history, plies-back, and prior-occurrence vocabulary
      and document search-line twofold versus game-history threefold handling.
- [ ] Clarify SEE attacker and gain names, swap-list indexing, early cutoff,
      parser-backed bound, and explicit unwind flow.
- [ ] Verify legality/draw/SEE tests and complete GNU debug/release and Clang
      debug suites.
- [ ] Compare optimized output and run balanced perft or search comparisons
      only if relevant output changes.
- [ ] Run formatting, stale-name searches, layout diagnostics, and
      `git diff --check`.

Commit: `refactor: clarify board rule algorithms`

## Phase 5: Simplify Board Tests

Status: pending.

- [ ] Delete the redundant halfmove-clock accessor test.
- [ ] Move irreversible-move repetition restoration from invariants to draw
      coverage.
- [ ] Rename the kings-only representation test accurately.
- [ ] Consolidate repeated make/FEN/unmake checks in one local helper while
      keeping en-passant cache cases explicit.
- [ ] Make `first_legal_move()` accept `const Board&`.
- [ ] Replace dynamic fixed-case containers with constexpr arrays and named
      case structs.
- [ ] Confirm 102 focused and 482 total tests under GNU debug/release and Clang
      debug.
- [ ] Run formatting, stale-reference searches, layout diagnostics, and
      `git diff --check`.

Commit: `tests: simplify board coverage`

## Retirement

After every phase is reviewed and committed, delete this temporary document.

Commit: `docs: retire board readability plan`
