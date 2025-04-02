#include "magics.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "constants.hpp"

U64 targets(const std::vector<Square>& squares);

TEST(Magics_getBishopAttacks, BoardCenterNoObstacles) {
    U64 expectedAttacks = targets({B1, C2, D3, F5, G6, H7, A8, H1, G2, F3, D5, C6, B7, A8});

    EXPECT_EQ(Magics::getBishopAttacks(E4, 0), expectedAttacks)
        << "should attack fully both diagonals";
}

TEST(Magics_getBishopAttacks, BoardCenterBlocked) {
    U64 occupancy       = targets({F5, D5, F3});
    U64 expectedAttacks = targets({B1, C2, D3, F5, D5, F3});

    EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST(Magics_getBishopAttacks, BoardCenterSurrounded) {
    U64 occupancy       = targets({F5, F3, D5, D3});
    U64 expectedAttacks = targets({F5, F3, D5, D3});

    EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks)
        << "should be fully blocked";
}

TEST(Magics_getBishopAttacks, BoardEdgeNoObstacles) {
    U64 expectedAttacks = targets({B2, C3, D4, E5, F6, G7, H8});

    EXPECT_EQ(Magics::getBishopAttacks(A1, 0), expectedAttacks) << "should attack single diagonal";
}

TEST(Magics_getBishopAttacks, BoardEdgeWithObstacles) {
    U64 occupancy       = targets({C3});
    U64 expectedAttacks = targets({B2, C3});

    EXPECT_EQ(Magics::getBishopAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on single diagonal";
}

TEST(Magics_getRookAttacks, BoardCenterNoObstacles) {
    U64 expectedAttacks = (BB::rank(RANK4) | BB::file(FILE5)) ^ BB::set(E4);

    EXPECT_EQ(Magics::getRookAttacks(E4, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST(Magics_getRookAttacks, BoardCenterBlocked) {
    U64 occupancy       = targets({D4, E5, G4});
    U64 expectedAttacks = targets({D4, E5, E3, E2, E1, F4, G4});

    EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST(Magics_getRookAttacks, BoardCenterSurrounded) {
    U64 occupancy       = targets({D4, E5, E3, F4});
    U64 expectedAttacks = targets({D4, E5, E3, F4});

    EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks) << "should be fully blocked";
}

TEST(Magics_getRookAttacks, BoardEdgeNoObstacles) {
    U64 expectedAttacks = (BB::rank(RANK1) | BB::file(FILE1)) ^ BB::set(A1);

    EXPECT_EQ(Magics::getRookAttacks(A1, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST(Magics_getRookAttacks, BoardEdgeWithObstacles) {
    U64 occupancy       = targets({A4, B1});
    U64 expectedAttacks = targets({A2, A3, A4, B1});

    EXPECT_EQ(Magics::getRookAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on both ranks and files";
}
