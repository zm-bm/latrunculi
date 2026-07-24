# Board Organization

This is the execution plan for improving `src/board` organization and
readability without redesigning `Board`. Each phase is one reviewable commit.
Do not begin the next phase until the current diff has been reviewed, verified,
and committed. Update the phase checklist and evidence in that same commit.

## Constraints

- Preserve chess behavior, representation, object layout, Zobrist behavior,
  repetition history, tactical caches, and caller-owned `PlyState` storage.
- Keep mutable `ply_state()`, `PlyStateStack`, the three legality tiers,
  raw/legal en-passant targets, public key recomputation, and the separate
  formatter.
- Keep `CastleSide`, the existing castling-right bit constants, `king_sq`,
  `piece_bb`, `zkey`, `bb`, and SEE terminology.
- Do not add compatibility aliases, wrappers, friends, `.inl` files,
  speculative abstractions, or redundant tests.
- Keep hot representation, attack, hash, and evaluation operations inline.
- Run five balanced standard-perft comparisons only when optimized hot-path
  output changes. Preserve node counts and investigate repeatable changes over
  2%.

## Phase 0: Record Baseline

Status: complete at `2184ba7a6ec3c6febc39f688a4d3ed3aebc57dff`.

- [x] Confirm a clean worktree.
- [x] Configure and build GNU debug and release developer builds.
- [x] Run the focused Board coverage.
- [x] Run the complete GNU debug suite.
- [x] Record object-size diagnostics.
- [x] Capture GNU and Clang optimized Board move and legality output.

Environment and results:

- GCC 13.3.0, Clang 18.1.3, CMake 3.28.3, C++23.
- Focused: 106 tests from 16 suites passed.
- Complete GNU debug: 486 tests from 49 suites passed.
- `sizeof(PlyState) == 40`; `sizeof(Board) == 256`.
- Release artifacts use `-O3 -DNDEBUG`.

Focused command:

```sh
build/debug-dev/bin/tests \
  --gtest_filter='AttacksTest.*:MagicAttacksTest.*:MoveGeometryTest.*:Board*:*FenTest.*:MoveGenTest.*:PerftTest.*'
```

Ignored baseline artifacts:

- `build/board-organization-baseline/gnu-board_move.objdump`
  (`b82a0a90f2572f803978057b34f0d49f5bb93169ad731195f5e008f1193bc735`)
- `build/board-organization-baseline/gnu-board_legality.objdump`
  (`c3a43e56ba3c9c6386140388ecb5755a07a42995851312c7317a7635f94017b0`)
- `build/board-organization-baseline/clang-18-board_move.objdump.txt`
  (`30f276b30068ec1eec7921c83446ada7e66cd6a4b9df0fdd70fbcb272a23903d`)
- `build/board-organization-baseline/clang-18-board_legality.objdump.txt`
  (`319f7e3df147d87afc47c05d7aa7cb9606eab07980f48138d9ca366158c00618`)

Planning commit: `docs: add board organization plan`

## Phase 1: Clarify File Responsibilities

Status: complete.

- [x] Rename `fen.hpp/.cpp` to `fen_parser.hpp/.cpp`.
- [x] Rename `tests/board/fen.test.cpp` to `fen_parser.test.cpp`.
- [x] Rename `board_rules.cpp` to `board_draw.cpp`.
- [x] Rename `board_notation.hpp/.cpp` to `san.hpp/.cpp`.
- [x] Rename `board_notation.test.cpp` to `san.test.cpp`.
- [x] Update CMake, includes, and affected test-suite names.
- [x] Make no symbol or behavioral changes.
- [x] Configure, build, and run FEN, draw, SAN, and format tests.
- [x] Run the complete GNU debug suite and `git diff --check`.

Evidence: 18 affected tests and all 486 GNU debug tests passed.

Commit: `refactor: clarify board file responsibilities`

## Phase 2: Trim The Board API

- [ ] Remove production-unused `see_at_least()` and its two
      threshold-equivalence tests.
- [ ] Remove unused `attacks_to(Square)`.
- [ ] Rename the boolean `attacks_to(Bitboard, Color)` overload to
      `any_attacked(Bitboard, Color)`.
- [ ] Make `load_fen()` private and remove its test-only mutation contract.
- [ ] Accept `std::string_view` in the constructor and private FEN loader.
- [ ] Retain exact SEE, parser rejection, construction, FEN round-trip, and
      invariant coverage.
- [ ] Confirm 103 focused and 483 total tests.
- [ ] Run focused and complete GNU debug tests, stale-symbol searches, and
      `git diff --check`.

Commit: `refactor: trim board api`

## Phase 3: Normalize Board Vocabulary

Apply these renames without compatibility aliases:

- [ ] `startfen` to `start_fen`.
- [ ] `piecetype_on()` to `piece_type_on()`.
- [ ] `halfmove()` and `halfmove_clk` to `halfmove_clock()` and
      `halfmove_clock`.
- [ ] `fullmove()` to `fullmove_number()`.
- [ ] `calculate_key()` to `recompute_key()`.
- [ ] `is_checking_move()` to `gives_check()`.
- [ ] `search_ply` to `ply_from_search_root`.
- [ ] `CastleRights` to `CastlingRights`.
- [ ] `castle_rights()` and stored `castle` fields to `castling_rights`.
- [ ] Replace `can_castle*` with `has_castling_rights(Color)` and
      `has_castling_right(CastleSide, Color)`.
- [ ] Rename private mutation helpers to `clear_castling_rights()` and
      `clear_rook_castling_right()`.
- [ ] Compile all eval, movegen, search, benchmark, and UCI consumers.
- [ ] Run the complete GNU debug suite, stale-name searches, and
      `git diff --check`.

Commit: `refactor: clarify board vocabulary`

## Phase 4: Organize The Board Interface

- [ ] Put the public contract before private implementation in `board.hpp`.
- [ ] Group lifecycle, representation, reversible state, attacks/castling,
      move validation, transitions, rules/evaluation, and diagnostics.
- [ ] Add a concise class-level caller-owned `PlyState` contract.
- [ ] Document make/unmake, checking, SEE, root-copy, draw-search, and
      key-recomputation preconditions.
- [ ] Preserve every data member's relative order.
- [ ] Move the cold constructor definition to `board_representation.cpp`.
- [ ] Move private castling-right mutation definitions to `board_move.cpp`.
- [ ] Remove redundant semicolons and name declarations by role.
- [ ] Keep the combined header and genuinely hot inline definitions.
- [ ] Run focused and complete GNU debug tests and `git diff --check`.
- [ ] Compare GNU and Clang optimized make/unmake output with Phase 0.

Commit: `refactor: organize board interface`

## Phase 5: Simplify State Setup

- [ ] Copy multidimensional representation arrays one row at a time.
- [ ] Use `add_piece<false>` during FEN loading before full key recomputation.
- [ ] Use `bind_ply_state()` consistently in make, unmake, and root copying.
- [ ] Preserve root history, traversal reset, hashing, ownership, and layout.
- [ ] Run FEN, representation, root-copy, move-application, invariant, and
      complete GNU debug tests.
- [ ] Run `git diff --check`.

Commit: `refactor: simplify board state setup`

## Phase 6: Clarify Legality And Move Flow

- [ ] Rename the internal legal-en-passant update to
      `refresh_legal_enpassant_target()`.
- [ ] Normalize locals to `move`, `piece_type`, `opponent`, `opponent_king`,
      `occupancy`, `captured_square`, and `destination_piece`.
- [ ] Remove unnecessary `else` branches after returns and demonstrably
      redundant guards.
- [ ] Keep only comments that explain contracts or non-obvious chess behavior.
- [ ] Do not split `board_legality.cpp` or extract new helper abstractions.
- [ ] Run 103 focused tests.
- [ ] Run all 483 GNU debug, GNU release, and Clang debug tests.
- [ ] Run formatting, stale-name searches, object-size diagnostics, and
      `git diff --check`.
- [ ] Compare optimized make/unmake and legality output with Phase 0.
- [ ] If generated code changed, run five balanced standard-perft comparisons.

Commit: `refactor: clarify board legality flow`
