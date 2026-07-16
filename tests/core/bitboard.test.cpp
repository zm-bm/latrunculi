#include "core/bitboard.hpp"
#include "core/square.hpp"

#include <vector>

#include <gtest/gtest.h>

TEST(bbTest, SingleSquareMasks) {
    for (int sq = A1; sq < INVALID; ++sq) {
        const Square square = Square(sq);
        ASSERT_EQ(bb::set(square), ~bb::clear_mask(square));
        EXPECT_TRUE(bb::contains(bb::set(square), square));
    }

    const Bitboard bitboard = bb::set(A1, B2, C3);
    EXPECT_TRUE(bb::contains(bitboard, A1));
    EXPECT_TRUE(bb::contains(bitboard, C3));
    EXPECT_FALSE(bb::contains(bitboard, D4));
}

TEST(bbTest, MutatingSquareOperationsAreIdempotent) {
    Bitboard bitboard = bb::set(A1, C3);

    bb::add(bitboard, B2);
    EXPECT_EQ(bitboard, bb::set(A1, B2, C3));

    bb::add(bitboard, B2);
    EXPECT_EQ(bitboard, bb::set(A1, B2, C3));

    bb::remove(bitboard, B2);
    EXPECT_EQ(bitboard, bb::set(A1, C3));

    bb::remove(bitboard, B2);
    EXPECT_EQ(bitboard, bb::set(A1, C3));

    bb::move(bitboard, A1, B2);
    EXPECT_EQ(bitboard, bb::set(B2, C3));

    bb::move(bitboard, A1, B2);
    EXPECT_EQ(bitboard, bb::set(B2, C3));

    bb::move(bitboard, B2, C3);
    EXPECT_EQ(bitboard, bb::set(C3));
}

TEST(bbTest, Cardinality) {
    EXPECT_EQ(bb::count(0b0), 0);
    EXPECT_EQ(bb::count(0b1000), 1);
    EXPECT_EQ(bb::count(0b1010), 2);
    EXPECT_EQ(bb::count(0b1011), 3);

    EXPECT_FALSE(bb::is_many(0b100));
    EXPECT_TRUE(bb::is_many(0b110));
    EXPECT_FALSE(bb::is_one(0b0));
    EXPECT_TRUE(bb::is_one(0b100));
    EXPECT_FALSE(bb::is_one(0b110));
}

TEST(bbTest, BitScans) {
    EXPECT_EQ(bb::lsb(bb::set(A1)), A1);
    EXPECT_EQ(bb::lsb(bb::set(H1)), H1);
    EXPECT_EQ(bb::lsb(bb::set(A8)), A8);
    EXPECT_EQ(bb::lsb(bb::set(H8)), H8);

    EXPECT_EQ(bb::msb(bb::set(A1)), A1);
    EXPECT_EQ(bb::msb(bb::set(H1)), H1);
    EXPECT_EQ(bb::msb(bb::set(A8)), A8);
    EXPECT_EQ(bb::msb(bb::set(H8)), H8);

    EXPECT_EQ(bb::lsb_mask(bb::set(A1)), bb::set(A1));
    EXPECT_EQ(bb::lsb_mask(bb::set(H1)), bb::set(H1));
    EXPECT_EQ(bb::lsb_mask(bb::set(A1, B2, C3)), bb::set(A1));
}

TEST(bbTest, OrderedPop) {
    Bitboard low_to_high = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::lsb_pop(low_to_high), A1);
    EXPECT_EQ(bb::lsb_pop(low_to_high), B2);
    EXPECT_EQ(bb::lsb_pop(low_to_high), C3);
    EXPECT_EQ(low_to_high, 0);

    Bitboard high_to_low = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::msb_pop(high_to_low), C3);
    EXPECT_EQ(bb::msb_pop(high_to_low), B2);
    EXPECT_EQ(bb::msb_pop(high_to_low), A1);
    EXPECT_EQ(high_to_low, 0);

    const Bitboard bitboard = bb::set(A1, B2, C3);
    EXPECT_EQ(bb::frontmost<WHITE>(bitboard), C3);
    EXPECT_EQ(bb::frontmost<BLACK>(bitboard), A1);

    Bitboard ordered = bitboard;
    EXPECT_EQ(bb::pop<WHITE>(ordered), C3);
    EXPECT_EQ(ordered, bb::set(A1, B2));
    EXPECT_EQ(bb::pop<BLACK>(ordered), A1);
    EXPECT_EQ(ordered, bb::set(B2));
}

TEST(bbTest, Scan) {
    std::vector<Square> squares;

    Bitboard bitboard = bb::set(A1, B2, C3);
    bb::scan<WHITE>(bitboard, [&squares](Square sq) { squares.push_back(sq); });

    EXPECT_EQ(squares.size(), 3);
    EXPECT_EQ(squares[0], C3);
    EXPECT_EQ(squares[1], B2);
    EXPECT_EQ(squares[2], A1);
    EXPECT_EQ(bitboard, bb::set(A1, B2, C3));
}

TEST(bbTest, FillAndShifts) {
    EXPECT_EQ(bb::fill_north(bb::set(A1)), bb::file(FILE1));
    EXPECT_EQ(bb::fill_north(bb::set(H1)), bb::file(FILE8));
    EXPECT_EQ(bb::fill_north(bb::set(A7)), bb::set(A7, A8));
    EXPECT_EQ(bb::fill_north(bb::set(H7)), bb::set(H7, H8));

    EXPECT_EQ(bb::fill_south(bb::set(A8)), bb::file(FILE1));
    EXPECT_EQ(bb::fill_south(bb::set(H8)), bb::file(FILE8));
    EXPECT_EQ(bb::fill_south(bb::set(A2)), bb::set(A2, A1));
    EXPECT_EQ(bb::fill_south(bb::set(H2)), bb::set(H2, H1));

    EXPECT_EQ(bb::fill(bb::set(A1)), bb::file(FILE1));
    EXPECT_EQ(bb::fill(bb::set(D4)), bb::file(FILE4));
    EXPECT_EQ(bb::fill(bb::set(H8)), bb::file(FILE8));

    EXPECT_EQ(bb::shift_south(bb::set(A1)), 0);
    EXPECT_EQ(bb::shift_south(bb::set(D4)), bb::set(D3));
    EXPECT_EQ(bb::shift_south(bb::set(H8)), bb::set(H7));

    EXPECT_EQ(bb::shift_north(bb::set(A1)), bb::set(A2));
    EXPECT_EQ(bb::shift_north(bb::set(D4)), bb::set(D5));
    EXPECT_EQ(bb::shift_north(bb::set(H8)), 0);

    EXPECT_EQ(bb::shift_east(bb::set(A1)), bb::set(B1));
    EXPECT_EQ(bb::shift_east(bb::set(D4)), bb::set(E4));
    EXPECT_EQ(bb::shift_east(bb::set(H8)), 0);

    EXPECT_EQ(bb::shift_west(bb::set(A1)), 0);
    EXPECT_EQ(bb::shift_west(bb::set(D4)), bb::set(C4));
    EXPECT_EQ(bb::shift_west(bb::set(H8)), bb::set(G8));
}

TEST(bbTest, Spans) {
    EXPECT_EQ(bb::span_north(bb::set(A1)), bb::set(A2, A3, A4, A5, A6, A7, A8));
    EXPECT_EQ(bb::span_north(bb::set(D4)), bb::set(D5, D6, D7, D8));
    EXPECT_EQ(bb::span_north(bb::set(H8)), 0);

    EXPECT_EQ(bb::span_south(bb::set(A1)), 0);
    EXPECT_EQ(bb::span_south(bb::set(D4)), bb::set(D1, D2, D3));
    EXPECT_EQ(bb::span_south(bb::set(H8)), bb::set(H1, H2, H3, H4, H5, H6, H7));

    EXPECT_EQ(bb::span_front<WHITE>(bb::set(A4)), bb::set(A5, A6, A7, A8));
    EXPECT_EQ(bb::span_front<BLACK>(bb::set(A4)), bb::set(A3, A2, A1));

    EXPECT_EQ(bb::span_back<WHITE>(bb::set(A4)), bb::set(A3, A2, A1));
    EXPECT_EQ(bb::span_back<BLACK>(bb::set(A4)), bb::set(A5, A6, A7, A8));

    EXPECT_EQ(bb::attack_span<WHITE>(bb::set(D5)), bb::set(C6, E6, C7, E7, C8, E8));
    EXPECT_EQ(bb::attack_span<BLACK>(bb::set(D4)), bb::set(C3, E3, C2, E2, C1, E1));

    EXPECT_EQ(bb::full_span<WHITE>(bb::set(D6)), bb::set(C7, D7, E7, C8, D8, E8));
    EXPECT_EQ(bb::full_span<BLACK>(bb::set(D3)), bb::set(C2, D2, E2, C1, D1, E1));
}
