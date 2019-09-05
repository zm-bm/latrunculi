#include "catch.hpp"
#include "globals.hpp"
#include "board.hpp"
#include "search.hpp"

void recursiveZobristCheck(Board board, int depth)
{
    if (depth < 0)
        return;

    auto gen  = MoveGen::Generator(&board);
    gen.run();

    for (auto& move : gen.moves)
    {
        if (!board.isLegalMove(move))
            continue;

        board.make(move);
        REQUIRE(board.calculateKey() == board.getKey());
        recursiveZobristCheck(board, depth-1);
        board.unmake();
    }
};

TEST_CASE( "Zobrist hashing", "[zobrist]" )
{
    G::init();

    SECTION("Check starting hash validity")
    {
        auto board = Board(G::STARTFEN);
        REQUIRE(board.calculateKey() == board.getKey());
        board = Board(G::KIWIPETE);
        REQUIRE(board.calculateKey() == board.getKey());
    }

    SECTION("Check hash validity after making moves")
    {
        // auto board = Board(G::STARTFEN);
        // recursiveZobristCheck(board, 5);
        // board = Board(G::KIWIPETE);
        // recursiveZobristCheck(board, 3);
        auto board = Board(G::KIWIPETE);
        recursiveZobristCheck(board, 2);
    }
}
