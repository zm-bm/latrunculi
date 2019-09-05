#include "catch.hpp"
#include "globals.hpp"
#include "board.hpp"
#include "search.hpp"

TEST_CASE( "Board tests", "[board]" )
{
    G::init();

    SECTION("Board to FEN")
    {
        auto board = Board(G::STARTFEN);
        REQUIRE(board.toFEN() == G::STARTFEN);
        board = Board(G::KIWIPETE);
        REQUIRE(board.toFEN() == G::KIWIPETE);
    }

    SECTION("Accessors")
    {
        auto board = Board(G::TESTFEN2);
        REQUIRE(board.sideToMove() == WHITE);
        REQUIRE(!board.canCastle(WHITE));
        REQUIRE(board.canCastle(BLACK));
        REQUIRE(board.getKingSq(WHITE) == G1);
        REQUIRE(board.getKingSq(BLACK) == E8);
        REQUIRE(board.getHmClock() == 0);
        REQUIRE(board.getEnPassant() == INVALID);
        board = Board(G::TESTFEN3);
        REQUIRE(board.sideToMove() == BLACK);
        REQUIRE(board.canCastle(WHITE));
        REQUIRE(!board.canCastle(BLACK));
        board = Board(G::TESTFEN4);
        REQUIRE(board.getHmClock() == 1);
    }
    
    SECTION("Piece counts")
    {
        auto board = Board(G::STARTFEN);
        REQUIRE(board.count<PAWN  >() == 16);
        REQUIRE(board.count<KNIGHT>() == 4);
        REQUIRE(board.count<BISHOP>() == 4);
        REQUIRE(board.count<ROOK  >() == 4);
        REQUIRE(board.count<QUEEN >() == 2);
        REQUIRE(board.count<KING  >() == 2);
    }

}