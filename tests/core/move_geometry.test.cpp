#include "core/move_geometry.hpp"

#include <gtest/gtest.h>

TEST(MoveGeometryTest, MapsEnPassantSquaresForBothColors) {
    EXPECT_EQ(move_geometry::pawn_push(WHITE), square::north);
    EXPECT_EQ(move_geometry::pawn_push(BLACK), square::south);

    EXPECT_EQ(move_geometry::enpassant_target(E4, WHITE), E3);
    EXPECT_EQ(move_geometry::enpassant_captured_square(E3, BLACK), E4);
    EXPECT_EQ(move_geometry::enpassant_target(E5, BLACK), E6);
    EXPECT_EQ(move_geometry::enpassant_captured_square(E6, WHITE), E5);
}

TEST(MoveGeometryTest, MapsCastlingForBothColorsAndSides) {
    const auto expect_castling = [](CastleSide side,
                                    Color      color,
                                    Square     king_from,
                                    Square     king_to,
                                    Square     rook_from,
                                    Square     rook_to,
                                    Bitboard   empty_path,
                                    Bitboard   king_path) {
        const auto& castling = move_geometry::castling(side, color);
        EXPECT_EQ(castling.king_from, king_from);
        EXPECT_EQ(castling.king_to, king_to);
        EXPECT_EQ(castling.rook_from, rook_from);
        EXPECT_EQ(castling.rook_to, rook_to);
        EXPECT_EQ(castling.empty_path, empty_path);
        EXPECT_EQ(castling.king_path, king_path);
        EXPECT_EQ(move_geometry::castle_side(king_from, king_to), side);
    };

    expect_castling(CASTLE_KINGSIDE, WHITE, E1, G1, H1, F1, bb::set(F1, G1), bb::set(E1, F1, G1));
    expect_castling(
        CASTLE_QUEENSIDE, WHITE, E1, C1, A1, D1, bb::set(B1, C1, D1), bb::set(C1, D1, E1));
    expect_castling(CASTLE_KINGSIDE, BLACK, E8, G8, H8, F8, bb::set(F8, G8), bb::set(E8, F8, G8));
    expect_castling(
        CASTLE_QUEENSIDE, BLACK, E8, C8, A8, D8, bb::set(B8, C8, D8), bb::set(C8, D8, E8));
}
