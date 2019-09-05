#include "catch.hpp"
#include "globals.hpp"
#include "types.hpp"

TEST_CASE( "Test types", "[types]")
{
    G::init();

    SECTION("color")
    {
        REQUIRE(~WHITE == BLACK);
    }

    SECTION("square")
    {
        REQUIRE(A1 == Types::getSquare(FILE1, RANK1));
        REQUIRE(E4 == Types::getSquare(FILE5, RANK4));
        REQUIRE(H8 == Types::getSquare(FILE8, RANK8));

        REQUIRE(RANK1 == Types::getRank(A1));
        REQUIRE(RANK4 == Types::getRank(E4));
        REQUIRE(RANK8 == Types::getRank(H8));

        REQUIRE(FILE1 == Types::getFile(A1));
        REQUIRE(FILE5 == Types::getFile(E4));
        REQUIRE(FILE8 == Types::getFile(H8));
    }

    SECTION("piece")
    {
        Piece wpawn = Types::makePiece(WHITE, PAWN);
        REQUIRE(WHITE == Types::getPieceColor(wpawn));
        REQUIRE(PAWN  == Types::getPieceType(wpawn));

        Piece bking = Types::makePiece(BLACK, KING);
        REQUIRE(BLACK == Types::getPieceColor(bking));
        REQUIRE(KING  == Types::getPieceType(bking));
    }

    SECTION("pawnmove")
    {
        REQUIRE(Types::move<WHITE, PawnMove::PUSH, true> (E4) == E5);
        REQUIRE(Types::move<WHITE, PawnMove::PUSH, false>(E5) == E4);

        REQUIRE(Types::move<BLACK, PawnMove::PUSH, true> (E5) == E4);
        REQUIRE(Types::move<BLACK, PawnMove::PUSH, false>(E4) == E5);

        REQUIRE(Types::move<WHITE, PawnMove::RIGHT, true> (D4) == E5);
        REQUIRE(Types::move<WHITE, PawnMove::RIGHT, false>(E5) == D4);

        REQUIRE(Types::move<WHITE, PawnMove::LEFT, true> (E4) == D5);
        REQUIRE(Types::move<WHITE, PawnMove::LEFT, false>(D5) == E4);
    }

}