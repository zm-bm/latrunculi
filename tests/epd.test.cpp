#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "thread.hpp"

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
        }
        if (token.rfind("am", 0) == 0) {
            iss >> avoidMove;
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

class EPDTest : public ::testing::Test {
   private:
    bool debug = false;
    int depth  = 12;
    SearchOptions options{debug, depth};
    Thread thread{1};

   protected:
    void testSearch(const std::string& fen, std::string& bestMove, std::string& avoidMove) {
        thread.set(fen, options);
        thread.search();
        auto moveSAN = thread.board.toSAN(thread.pv.bestMove());
        std::cout << moveSAN << " = " << bestMove << '\n';
        // if (bestMove != "") EXPECT_EQ(moveSAN, bestMove) << fen;
    }
};

TEST_F(EPDTest, epdTests) {
    std::string filename = "tests/ccr.epd";
    auto cases           = readEPDFile(filename);

    for (auto& [fen, bestMove, avoidMove] : cases) {
        testSearch(fen, bestMove, avoidMove);
    }
}
