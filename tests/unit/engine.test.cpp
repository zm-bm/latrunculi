#include "engine.hpp"

#include <sstream>
#include <thread>

#include "gtest/gtest.h"

class EngineTest : public ::testing::Test {
   protected:
    std::ostringstream output;
    Engine engine = Engine(output, std::cin);

    void SetUp() override { engine.out.rdbuf(output.rdbuf()); }

    bool execute(const std::string& command) { return engine.execute(command); }

    bool debug() { return engine.uciOptions.debug; }

    Board& board() { return engine.board; }
};

TEST_F(EngineTest, ExecuteQuitCommand) {
    EXPECT_TRUE(execute("uci"));
    EXPECT_FALSE(execute("quit"));
}

TEST_F(EngineTest, ExecuteIsReadyCommand) {
    EXPECT_TRUE(execute("isready"));
    EXPECT_EQ(output.str(), "readyok\n");
}

TEST_F(EngineTest, ExecuteDebugOnCommand) {
    EXPECT_TRUE(execute("debug on"));
    EXPECT_TRUE(debug());
}

TEST_F(EngineTest, ExecuteDebugOffCommand) {
    execute("debug on");
    EXPECT_TRUE(debug());
    execute("debug off");
    EXPECT_FALSE(debug());
}

TEST_F(EngineTest, ExecutePositionStartPosCommand) {
    EXPECT_TRUE(execute("position startpos"));
    EXPECT_EQ(board().toFEN(), STARTFEN);
}

TEST_F(EngineTest, ExecutePositionStartPosWithMovesCommand) {
    EXPECT_TRUE(execute("position startpos moves e2e4 e7e5"));
    EXPECT_EQ(board().toFEN(), "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
}

TEST_F(EngineTest, ExecutePositionFENCommand) {
    auto position = std::string("position fen ") + EMPTYFEN;
    EXPECT_TRUE(execute(position));
    EXPECT_EQ(board().toFEN(), EMPTYFEN);
}

TEST_F(EngineTest, ExecutePositionFENWithMovesCommand) {
    auto position = std::string("position fen ") + EMPTYFEN + " moves e1e2 e8d8";
    EXPECT_TRUE(execute(position));
    EXPECT_EQ(board().toFEN(), "3k4/8/8/8/8/8/4K3/8 w - - 2 2");
}

TEST_F(EngineTest, ExecuteGoCommandWithDepth) {
    EXPECT_TRUE(execute("go depth 3"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, ExecuteMoveCommand) {
    execute("position startpos");
    EXPECT_TRUE(execute("move e2e4"));
    EXPECT_EQ(board().toFEN(), "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
}

TEST_F(EngineTest, ExecuteUndoMoveCommand) {
    execute("position startpos");
    execute("move e2e4");
    EXPECT_TRUE(execute("move undo"));
    EXPECT_EQ(board().toFEN(), STARTFEN);
}

TEST_F(EngineTest, ExecuteMovesCommand) {
    execute("position startpos");
    EXPECT_TRUE(execute("moves"));
    EXPECT_FALSE(output.str().empty());
}

TEST_F(EngineTest, ExecutePerftCommand) {
    execute("position startpos");
    EXPECT_TRUE(execute("perft 1"));
    EXPECT_NE(output.str().find("NODES: 20"), std::string::npos);
}

TEST_F(EngineTest, ExecuteInvalidCommand) {
    EXPECT_TRUE(execute("invalidcommand"));
    EXPECT_NE(output.str().find("Unknown command"), std::string::npos);
}

TEST_F(EngineTest, GoCommandDepthNegative) {
    EXPECT_TRUE(execute("position startpos"));
    output.str("");
    EXPECT_TRUE(execute("go depth -3"));
    std::string outStr = output.str();
    EXPECT_NE(outStr.find("invalid depth"), std::string::npos);
}

TEST_F(EngineTest, GoCommandMovetimeNegative) {
    EXPECT_TRUE(execute("position startpos"));
    output.str("");
    EXPECT_TRUE(execute("go movetime -1000"));
    std::string outStr = output.str();
    EXPECT_NE(outStr.find("invalid movetime"), std::string::npos);
}

TEST_F(EngineTest, GoCommandDepthNonNumeric) {
    EXPECT_TRUE(execute("position startpos"));
    output.str("");
    EXPECT_TRUE(execute("go depth abc"));
    std::string outStr = output.str();
    EXPECT_NE(outStr.find("invalid depth"), std::string::npos);
}

TEST_F(EngineTest, SetoptionThreadsNegative) {
    output.str("");
    EXPECT_TRUE(execute("setoption name Threads value -1"));
    std::string outStr = output.str();
    EXPECT_NE(outStr.find("invalid Threads"), std::string::npos);
}

TEST_F(EngineTest, SetoptionHashNegative) {
    output.str("");
    EXPECT_TRUE(execute("setoption name Hash value -1"));
    std::string outStr = output.str();
    EXPECT_NE(outStr.find("invalid Hash"), std::string::npos);
}
