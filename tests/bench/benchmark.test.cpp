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

    bool testSearch(std::string& fen, std::string& bestMove, std::string& avoidMove) {
        // reset the output stream and set up the board
        oss.str("");
        oss.clear();

        // log the test case
        std::cout << "\nfen= " << fen << std::endl;
        if (!bestMove.empty()) std::cout << "bm=" << bestMove;
        if (!avoidMove.empty()) std::cout << "\tam=" << avoidMove;
        std::cout << std::endl;

        engine.execute("position fen " + fen);
        engine.execute("go movetime " + MOVETIME);

        std::string output;
        while (true) {
            output = oss.str();
            if (output.find("bestmove") != std::string::npos) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        bool onlyBest  = !bestMove.empty() && avoidMove.empty();
        bool onlyAvoid = bestMove.empty() && !avoidMove.empty();
        bool bothMoves = !bestMove.empty() && !avoidMove.empty();

        bool success     = false;
        int successDepth = INT_MAX;
        int successTime  = INT_MAX;

        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("info") != std::string::npos) {
                std::istringstream iss(line);
                std::string token;
                InfoData infoData = parseInfoLine(line);
                if (infoData.pvFirstMove.empty()) continue;

                std::string engineMove = getEngineMove(fen, infoData.pvFirstMove);

                std::cout << "info: depth=" << infoData.depth << ", time=" << infoData.time
                          << ", nps=" << infoData.nps << ", pv=" << infoData.pvFirstMove
                          << std::endl;
                if (onlyBest) {
                    if (engineMove == bestMove && infoData.depth > MIN_DEPTH) {
                        std::cout << "SUCCESS: " << engineMove << " (Depth: " << infoData.depth
                                  << ", Time: " << infoData.time << " ms)" << std::endl;
                        success      = true;
                        successDepth = std::min(infoData.depth, successDepth);
                        successTime  = std::min(infoData.time, successTime);
                    } else {
                        std::cout << "FAILURE: Expected " << bestMove << ", got " << engineMove
                                  << " (Depth: " << infoData.depth << ", Time: " << infoData.time
                                  << " ms)" << std::endl;
                        success      = false;
                        successDepth = INT_MAX;
                        successTime  = INT_MAX;
                    }
                } else if (onlyAvoid) {
                    if (engineMove != avoidMove && infoData.depth > MIN_DEPTH) {
                        std::cout << "SUCCESS: Avoided " << avoidMove
                                  << " (Depth: " << infoData.depth << ", Time: " << infoData.time
                                  << " ms)" << std::endl;
                        success      = true;
                        successDepth = std::min(infoData.depth, successDepth);
                        successTime  = std::min(infoData.time, successTime);
                    } else {
                        std::cout << "FAILURE: Did not avoid " << avoidMove
                                  << " (Depth: " << infoData.depth << ", Time: " << infoData.time
                                  << " ms)" << std::endl;
                        success      = false;
                        successDepth = INT_MAX;
                        successTime  = INT_MAX;
                    }
                } else if (bothMoves) {
                    if (engineMove == bestMove && engineMove != avoidMove &&
                        infoData.depth > MIN_DEPTH) {
                        std::cout << "SUCCESS: " << engineMove << " (Depth: " << infoData.depth
                                  << ", Time: " << infoData.time << " ms)" << std::endl;
                        success      = true;
                        successDepth = std::min(infoData.depth, successDepth);
                        successTime  = std::min(infoData.time, successTime);
                    } else {
                        std::cout << "FAILURE: Expected either " << bestMove << " or avoided "
                                  << avoidMove << ", got " << engineMove
                                  << " (Depth: " << infoData.depth << ", Time: " << infoData.time
                                  << " ms)" << std::endl;
                        success      = false;
                        successDepth = INT_MAX;
                        successTime  = INT_MAX;
                    }
                }
                std::cout << "----------------------------------------" << std::endl;
            }
        }

        return success;
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
