#include "magic.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "constants.hpp"
#include "test_utils.hpp"

TEST(Magic_getBishopAttacks, BoardCenterNoObstacles) {
    U64 expectedAttacks = BB::set(B1, C2, D3, F5, G6, H7, A8, H1, G2, F3, D5, C6, B7, A8);

    EXPECT_EQ(Magic::getBishopAttacks(E4, 0), expectedAttacks)
        << "should attack fully both diagonals";
}

TEST(Magic_getBishopAttacks, BoardCenterBlocked) {
    U64 occupancy       = BB::set(F5, D5, F3);
    U64 expectedAttacks = BB::set(B1, C2, D3, F5, D5, F3);

    EXPECT_EQ(Magic::getBishopAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST(Magic_getBishopAttacks, BoardCenterSurrounded) {
    U64 occupancy       = BB::set(F5, F3, D5, D3);
    U64 expectedAttacks = BB::set(F5, F3, D5, D3);

    EXPECT_EQ(Magic::getBishopAttacks(E4, occupancy), expectedAttacks) << "should be fully blocked";
}

TEST(Magic_getBishopAttacks, BoardEdgeNoObstacles) {
    U64 expectedAttacks = BB::set(B2, C3, D4, E5, F6, G7, H8);

    EXPECT_EQ(Magic::getBishopAttacks(A1, 0), expectedAttacks) << "should attack single diagonal";
}

TEST(Magic_getBishopAttacks, BoardEdgeWithObstacles) {
    U64 occupancy       = BB::set(C3);
    U64 expectedAttacks = BB::set(B2, C3);

    EXPECT_EQ(Magic::getBishopAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on single diagonal";
}

TEST(Magic_getRookAttacks, BoardCenterNoObstacles) {
    U64 expectedAttacks = (BB::rankBB(Rank::R4) | BB::fileBB(File::F5)) ^ BB::set(E4);

    EXPECT_EQ(Magic::getRookAttacks(E4, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST(Magic_getRookAttacks, BoardCenterBlocked) {
    U64 occupancy       = BB::set(D4, E5, G4);
    U64 expectedAttacks = BB::set(D4, E5, E3, E2, E1, F4, G4);

    EXPECT_EQ(Magic::getRookAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST(Magic_getRookAttacks, BoardCenterSurrounded) {
    U64 occupancy       = BB::set(D4, E5, E3, F4);
    U64 expectedAttacks = BB::set(D4, E5, E3, F4);

    EXPECT_EQ(Magic::getRookAttacks(E4, occupancy), expectedAttacks) << "should be fully blocked";
}

TEST(Magic_getRookAttacks, BoardEdgeNoObstacles) {
    U64 expectedAttacks = (BB::rankBB(Rank::R1) | BB::fileBB(File::F1)) ^ BB::set(A1);

    EXPECT_EQ(Magic::getRookAttacks(A1, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST(Magic_getRookAttacks, BoardEdgeWithObstacles) {
    U64 occupancy       = BB::set(A4, B1);
    U64 expectedAttacks = BB::set(A2, A3, A4, B1);

    EXPECT_EQ(Magic::getRookAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on both ranks and files";
}
