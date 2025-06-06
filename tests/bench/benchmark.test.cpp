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

struct InfoData {
    int depth = 0;
    int time  = 0;
    U64 nps   = 0;
    std::string pvFirstMove;

    friend std::ostream& operator<<(std::ostream& os, const InfoData& info) {
        std::cout << "info: depth=" << info.depth << ", time=" << info.time << ", nps=" << info.nps
                  << ", pv=" << info.pvFirstMove;
        return os;
    }
};

InfoData parseInfoLine(const std::string& line) {
    InfoData data;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        if (token == "depth") {
            if (iss >> token) {
                data.depth = std::stoi(token);
            }
        } else if (token == "time") {
            if (iss >> token) {
                data.time = std::stoi(token);
            }
        } else if (token == "nps") {
            if (iss >> token) {
                data.nps = std::stoull(token);
            }
        } else if (token == "pv") {
            if (iss >> token) {
                data.pvFirstMove = token;
            }
            break;
        }
    }
    return data;
}

class BenchmarkTest : public ::testing::Test {
   private:
    const std::string TESTFILE{"./tests/ccr.epd"};
    const std::string MOVETIME = "10000";  // in milliseconds
    const int MIN_DEPTH        = 5;

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

    std::string getEngineMove(std::string fen, std::string move) {
        Board board(fen);
        MoveGenerator<GenType::All> moves{board};

        auto moveMatches = [&](Move m) { return m.str() == move; };
        auto movePtr     = std::find_if(moves.begin(), moves.end(), moveMatches);

        return board.toSAN(*movePtr);
    }

    void logCase(const std::string& fen,
                 const std::string& bestMove,
                 const std::string& avoidMove) {
        std::cout << "fen: " << fen << std::endl;
        if (!bestMove.empty()) std::cout << "bm=" << bestMove;
        if (!avoidMove.empty()) std::cout << "\tam=" << avoidMove;
        std::cout << "\n" << std::endl;
    }

    void waitForSearch() {
        std::string output;

        while (true) {
            output = oss.str();
            if (output.find("bestmove") != std::string::npos) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool testSearch(
        std::string& fen, std::string& bestMove, std::string& avoidMove, int& depth, int& time) {
        bool success     = false;
        int successDepth = INT_MAX;
        int successTime  = INT_MAX;

        // reset the output stream and set up the board
        oss.str("");
        oss.clear();

        engine.execute("position fen " + fen);
        engine.execute("go movetime " + MOVETIME);
        waitForSearch();

        std::istringstream lines(oss.str());
        std::string line;

        while (std::getline(lines, line)) {
            if (line.find("info") != std::string::npos) {
                InfoData infoData = parseInfoLine(line);

                if (infoData.pvFirstMove.empty() || infoData.depth < MIN_DEPTH) {
                    continue;
                }

                std::string engineMove = getEngineMove(fen, infoData.pvFirstMove);

                success = ((bestMove.empty() || engineMove == bestMove) &&
                           (avoidMove.empty() || engineMove != avoidMove));
                if (success) {
                    successDepth = std::min(infoData.depth, successDepth);
                    successTime  = std::min(infoData.time, successTime);
                } else {
                    successDepth = INT_MAX;
                    successTime  = INT_MAX;
                }
            }
        }

        depth = successDepth;
        time  = successTime;
        return success;
    }

    void testAll() {
        int successful = 0;
        int totalDepth = 0;
        int totalTime  = 0;

        for (auto& [fen, bestMove, avoidMove] : cases) {
            logCase(fen, bestMove, avoidMove);

            int testDepth = 0;
            int testTime  = 0;
            bool success  = testSearch(fen, bestMove, avoidMove, testDepth, testTime);

            if (success) {
                std::cout << "Test passed: solution found in " << testDepth
                          << " ply, time = " << testTime << " ms" << std::endl;
                totalDepth += testDepth;
                totalTime  += testTime;
                successful++;
            } else {
                std::cout << "Test failed." << std::endl;
            }
        }

        std::cout << "BenchmarkTest: " << successful << " out of " << cases.size()
                  << " cases passed.\n";
        if (successful > 0) {
            std::cout << "Aggregate Metrics: average ply = " << (totalDepth / successful)
                      << ", average time = " << (totalTime / successful) << " ms" << std::endl;
        }
    }
};

TEST_F(BenchmarkTest, ccr) { testAll(); }
