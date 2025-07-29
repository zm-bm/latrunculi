#include "bb.hpp"

#include <gtest/gtest.h>

#include "defs.hpp"

TEST(bbTest, SetAndClear) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        ASSERT_EQ(bb::set(sq), ~bb::clear(sq));
    }
}

TEST(bbTest, Distance) {
    EXPECT_EQ(bb::distance(A1, A1), 0);
    EXPECT_EQ(bb::distance(A1, A2), 1);
    EXPECT_EQ(bb::distance(A1, B1), 1);
    EXPECT_EQ(bb::distance(A1, B2), 1);
    EXPECT_EQ(bb::distance(A1, G7), 6);
    EXPECT_EQ(bb::distance(A1, H7), 7);
    EXPECT_EQ(bb::distance(A1, G8), 7);
    EXPECT_EQ(bb::distance(A1, H8), 7);
}

TEST(bbTest, Collinear) {
    std::vector<std::tuple<Square, Square, uint64_t>> test_cases = {
        {B2, D2, bb::rank(RANK2)},
        {B2, B4, bb::file(FILE2)},
        {A1, H8, bb::set(A1, B2, C3, D4, E5, F6, G7, H8)},
        {A8, H1, bb::set(A8, B7, C6, D5, E4, F3, G2, H1)},
        {C1, A3, bb::set(A3, B2, C1)},
        {F1, H3, bb::set(F1, G2, H3)},
        {B2, C4, 0},
    };

    for (const auto& [sq1, sq2, expected] : test_cases) {
        EXPECT_EQ(bb::collinear(sq1, sq2), expected);
        EXPECT_EQ(bb::collinear(sq2, sq1), expected);
    }
}

TEST(bbTest, Between) {
    EXPECT_EQ(bb::between(B2, D2), bb::set(C2));
    EXPECT_EQ(bb::between(D2, B2), bb::set(C2));
    EXPECT_EQ(bb::between(B2, B4), bb::set(B3));
    EXPECT_EQ(bb::between(B4, B2), bb::set(B3));
    EXPECT_EQ(bb::between(B2, C4), 0);
    EXPECT_EQ(bb::between(C4, B2), 0);
}

TEST(bbTest, IsMany) {
    EXPECT_EQ(bb::is_many(0b100), 0);
    EXPECT_NE(bb::is_many(0b110), 0);
}

TEST(bbTest, Count) {
    EXPECT_EQ(bb::count(0b0), 0);
    EXPECT_EQ(bb::count(0b1000), 1);
    EXPECT_EQ(bb::count(0b1010), 2);
    EXPECT_EQ(bb::count(0b1011), 3);
}

TEST(bbTest, Lsb) {
    EXPECT_EQ(bb::lsb(bb::set(A1)), A1);
    EXPECT_EQ(bb::lsb(bb::set(H1)), H1);
    EXPECT_EQ(bb::lsb(bb::set(A8)), A8);
    EXPECT_EQ(bb::lsb(bb::set(H8)), H8);
}

TEST(bbTest, Msb) {
    EXPECT_EQ(bb::msb(bb::set(A1)), A1);
    EXPECT_EQ(bb::msb(bb::set(H1)), H1);
    EXPECT_EQ(bb::msb(bb::set(A8)), A8);
    EXPECT_EQ(bb::msb(bb::set(H8)), H8);
}

TEST(bbTest, LsbPop) {
    uint64_t bitboard = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::lsb_pop(bitboard), A1);
    EXPECT_EQ(bb::lsb_pop(bitboard), B2);
    EXPECT_EQ(bb::lsb_pop(bitboard), C3);
    EXPECT_EQ(bitboard, 0);
}

TEST(bbTest, MsbPop) {
    uint64_t bitboard = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::msb_pop(bitboard), C3);
    EXPECT_EQ(bb::msb_pop(bitboard), B2);
    EXPECT_EQ(bb::msb_pop(bitboard), A1);
    EXPECT_EQ(bitboard, 0);
}

TEST(bbTest, SelectSquare) {
    uint64_t bitboard = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::select<WHITE>(bitboard), C3);
    EXPECT_EQ(bb::select<BLACK>(bitboard), A1);
}

TEST(bbTest, PopSquare) {
    uint64_t bitboard = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::pop<WHITE>(bitboard), C3);
    EXPECT_EQ(bitboard, bb::set(A1, B2));
    EXPECT_EQ(bb::pop<BLACK>(bitboard), A1);
    EXPECT_EQ(bitboard, bb::set(B2));
}

TEST(bbTest, Scan) {
    std::vector<Square> squares;

    uint64_t bitboard = bb::set(A1, B2, C3);
    bb::scan<WHITE>(bitboard, [&squares](Square sq) { squares.push_back(sq); });

    EXPECT_EQ(squares.size(), 3);
    EXPECT_EQ(squares[0], C3);
    EXPECT_EQ(squares[1], B2);
    EXPECT_EQ(squares[2], A1);
}

TEST(bbTest, CorrectFillNorthValues) {
    EXPECT_EQ(bb::fill_north(bb::set(A1)), bb::file(FILE1));
    EXPECT_EQ(bb::fill_north(bb::set(H1)), bb::file(FILE8));
    EXPECT_EQ(bb::fill_north(bb::set(A7)), bb::set(A7, A8));
    EXPECT_EQ(bb::fill_north(bb::set(H7)), bb::set(H7, H8));
}

TEST(bbTest, CorrectFillSouthValues) {
    EXPECT_EQ(bb::fill_south(bb::set(A8)), bb::file(FILE1));
    EXPECT_EQ(bb::fill_south(bb::set(H8)), bb::file(FILE8));
    EXPECT_EQ(bb::fill_south(bb::set(A2)), bb::set(A2, A1));
    EXPECT_EQ(bb::fill_south(bb::set(H2)), bb::set(H2, H1));
}

TEST(bbTest, CorrectFillFilesValues) {
    EXPECT_EQ(bb::fill(bb::set(A1)), bb::file(FILE1));
    EXPECT_EQ(bb::fill(bb::set(D4)), bb::file(FILE4));
    EXPECT_EQ(bb::fill(bb::set(H8)), bb::file(FILE8));
}

TEST(bbTest, CorrectShiftSouthValues) {
    EXPECT_EQ(bb::shift_south(bb::set(A1)), 0);
    EXPECT_EQ(bb::shift_south(bb::set(D4)), bb::set(D3));
    EXPECT_EQ(bb::shift_south(bb::set(H8)), bb::set(H7));
}

TEST(bbTest, CorrectShiftNorthValues) {
    EXPECT_EQ(bb::shift_north(bb::set(A1)), bb::set(A2));
    EXPECT_EQ(bb::shift_north(bb::set(D4)), bb::set(D5));
    EXPECT_EQ(bb::shift_north(bb::set(H8)), 0);
}

TEST(bbTest, CorrectShiftEastValues) {
    EXPECT_EQ(bb::shift_east(bb::set(A1)), bb::set(B1));
    EXPECT_EQ(bb::shift_east(bb::set(D4)), bb::set(E4));
    EXPECT_EQ(bb::shift_east(bb::set(H8)), 0);
}

TEST(bbTest, CorrectShiftWestValues) {
    EXPECT_EQ(bb::shift_west(bb::set(A1)), 0);
    EXPECT_EQ(bb::shift_west(bb::set(D4)), bb::set(C4));
    EXPECT_EQ(bb::shift_west(bb::set(H8)), bb::set(G8));
}

TEST(bbTest, CorrectSpanNorthValues) {
    EXPECT_EQ(bb::span_north(bb::set(A1)), bb::set(A2, A3, A4, A5, A6, A7, A8));
    EXPECT_EQ(bb::span_north(bb::set(D4)), bb::set(D5, D6, D7, D8));
    EXPECT_EQ(bb::span_north(bb::set(H8)), 0);
}

TEST(bbTest, CorrectSpanSouthValues) {
    EXPECT_EQ(bb::span_south(bb::set(A1)), 0);
    EXPECT_EQ(bb::span_south(bb::set(D4)), bb::set(D1, D2, D3));
    EXPECT_EQ(bb::span_south(bb::set(H8)), bb::set(H1, H2, H3, H4, H5, H6, H7));
}

TEST(bbTest, CorrectSpanFrontValues) {
    EXPECT_EQ(bb::span_front<WHITE>(bb::set(A4)), bb::set(A5, A6, A7, A8));
    EXPECT_EQ(bb::span_front<BLACK>(bb::set(A4)), bb::set(A3, A2, A1));
}

TEST(bbTest, CorrectSpanBackValues) {
    EXPECT_EQ(bb::span_back<WHITE>(bb::set(A4)), bb::set(A3, A2, A1));
    EXPECT_EQ(bb::span_back<BLACK>(bb::set(A4)), bb::set(A5, A6, A7, A8));
}

TEST(bbTest, CorrectPawnAttackSpanValues) {
    EXPECT_EQ(bb::attack_span<WHITE>(bb::set(D5)), bb::set(C6, E6, C7, E7, C8, E8));
    EXPECT_EQ(bb::attack_span<BLACK>(bb::set(D4)), bb::set(C3, E3, C2, E2, C1, E1));
}

TEST(bbTest, CorrectPawnFullSpanValues) {
    EXPECT_EQ(bb::full_span<WHITE>(bb::set(D6)), bb::set(C7, D7, E7, C8, D8, E8));
    EXPECT_EQ(bb::full_span<BLACK>(bb::set(D3)), bb::set(C2, D2, E2, C1, D1, E1));
}

TEST(bbTest, CorrectPawnMoves) {
    uint64_t pawns = bb::set(D4);
    EXPECT_EQ(bb::pawn_moves<PAWN_PUSH>(pawns, WHITE), bb::set(D5));
    EXPECT_EQ(bb::pawn_moves<PAWN_PUSH>(pawns, BLACK), bb::set(D3));
    EXPECT_EQ(bb::pawn_moves<PAWN_LEFT>(pawns, WHITE), bb::set(C5));
    EXPECT_EQ(bb::pawn_moves<PAWN_RIGHT>(pawns, WHITE), bb::set(E5));
    EXPECT_EQ(bb::pawn_moves<PAWN_LEFT>(pawns, BLACK), bb::set(E3));
    EXPECT_EQ(bb::pawn_moves<PAWN_RIGHT>(pawns, BLACK), bb::set(C3));
    EXPECT_EQ(bb::pawn_moves<PAWN_PUSH2>(pawns, WHITE), bb::set(D6));
    EXPECT_EQ(bb::pawn_moves<PAWN_PUSH2>(pawns, BLACK), bb::set(D2));

    uint64_t pawns_left = bb::set(A4);
    EXPECT_EQ(bb::pawn_moves<PAWN_LEFT>(pawns_left, WHITE), 0);
    EXPECT_EQ(bb::pawn_moves<PAWN_RIGHT>(pawns_left, BLACK), 0);

    uint64_t pawns_right = bb::set(H4);
    EXPECT_EQ(bb::pawn_moves<PAWN_RIGHT>(pawns_right, WHITE), 0);
    EXPECT_EQ(bb::pawn_moves<PAWN_LEFT>(pawns_right, BLACK), 0);
}

TEST(bbTest, CorrectPawnAttacks) {
    uint64_t pawns = bb::set(A4, D4, H4);
    EXPECT_EQ(bb::pawn_attacks(pawns, WHITE), bb::set(B5, C5, E5, G5));
    EXPECT_EQ(bb::pawn_attacks(pawns, BLACK), bb::set(B3, C3, E3, G3));
}

TEST(bbTest, CorrectMovesKnights) {
    EXPECT_EQ(bb::moves<KNIGHT>(A1), bb::set(B3, C2));
    EXPECT_EQ(bb::moves<KNIGHT>(H1), bb::set(G3, F2));
    EXPECT_EQ(bb::moves<KNIGHT>(A8), bb::set(B6, C7));
    EXPECT_EQ(bb::moves<KNIGHT>(H8), bb::set(G6, F7));
    EXPECT_EQ(bb::moves<KNIGHT>(G2), bb::set(E1, E3, F4, H4));
    EXPECT_EQ(bb::moves<KNIGHT>(C6), bb::set(A5, A7, B4, B8, D4, D8, E5, E7));
}

TEST(bbTest, CorrectMovesKings) {
    EXPECT_EQ(bb::moves<KING>(A1), bb::set(A2, B2, B1));
    EXPECT_EQ(bb::moves<KING>(H1), bb::set(H2, G2, G1));
    EXPECT_EQ(bb::moves<KING>(A8), bb::set(A7, B7, B8));
    EXPECT_EQ(bb::moves<KING>(H8), bb::set(H7, G7, G8));
    EXPECT_EQ(bb::moves<KING>(G2), bb::set(F1, F2, F3, G1, G3, H1, H2, H3));
}

// magic attacks are not tested here, as they are tested in magic.test.cpp
