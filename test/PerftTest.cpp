#include <memory>
#include "catch.hpp"
#include "globals.hpp"
#include "board.hpp"
#include "search.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

TEST_CASE( "Test perft results - Start position", "[perft]" )
{
    G::init();

    auto board = Board(G::STARTFEN);
    Search search(&board);

    SECTION("Start position - Perft 1")
    {
        REQUIRE(search.perft<true>(1) == 20);
    }

    SECTION("Start position - Perft 2")
    {
        REQUIRE(search.perft<true>(2) == 400);
    }

    SECTION("Start position - Perft 3")
    {
        REQUIRE(search.perft<true>(3) == 8902);
    }

    SECTION("Start position - Perft 4")
    {
        REQUIRE(search.perft<true>(4) == 197281);
    }

    // SECTION("Start position - Perft 5")
    // {
    //     REQUIRE(search.perft<true>(5) == 4865609);
    // }
    
    // SECTION("Start position - Perft 6")
    // {
    //     REQUIRE(search.perft<true>(6) == 119060324);
    // }
}

TEST_CASE( "Test perft results - Kiwipete", "[perft]" )
{
    G::init();

    auto board = Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
    Search search(&board);

    SECTION("Kiwipete - Perft 1")
    {
        REQUIRE(search.perft<true>(1) == 48);
    }

    SECTION("Kiwipete - Perft 2")
    {
        REQUIRE(search.perft<true>(2) == 2039);
    }

    SECTION("Kiwipete - Perft 3")
    {
        REQUIRE(search.perft<true>(3) == 97862);
    }

    SECTION("Kiwipete - Perft 4")
    {
        REQUIRE(search.perft<true>(4) == 4085603);
    }

    // SECTION("Kiwipete - Perft 5")
    // {
    //     REQUIRE(search.perft<true>(5) == 193690690);
    // }
}

TEST_CASE( "Test perft results - Position 3", "[perft]" )
{
    G::init();

    auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
    Search search(&board);

    SECTION("Position 3 - Perft 1")
    {
        REQUIRE(search.perft<true>(1) == 14);
    }

    SECTION("Position 3 - Perft 2")
    {
        REQUIRE(search.perft<true>(2) == 191);
    }

    SECTION("Position 3 - Perft 3")
    {
        REQUIRE(search.perft<true>(3) == 2812);
    }

    SECTION("Position 3 - Perft 4")
    {
        REQUIRE(search.perft<true>(4) == 43238);
    }

    SECTION("Position 3 - Perft 5")
    {
        REQUIRE(search.perft<true>(5) == 674624);
    }

    // SECTION("Position 3 - Perft 6")
    // {
    //     REQUIRE(search.perft<true>(6) == 11030083);
    // }
}

TEST_CASE( "Test perft results - Position 4", "[perft]" )
{
    G::init();

    auto boardW = Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
    Search searchW(&boardW);

    auto boardB = Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ");
    Search searchB(&boardB);

    SECTION("Position 4 as White - Perft 1")
    {
        REQUIRE(searchW.perft<true>(1) == 6);
    }

    SECTION("Position 4 as Black- Perft 1")
    {
        REQUIRE(searchB.perft<true>(1) == 6);
    }

    SECTION("Position 4 as White - Perft 2")
    {
        REQUIRE(searchW.perft<true>(2) == 264);
    }

    SECTION("Position 4 as Black- Perft 2")
    {
        REQUIRE(searchB.perft<true>(2) == 264);
    }

    SECTION("Position 4 as White - Perft 3")
    {
        REQUIRE(searchW.perft<true>(3) == 9467);
    }

    SECTION("Position 4 as Black- Perft 3")
    {
        REQUIRE(searchB.perft<true>(3) == 9467);
    }

    SECTION("Position 4 as White - Perft 4")
    {
        REQUIRE(searchW.perft<true>(4) == 422333);
    }

    SECTION("Position 4 as Black- Perft 4")
    {
        REQUIRE(searchB.perft<true>(4) == 422333);
    }

    // SECTION("Position 4 as White - Perft 5")
    // {
    //     REQUIRE(searchW.perft<true>(5) == 15833292);
    // }

    // SECTION("Position 4 as Black- Perft 5")
    // {
    //     REQUIRE(searchB.perft<true>(5) == 15833292);
    // }
}

TEST_CASE( "Test perft results - Position 5", "[perft]" )
{
    G::init();
    auto board = Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    Search search(&board);

    SECTION("Position 5 - Perft 1")
    {
        REQUIRE(search.perft<true>(1) == 44);
    }

    SECTION("Position 5 - Perft 2")
    {
        REQUIRE(search.perft<true>(2) == 1486);
    }

    SECTION("Position 5 - Perft 3")
    {
        REQUIRE(search.perft<true>(3) == 62379);
    }

    SECTION("Position 5 - Perft 4")
    {
        REQUIRE(search.perft<true>(4) == 2103487);
    }

    // SECTION("Position 5 - Perft 5")
    // {
    //     REQUIRE(search.perft<true>(5) == 89941194);
    // }
}