#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "thread.hpp"
#include "eval.hpp"

using EPDCases = std::vector<std::tuple<std::string, std::string, std::string>>;

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

std::ostringstream output;

class SearchBenchmark : public ::testing::Test {
   private:
    SearchOptions options{false, 20, 10000};
    ThreadPool pool{1, std::cout};

   protected:
    bool testSearch(const std::string& fen, std::string& bestMove, std::string& avoidMove) {
        Board board{fen};
        pool.startAll(board, options);
        pool.waitAll();

        auto moveSAN = pool.threads[0]->board.toSAN(pool.threads[0]->pv.bestMove());
        if (!bestMove.empty() && moveSAN != bestMove) return false;
        if (!avoidMove.empty() && moveSAN == avoidMove) return false;

        return true;
    }
};

TEST_F(SearchBenchmark, ccr) {
    auto filename = "./tests/ccr.epd";
    auto cases    = readEPDFile(filename);

    int successful = 0;
    for (auto& [fen, bestMove, avoidMove] : cases) {
        bool result = testSearch(fen, bestMove, avoidMove);

        if (result) {
            successful++;
            std::cout << "successful ";
        } else {
            std::cout << "failed ";
        }
        if (!bestMove.empty()) std::cout << "bm " << bestMove;
        if (!avoidMove.empty()) std::cout << "am " << avoidMove;
        std::cout << "\n\n";
    }
    std::cout << successful << " out of " << cases.size() << '\n';
    ASSERT_GE(successful, 0);
}
