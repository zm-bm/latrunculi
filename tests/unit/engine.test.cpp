#include "engine.hpp"

#include <sstream>
#include <thread>

#include "gtest/gtest.h"
#include "tt.hpp"

class EngineTest : public ::testing::Test {
   protected:
    std::ostringstream output;
    Engine engine{output, std::cin};

    void SetUp() override {
        output.str("");  // Clear output before each test
    }

    bool execute(const std::string& command) { return engine.execute(command); }

    bool debug() { return engine.uciOptions.debug; }
    Board& board() { return engine.board; }
    ThreadPool& threadpool() { return engine.threadpool; }
};

// 'uci' command test
TEST_F(EngineTest, UCI) {
    EXPECT_TRUE(execute("uci"));
    EXPECT_NE(output.str().find("id name Latrunculi"), std::string::npos);
}

// 'debug' command tests
TEST_F(EngineTest, Debug_InitialState) { EXPECT_EQ(debug(), DEFAULT_DEBUG); }

TEST_F(EngineTest, Debug_InvalidCommand) {
    EXPECT_TRUE(execute("debug xyz"));
    EXPECT_EQ(debug(), DEFAULT_DEBUG);
}

TEST_F(EngineTest, Debug_ValidOnCommand) {
    EXPECT_TRUE(execute("debug on"));
    EXPECT_TRUE(debug());
}

TEST_F(EngineTest, Debug_ValidOffCommand) {
    EXPECT_TRUE(execute("debug on"));
    EXPECT_TRUE(debug());

    execute("debug off");
    EXPECT_FALSE(debug());
}

// 'isready' command tests
TEST_F(EngineTest, IsReady) {
    EXPECT_TRUE(execute("isready"));
    EXPECT_EQ(output.str(), "readyok\n");
}

// 'setoption' tests
TEST_F(EngineTest, SetOption_NameThreads_Negative) {
    EXPECT_TRUE(execute("setoption name Threads value -1"));
    EXPECT_EQ(threadpool().size(), DEFAULT_THREADS);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameThreads_Zero) {
    EXPECT_TRUE(execute("setoption name Threads value 0"));
    EXPECT_EQ(threadpool().size(), DEFAULT_THREADS);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameThreads_TooLarge) {
    EXPECT_TRUE(execute("setoption name Threads value " + std::to_string(MAX_THREADS + 1)));
    EXPECT_EQ(threadpool().size(), DEFAULT_THREADS);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameThreads_NaN) {
    EXPECT_TRUE(execute("setoption name Threads value abc"));
    EXPECT_EQ(threadpool().size(), DEFAULT_THREADS);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameThreads_Valid) {
    EXPECT_TRUE(execute("setoption name Threads value 4"));
    EXPECT_EQ(threadpool().size(), 4);
}

TEST_F(EngineTest, SetOption_NameHash_Negative) {
    EXPECT_TRUE(execute("setoption name Hash value -1"));
    EXPECT_EQ(TT::table.size(), DEFAULT_HASH_MB);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameHash_Zero) {
    EXPECT_TRUE(execute("setoption name Hash value 0"));
    EXPECT_EQ(TT::table.size(), DEFAULT_HASH_MB);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameHash_TooLarge) {
    EXPECT_TRUE(execute("setoption name Hash value " + std::to_string(MAX_HASH_MB + 1)));
    EXPECT_EQ(TT::table.size(), DEFAULT_HASH_MB);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameHash_NaN) {
    EXPECT_TRUE(execute("setoption name Hash value abc"));
    EXPECT_EQ(TT::table.size(), DEFAULT_HASH_MB);
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_NameHash_Valid) {
    EXPECT_TRUE(execute("setoption name Hash value 64"));
    EXPECT_EQ(TT::table.size(), 64);
}

TEST_F(EngineTest, SetOption_InvalidName_Missing) {
    EXPECT_TRUE(execute("setoption"));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_InvalidName_NoName) {
    EXPECT_TRUE(execute("setoption abc"));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_InvalidName_NoNameValue) {
    EXPECT_TRUE(execute("setoption name"));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_InvalidName_NoValue) {
    EXPECT_TRUE(execute("setoption name abc"));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

TEST_F(EngineTest, SetOption_InvalidName_WithValue) {
    EXPECT_TRUE(execute("setoption name abc value 10"));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

// 'ucinewgame' command test
TEST_F(EngineTest, UciNewGame) { EXPECT_TRUE(execute("ucinewgame")); }

// 'position' command tests
TEST_F(EngineTest, Position_None) {
    EXPECT_TRUE(execute("position"));
    EXPECT_NE(output.str().find("invalid position"), std::string::npos);
}

TEST_F(EngineTest, Position_Invalid) {
    EXPECT_TRUE(execute("position abc"));
    EXPECT_NE(output.str().find("invalid position"), std::string::npos);
}

TEST_F(EngineTest, Position_StartPos_NoMoves) {
    EXPECT_TRUE(execute("position startpos"));
    EXPECT_EQ(board().toFEN(), STARTFEN);
}

TEST_F(EngineTest, Position_StartPos_EmptyMoves) {
    EXPECT_TRUE(execute("position startpos moves"));
    EXPECT_EQ(board().toFEN(), STARTFEN);
}

TEST_F(EngineTest, Position_StartPos_ValidMoves) {
    EXPECT_TRUE(execute("position startpos moves e2e4 e7e5"));
    EXPECT_EQ(board().toFEN(), "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
}

TEST_F(EngineTest, Position_StartPos_InvalidMoves) {
    EXPECT_TRUE(execute("position startpos moves e7e5 e2e4"));
    EXPECT_EQ(board().toFEN(), STARTFEN);
}

TEST_F(EngineTest, Position_FEN) {
    auto position = std::string("position fen ") + EMPTYFEN;
    EXPECT_TRUE(execute(position));
    EXPECT_EQ(board().toFEN(), EMPTYFEN);
}

TEST_F(EngineTest, Position_FEN_ValidMoves) {
    auto position = std::string("position fen ") + EMPTYFEN + " moves e1e2 e8d8";
    EXPECT_TRUE(execute(position));
    EXPECT_EQ(board().toFEN(), "3k4/8/8/8/8/8/4K3/8 w - - 2 2");
}

// 'go' and 'stop' command tests
TEST_F(EngineTest, GoCommand_Depth_Negative) {
    EXPECT_TRUE(execute("go depth -3"));
    EXPECT_NE(output.str().find("invalid depth"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Depth_Invalid) {
    EXPECT_TRUE(execute("go depth abc"));
    EXPECT_NE(output.str().find("invalid depth"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Depth_Valid) {
    EXPECT_TRUE(execute("go depth 3"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Movetime_Negative) {
    EXPECT_TRUE(execute("go movetime -1000"));
    EXPECT_NE(output.str().find("invalid movetime"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Movetime_Invalid) {
    EXPECT_TRUE(execute("go movetime abc"));
    EXPECT_NE(output.str().find("invalid movetime"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Movetime_Valid) {
    EXPECT_TRUE(execute("go movetime 50"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Nodes_Negative) {
    EXPECT_TRUE(execute("go nodes -10000"));
    EXPECT_NE(output.str().find("invalid nodes"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Nodes_Invalid) {
    EXPECT_TRUE(execute("go nodes abc"));
    EXPECT_NE(output.str().find("invalid nodes"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_Nodes_Valid) {
    EXPECT_TRUE(execute("go nodes 10000"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_WtimeBtime_Valid) {
    EXPECT_TRUE(execute("go wtime 1000 btime 1000"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_WincBinc_Valid) {
    EXPECT_TRUE(execute("go wtime 1000 btime 1000 winc 100 binc 100"));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoCommand_StopCommand) {
    EXPECT_TRUE(execute("go"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(execute("stop"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

// 'ponderhit' command tests
TEST_F(EngineTest, PonderHitCommand) { EXPECT_TRUE(execute("ponderhit")); }

// 'exit' or 'quit' command test
TEST_F(EngineTest, QuitCommand) {
    EXPECT_TRUE(execute("uci"));
    EXPECT_FALSE(execute("quit"));
}

TEST_F(EngineTest, ExitCommand) {
    EXPECT_TRUE(execute("uci"));
    EXPECT_FALSE(execute("exit"));
}

// invalid command tests
TEST_F(EngineTest, ExecuteInvalidCommand) {
    EXPECT_TRUE(execute("invalidcommand"));
    EXPECT_NE(output.str().find("Unknown command"), std::string::npos);
}

// non-UCI command tests
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
