#include "catch.hpp"
#include "globals.hpp"
#include "uci.hpp"

TEST_CASE( "Test UCI", "[uci]")
{
    G::init();
    UCI::Controller controller(std::cin, std::cout);

    SECTION("uci")
    {
        REQUIRE(controller.execute("uci"));
    }

    SECTION("debug")
    {
        REQUIRE(controller.execute("debug on"));
        REQUIRE(controller.execute("debug off"));
    }

    SECTION("position")
    {
        REQUIRE(controller.execute("position fen rnbqkbnr/pp2pppp/3p4/1Bp5/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq -"));
        REQUIRE(controller.execute("position startpos"));
    }

    SECTION("go")
    {
        REQUIRE(controller.execute("go perft 4"));
        REQUIRE(controller.execute("go depth 4"));
    }

    SECTION("move")
    {
        REQUIRE(controller.execute("move d2d4"));
        REQUIRE(controller.execute("move undo"));
    }

    SECTION("moves")
    {
        REQUIRE(controller.execute("moves"));
    }

}