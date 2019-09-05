#include "catch.hpp"
#include "globals.hpp"

TEST_CASE( "Test globals", "[globals]")
{
    G::init();

    SECTION("Knight attacks")
    {
        REQUIRE(G::KNIGHT_ATTACKS[A1] == (G::bitset(B3) 
                                        | G::bitset(C2)));

        REQUIRE(G::KNIGHT_ATTACKS[G2] == (G::bitset(E1)
                                        | G::bitset(E3)
                                        | G::bitset(F4)
                                        | G::bitset(H4)));

        REQUIRE(G::KNIGHT_ATTACKS[C6] == (G::bitset(A5)
                                        | G::bitset(A7)
                                        | G::bitset(B4)
                                        | G::bitset(B8)
                                        | G::bitset(D4)
                                        | G::bitset(D8)
                                        | G::bitset(E5)
                                        | G::bitset(E7)));
    }

    SECTION("King attacks")
    {
        REQUIRE(G::KING_ATTACKS[A1] == (G::bitset(A2) 
                                      | G::bitset(B2)
                                      | G::bitset(B1)));

        REQUIRE(G::KING_ATTACKS[G2] == (G::bitset(F1)
                                      | G::bitset(F2)
                                      | G::bitset(F3)
                                      | G::bitset(G1)
                                      | G::bitset(G3)
                                      | G::bitset(H1)
                                      | G::bitset(H2)
                                      | G::bitset(H3)));

    }

}