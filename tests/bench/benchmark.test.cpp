#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "engine.hpp"
#include "movegen.hpp"
#include "thread.hpp"

using Case = std::tuple<std::string, std::string, std::string>;

class BenchmarkTest : public ::testing::Test {
   private:
    const std::string TESTFILE{"./tests/ccr.epd"};
    const std::string MOVETIME = "10000";  // 10 seconds

    std::vector<Case> cases;

    std::ostringstream oss;
    std::stringstream iss;
    Engine engine{oss, iss};

   protected:
    void SetUp() override {
        std::ifstream file(TESTFILE);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                cases.push_back(buildCase(line));
            }
            file.close();
        }
    }

    Case buildCase(const std::string& line) {
        std::istringstream iss(line);
        std::string fen, bestMove, avoidMove, token;

        for (int i = 0; i < 4; ++i) {
            iss >> token;
            fen += token + " ";
        }
        fen.pop_back();

        while (iss >> token) {
            if (token.rfind("bm", 0) == 0) {
                iss >> bestMove;
                if (bestMove.back() == ';') bestMove.pop_back();
            }
            if (token.rfind("am", 0) == 0) {
                iss >> avoidMove;
                if (avoidMove.back() == ';') avoidMove.pop_back();
            }
        }

        return {fen, bestMove, avoidMove};
    }

    bool testSearch(std::string& fen, std::string& bestMove, std::string& avoidMove) {
        // reset the output stream
        oss.str("");
        oss.clear();

        engine.execute("position fen " + fen);
        engine.execute("go movetime " + MOVETIME);

        // poll output every 10ms until "bestmove " is found
        std::string output;
        while (true) {
            output = oss.str();
            // std::cout << output << std::endl;
            if (output.find("bestmove") != std::string::npos) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::string token, engineMove, engineMoveSAN;
        std::istringstream iss(output);
        while (iss >> token && token != "bestmove");
        iss >> engineMove;

        Board board(fen);
        MoveGenerator<GenType::All> moves{board};
        auto moveMatches = [&](Move m) { return m.str() == engineMove; };
        auto move        = std::find_if(moves.begin(), moves.end(), moveMatches);
        engineMoveSAN    = board.toSAN(*move);

        std::cout << engineMoveSAN;
        ;
        if (!bestMove.empty()) std::cout << "\tbm=" << bestMove;
        if (!avoidMove.empty()) std::cout << "\tam=" << avoidMove;

        if ((bestMove.empty() || bestMove == engineMoveSAN) &&
            (avoidMove.empty() || avoidMove != engineMoveSAN)) {
            std::cout << "\tSUCCESS\n" << std::endl;
            ;
            return true;
        } else {
            std::cout << "\tFAILURE\n" << std::endl;
            ;
            return false;
        }
    }

    void testAll() {
        int passed = 0;

        for (auto& [fen, bestMove, avoidMove] : cases) {
            passed += testSearch(fen, bestMove, avoidMove);
        }

        std::cout << "BenchmarkTest: " << passed << " out of " << cases.size()
                  << " cases passed.\n";
    }
};

TEST_F(BenchmarkTest, ccr) { testAll(); }
