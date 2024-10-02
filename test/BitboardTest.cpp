#include "catch.hpp"
#include "globals.hpp"
#include "bb.hpp"

TEST_CASE( "Bitboard basics", "[bitboard]" )
{
    SECTION("Is Set")
    {
        auto bitboard = BB(0x1);
        REQUIRE(bitboard.isSet(A1));
        for (auto sq = A2; sq != INVALID; sq++)
            REQUIRE(!bitboard.isSet(sq));
    }

    SECTION("Empty")
    {
        auto bitboard = BB(0x0);
        REQUIRE(bitboard.isEmpty());
        for (auto sq = A1; sq != INVALID; sq++)
            REQUIRE(!bitboard.isSet(sq));
    }

    SECTION("Clear")
    {
        auto bitboard = BB(0x1);
        bitboard.clear(A1);
        REQUIRE(bitboard.isEmpty());
    }

    SECTION("Toggle with square")
    {
        auto bitboard = BB(0x0);
        bitboard.toggle(A1);
        REQUIRE(bitboard.isSet(A1));
        REQUIRE(!bitboard.isEmpty());

        bitboard.toggle(A1);
        REQUIRE(!bitboard.isSet(A1));
        REQUIRE(bitboard.isEmpty());
    }

    SECTION("Toggle with bitboard")
    {
        auto bitboard = BB(0x0);
        bitboard.toggle(0x1);
        REQUIRE(bitboard.isSet(A1));
        REQUIRE(!bitboard.isEmpty());

        bitboard.toggle(0x1);
        REQUIRE(!bitboard.isSet(A1));
        REQUIRE(bitboard.isEmpty());
    }

    SECTION("Multiple set")
    {
        auto bitboard = BB(0x1);
        REQUIRE(!bitboard.moreThanOneSet());
        bitboard.toggle(A2);
        REQUIRE(bitboard.moreThanOneSet());
    }

    SECTION("Significant bits")
    {
        auto bitboard = BB(0x0);
        bitboard.toggle(F7);
        bitboard.toggle(B2);
        REQUIRE(bitboard.msb() == F7);
        REQUIRE(bitboard.lsb() == B2);
        bitboard.toggle(H8);
        bitboard.toggle(A1);
        REQUIRE(bitboard.msb() == H8);
        REQUIRE(bitboard.lsb() == A1);
    }

    SECTION("Bit counts")
    {
        auto bitboard = BB(0xFF);
        REQUIRE(bitboard.count() == 8);
        bitboard.toggle(0xFF00);
        REQUIRE(bitboard.count() == 16);
    }

}
