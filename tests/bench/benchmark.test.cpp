#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "engine.hpp"
#include "eval.hpp"
#include "movegen.hpp"
#include "thread.hpp"

using EPDCases = std::vector<std::tuple<std::string, std::string, std::string>>;

bool debug   = false;
int depth    = 20;
int movetime = 10000;
std::ostringstream oss;
std::istringstream iss;

void parseEPDLine(const std::string& line, EPDCases& cases) {
    std::istringstream iss(line);
    std::string token, fen, bestMove, avoidMove;

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

    cases.emplace_back(fen, bestMove, avoidMove);
}

EPDCases readEPDFile(const std::string& filename) {
    EPDCases cases;

    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            std::string fen, bestMove;
            parseEPDLine(line, cases);
        }
        file.close();
    }

    return cases;
}

class SearchBenchmark : public ::testing::Test {
   private:
    UCIOutput uciOutput{oss};
    ThreadPool threadpool{1, uciOutput};

   protected:
    void testSearch(std::string& fen, std::string& bestMove, std::string& avoidMove) {
        // reset the output stream
        oss.str("");
        oss.clear();

        SearchOptions options{fen, debug, depth, movetime};
        threadpool.startAll(options);
        threadpool.waitAll();
    }
};

TEST_F(SearchBenchmark, ccr) {
    auto filename = "./tests/ccr.epd";
    auto cases    = readEPDFile(filename);
    std::string token, engineMove, engineMoveSAN;

    int successful = 0;
    for (auto& [fen, bestMove, avoidMove] : cases) {
        testSearch(fen, bestMove, avoidMove);

        std::istringstream iss(oss.str());
        while (iss >> token && token != "bestmove");
        iss >> engineMove;

        Board board(fen);
        MoveGenerator<GenType::All> moves{board};
        auto moveMatches = [&](Move m) { return m.str() == engineMove; };
        auto move        = std::find_if(moves.begin(), moves.end(), moveMatches);
        engineMoveSAN    = board.toSAN(*move);

        if ((bestMove.empty() || bestMove == engineMoveSAN) &&
            (avoidMove.empty() || avoidMove != engineMoveSAN)) {
            successful++;
            std::cout << "SUCCESS\t";
        } else {
            std::cout << "FAILURE\t";
        }

        std::cout << engineMoveSAN << '\t';
        if (!bestMove.empty()) std::cout << " bm " << bestMove;
        if (!avoidMove.empty()) std::cout << " am " << avoidMove;
        std::cout << "\n";
    }

    std::cout << successful << " out of " << cases.size() << '\n';
    EXPECT_GE(successful, 0);
}
