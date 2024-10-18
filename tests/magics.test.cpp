#include "magics.hpp"

#include <gtest/gtest.h>

#include "constants.hpp"
#include "bb.hpp"

class MagicsTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() { Magics::init(); }
};

U64 targets(std::vector<Square> squares);

TEST_F(MagicsTest, BishopMiddleOfBoardNoObstacles) {
    U64 expectedAttacks = targets(
        {B1, C2, D3, F5, G6, H7, A8, H1, G2, F3, D5, C6, B7, A8});

    EXPECT_EQ(Magics::getBishopAttacks(E4, 0), expectedAttacks)
        << "should attack fully both diagonals";
}

TEST_F(MagicsTest, BishopBlockedDiagonals) {
    U64 occupancy = targets({F5, D5, F3});
    U64 expectedAttacks = targets({B1, C2, D3, F5, D5, F3});

    EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST_F(MagicsTest, BishopSurroundedByPieces) {
    U64 occupancy = targets({F5, F3, D5, D3});
    U64 expectedAttacks = targets({F5, F3, D5, D3});

    EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks)
        << "should be fully blocked";
}

TEST_F(MagicsTest, BishopEdgeOfBoardNoObstacles) {
    U64 expectedAttacks = targets({B2, C3, D4, E5, F6, G7, H8});

    EXPECT_EQ(Magics::getBishopAttacks(A1, 0), expectedAttacks)
        << "should attack single diagonal";
}

TEST_F(MagicsTest, BishopEdgeOfBoardWithObstacles) {
    U64 occupancy = targets({C3});
    U64 expectedAttacks = targets({B2, C3});

    EXPECT_EQ(Magics::getBishopAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on single diagonal";
}

TEST_F(MagicsTest, RookMiddleOfBoardNoObstacles) {
    U64 expectedAttacks =
        (BB::rankmask(RANK4, WHITE) | BB::filemask(FILE5, WHITE)) ^ BB::set(E4);

    EXPECT_EQ(Magics::getRookAttacks(E4, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST_F(MagicsTest, RookBlocked) {
    U64 occupancy = targets({D4, E5, G4});
    U64 expectedAttacks = targets({D4, E5, E3, E2, E1, F4, G4});

    EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks)
        << "should be partially blocked";
}

TEST_F(MagicsTest, RookSurroundedByPieces) {
    U64 occupancy = targets({D4, E5, E3, F4});
    U64 expectedAttacks = targets({D4, E5, E3, F4});

    EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks)
        << "should be fully blocked";
}

TEST_F(MagicsTest, RookEdgeOfBoardNoObstacles) {
    U64 expectedAttacks =
        (BB::rankmask(RANK1, WHITE) | BB::filemask(FILE1, WHITE)) ^ BB::set(A1);

    EXPECT_EQ(Magics::getRookAttacks(A1, 0), expectedAttacks)
        << "should attack fully both ranks and files";
}

TEST_F(MagicsTest, RookEdgeOfBoardWithObstacles) {
    U64 occupancy = targets({A4, B1});
    U64 expectedAttacks = targets({A2, A3, A4, B1});

    EXPECT_EQ(Magics::getRookAttacks(A1, occupancy), expectedAttacks)
        << "should be blocked on both ranks and files";
}
