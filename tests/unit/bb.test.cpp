#include "bb.hpp"

#include <gtest/gtest.h>

#include <numeric>

#include "test_utils.hpp"
#include "types.hpp"

TEST(BB, CorrectSet) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::set(Square(i));
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BB, CorrectClear) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::clear(Square(i));
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BB, CorrectFileBB) {
    for (int i = 0; i < 8; ++i) {
        U64 result   = BB::fileBB(File(i));
        U64 expected = 0x0101010101010101ULL << i;
        ASSERT_EQ(result, expected) << "Failed at file " << i;
    }
}

TEST(BB, CorrectRankBB) {
    for (int i = 0; i < 8; ++i) {
        U64 result   = BB::rankBB(Rank(i));
        U64 expected = 0xFFULL << (i * 8);
        ASSERT_EQ(result, expected) << "Failed at rank " << i;
    }
}

TEST(BB, CorrectDistanceValues) {
    EXPECT_EQ(BB::distance(A1, A1), 0);
    EXPECT_EQ(BB::distance(A1, A2), 1);
    EXPECT_EQ(BB::distance(A1, B1), 1);
    EXPECT_EQ(BB::distance(A1, B2), 1);
    EXPECT_EQ(BB::distance(A1, G7), 6);
    EXPECT_EQ(BB::distance(A1, H7), 7);
    EXPECT_EQ(BB::distance(A1, G8), 7);
    EXPECT_EQ(BB::distance(A1, H8), 7);
}

TEST(BB, CorrectCollinear) {
    EXPECT_EQ(BB::collinear(B2, D2), BB::rankBB(Rank::R2));
    EXPECT_EQ(BB::collinear(D2, B2), BB::rankBB(Rank::R2));
    EXPECT_EQ(BB::collinear(B2, B4), BB::fileBB(File::F2));
    EXPECT_EQ(BB::collinear(B4, B2), BB::fileBB(File::F2));
    EXPECT_EQ(BB::collinear(A1, H8), BB::set(A1, B2, C3, D4, E5, F6, G7, H8));
    EXPECT_EQ(BB::collinear(H8, A1), BB::set(A1, B2, C3, D4, E5, F6, G7, H8));
    EXPECT_EQ(BB::collinear(B2, C4), 0);
    EXPECT_EQ(BB::collinear(C4, B2), 0);
}

TEST(BB, CorrectBetween) {
    EXPECT_EQ(BB::between(B2, D2), BB::set(C2));
    EXPECT_EQ(BB::between(D2, B2), BB::set(C2));
    EXPECT_EQ(BB::between(B2, B4), BB::set(B3));
    EXPECT_EQ(BB::between(B4, B2), BB::set(B3));
    EXPECT_EQ(BB::between(B2, C4), 0);
    EXPECT_EQ(BB::between(C4, B2), 0);
}

TEST(BB, CorrectMoreThanOneValues) {
    EXPECT_EQ(BB::isMany(0b100), 0);
    EXPECT_NE(BB::isMany(0b110), 0);
}

TEST(BB, CorrectBitCount) {
    EXPECT_EQ(BB::count(0b0), 0);
    EXPECT_EQ(BB::count(0b1000), 1);
    EXPECT_EQ(BB::count(0b1010), 2);
    EXPECT_EQ(BB::count(0b1011), 3);
}

TEST(BB, CorrectLeastSignificantBit) {
    EXPECT_EQ(BB::lsb(BB::set(A1)), A1);
    EXPECT_EQ(BB::lsb(BB::set(H1)), H1);
    EXPECT_EQ(BB::lsb(BB::set(A8)), A8);
    EXPECT_EQ(BB::lsb(BB::set(H8)), H8);
}

TEST(BB, CorrectMostSignificantBit) {
    EXPECT_EQ(BB::msb(BB::set(A1)), A1);
    EXPECT_EQ(BB::msb(BB::set(H1)), H1);
    EXPECT_EQ(BB::msb(BB::set(A8)), A8);
    EXPECT_EQ(BB::msb(BB::set(H8)), H8);
}

TEST(BB, CorrectLeastSignificantBitPop) {
    U64 bb = BB::set(A1, B2, C3);
    EXPECT_EQ(BB::lsbPop(bb), A1);
    EXPECT_EQ(BB::lsbPop(bb), B2);
    EXPECT_EQ(BB::lsbPop(bb), C3);
    EXPECT_EQ(bb, 0);
}

TEST(BB, CorrectMostSignificantBitPop) {
    U64 bb = BB::set(A1, B2, C3);
    EXPECT_EQ(BB::msbPop(bb), C3);
    EXPECT_EQ(BB::msbPop(bb), B2);
    EXPECT_EQ(BB::msbPop(bb), A1);
    EXPECT_EQ(bb, 0);
}

TEST(BB, CorrectFillNorthValues) {
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), BB::fileBB(File::F1));
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), BB::fileBB(File::F8));
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7, A8));
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7, H8));
}

TEST(BB, CorrectFillSouthValues) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), BB::fileBB(File::F1));
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), BB::fileBB(File::F8));
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2, A1));
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2, H1));
}

TEST(BB, CorrectFillFilesValues) {
    EXPECT_EQ(BB::fillFiles(BB::set(A1)), BB::fileBB(File::F1));
    EXPECT_EQ(BB::fillFiles(BB::set(D4)), BB::fileBB(File::F4));
    EXPECT_EQ(BB::fillFiles(BB::set(H8)), BB::fileBB(File::F8));
}

TEST(BB, CorrectShiftSouthValues) {
    EXPECT_EQ(BB::shiftSouth(BB::set(A1)), 0);
    EXPECT_EQ(BB::shiftSouth(BB::set(D4)), BB::set(D3));
    EXPECT_EQ(BB::shiftSouth(BB::set(H8)), BB::set(H7));
}

TEST(BB, CorrectShiftNorthValues) {
    EXPECT_EQ(BB::shiftNorth(BB::set(A1)), BB::set(A2));
    EXPECT_EQ(BB::shiftNorth(BB::set(D4)), BB::set(D5));
    EXPECT_EQ(BB::shiftNorth(BB::set(H8)), 0);
}

TEST(BB, CorrectShiftEastValues) {
    EXPECT_EQ(BB::shiftEast(BB::set(A1)), BB::set(B1));
    EXPECT_EQ(BB::shiftEast(BB::set(D4)), BB::set(E4));
    EXPECT_EQ(BB::shiftEast(BB::set(H8)), 0);
}

TEST(BB, CorrectShiftWestValues) {
    EXPECT_EQ(BB::shiftWest(BB::set(A1)), 0);
    EXPECT_EQ(BB::shiftWest(BB::set(D4)), BB::set(C4));
    EXPECT_EQ(BB::shiftWest(BB::set(H8)), BB::set(G8));
}

TEST(BB, CorrectSpanNorthValues) {
    EXPECT_EQ(BB::spanNorth(BB::set(A1)), BB::set(A2, A3, A4, A5, A6, A7, A8));
    EXPECT_EQ(BB::spanNorth(BB::set(D4)), BB::set(D5, D6, D7, D8));
    EXPECT_EQ(BB::spanNorth(BB::set(H8)), 0);
}

TEST(BB, CorrectSpanSouthValues) {
    EXPECT_EQ(BB::spanSouth(BB::set(A1)), 0);
    EXPECT_EQ(BB::spanSouth(BB::set(D4)), BB::set(D1, D2, D3));
    EXPECT_EQ(BB::spanSouth(BB::set(H8)), BB::set(H1, H2, H3, H4, H5, H6, H7));
}

TEST(BB, CorrectSpanFrontValues) {
    EXPECT_EQ(BB::spanFront<WHITE>(BB::set(A4)), BB::set(A5, A6, A7, A8));
    EXPECT_EQ(BB::spanFront<BLACK>(BB::set(A4)), BB::set(A3, A2, A1));
}

TEST(BB, CorrectSpanBackValues) {
    EXPECT_EQ(BB::spanBack<WHITE>(BB::set(A4)), BB::set(A3, A2, A1));
    EXPECT_EQ(BB::spanBack<BLACK>(BB::set(A4)), BB::set(A5, A6, A7, A8));
}

TEST(BB, CorrectPawnAttackSpanValues) {
    EXPECT_EQ(BB::pawnAttackSpan<WHITE>(BB::set(D5)), BB::set(C6, E6, C7, E7, C8, E8));
    EXPECT_EQ(BB::pawnAttackSpan<BLACK>(BB::set(D4)), BB::set(C3, E3, C2, E2, C1, E1));
}

TEST(BB, CorrectPawnFullSpanValues) {
    EXPECT_EQ(BB::pawnFullSpan<WHITE>(BB::set(D6)), BB::set(C7, D7, E7, C8, D8, E8));
    EXPECT_EQ(BB::pawnFullSpan<BLACK>(BB::set(D3)), BB::set(C2, D2, E2, C1, D1, E1));
}

TEST(BB, CorrectPawnMoves) {
    U64 pawns = BB::set(D4);
    EXPECT_EQ(BB::pawnMoves<PawnMove::Push>(pawns, WHITE), BB::set(D5));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Push>(pawns, BLACK), BB::set(D3));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Left>(pawns, WHITE), BB::set(C5));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Right>(pawns, WHITE), BB::set(E5));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Left>(pawns, BLACK), BB::set(E3));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Right>(pawns, BLACK), BB::set(C3));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Double>(pawns, WHITE), BB::set(D6));
    EXPECT_EQ(BB::pawnMoves<PawnMove::Double>(pawns, BLACK), BB::set(D2));

    U64 pawnsLeft = BB::set(A4);
    EXPECT_EQ(BB::pawnMoves<PawnMove::Left>(pawnsLeft, WHITE), 0);
    EXPECT_EQ(BB::pawnMoves<PawnMove::Right>(pawnsLeft, BLACK), 0);

    U64 pawnsRight = BB::set(H4);
    EXPECT_EQ(BB::pawnMoves<PawnMove::Right>(pawnsRight, WHITE), 0);
    EXPECT_EQ(BB::pawnMoves<PawnMove::Left>(pawnsRight, BLACK), 0);
}

TEST(BB, CorrectPawnAttacks) {
    U64 pawns = BB::set(A4, D4, H4);
    EXPECT_EQ(BB::pawnAttacks(pawns, WHITE), BB::set(B5, C5, E5, G5));
    EXPECT_EQ(BB::pawnAttacks(pawns, BLACK), BB::set(B3, C3, E3, G3));
}

TEST(BB, CorrectPawnDoubleAttacks) {
    U64 pawns = BB::set(C3, D4, E4, F4, E5);
    EXPECT_EQ(BB::pawnDoubleAttacks(pawns, WHITE), BB::set(E5));
    EXPECT_EQ(BB::pawnDoubleAttacks(pawns, BLACK), BB::set(E3));
}

TEST(BB, CorrectMovesKnights) {
    EXPECT_EQ(BB::moves<PieceType::Knight>(A1), BB::set(B3, C2));
    EXPECT_EQ(BB::moves<PieceType::Knight>(H1), BB::set(G3, F2));
    EXPECT_EQ(BB::moves<PieceType::Knight>(A8), BB::set(B6, C7));
    EXPECT_EQ(BB::moves<PieceType::Knight>(H8), BB::set(G6, F7));
    EXPECT_EQ(BB::moves<PieceType::Knight>(G2), BB::set(E1, E3, F4, H4));
    EXPECT_EQ(BB::moves<PieceType::Knight>(C6), BB::set(A5, A7, B4, B8, D4, D8, E5, E7));
}

// magic attacks are not tested here, as they are tested in magic.test.cpp

TEST(BB, CorrectMovesKings) {
    EXPECT_EQ(BB::moves<PieceType::King>(A1), BB::set(A2, B2, B1));
    EXPECT_EQ(BB::moves<PieceType::King>(H1), BB::set(H2, G2, G1));
    EXPECT_EQ(BB::moves<PieceType::King>(A8), BB::set(A7, B7, B8));
    EXPECT_EQ(BB::moves<PieceType::King>(H8), BB::set(H7, G7, G8));
    EXPECT_EQ(BB::moves<PieceType::King>(G2), BB::set(F1, F2, F3, G1, G3, H1, H2, H3));
}
