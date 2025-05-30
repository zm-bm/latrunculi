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

    std::string getLastOutput() {
        std::istringstream iss(oss.str());
        std::string line, last;
        while (std::getline(iss, line)) {
            last = line;
        }
        return last;
    }

    std::string extractPV() {
        auto outputStr = getLastOutput();
        std::istringstream iss(outputStr);
        std::string token;
        while (iss >> token) {
            if (token == "pv") {
                std::string pvMove;
                if (iss >> pvMove) {
                    return pvMove;
                }
            }
        }
        return "";
    }

    std::string getEngineMove(std::string fen, std::string move) {
        Board board(fen);
        MoveGenerator<GenType::All> moves{board};

        auto moveMatches = [&](Move m) { return m.str() == move; };
        auto movePtr     = std::find_if(moves.begin(), moves.end(), moveMatches);

        return board.toSAN(*movePtr);
    }

    bool testSearch(std::string& fen, std::string& expectedMove, std::string& avoidMove) {
        // reset the output stream
        oss.str("");
        oss.clear();

        // log the test case
        std::cout << "fen= " << fen << std::endl;
        if (!expectedMove.empty()) std::cout << "bm=" << expectedMove;
        if (!avoidMove.empty()) std::cout << "\tam=" << avoidMove;
        std::cout << std::endl;

        auto start = std::chrono::steady_clock::now();

        engine.execute("stop");
        engine.execute("position fen " + fen);
        engine.execute("go movetime " + MOVETIME);

        std::string output;
        bool foundBestMove = false;
        while (true) {
            output = oss.str();

            std::string pvMove = extractPV();
            if (!pvMove.empty()) {
                auto engineMove = getEngineMove(fen, pvMove);

                if ((expectedMove.empty() || expectedMove == engineMove) &&
                    (avoidMove.empty() || avoidMove != engineMove)) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                    std::cout << "SUCCESS: " << engineMove << " (Time: " << elapsed << " ms)"
                              << std::endl;
                    foundBestMove = true;
                    break;
                }
            }

            if (output.find("bestmove") != std::string::npos) {
                std::string pvMove = extractPV();
                auto engineMove    = getEngineMove(fen, pvMove);
                std::cout << "FAILURE:" << engineMove << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // temp output for debugging
        std::cout << "Output: \n" << output << std::endl;

        return foundBestMove;
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
