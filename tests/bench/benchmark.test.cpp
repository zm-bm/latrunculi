#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "engine.hpp"
#include "movegen.hpp"
#include "thread.hpp"

const std::string TESTFILE = "./tests/arasan20.epd";
const int MOVETIME         = 10000;  // in milliseconds
const int THREADS          = 1;

struct TestCase {
    std::string fen;
    std::string bestMove;
    std::string avoidMove;
    std::string testString;

    friend std::ostream& operator<<(std::ostream& os, const TestCase& c) {
        os << "Case: " << c.testString;
        return os;
    }
};

struct TestResult {
    bool success;
    int maxDepth;
    int solutionDepth;
    int maxTime;
    int solutionTime;
    U64 nps;

    friend std::ostream& operator<<(std::ostream& os, const TestResult& result) {
        if (result.success) {
            os << "Success: ";
            os << "solutionDepth = " << result.solutionDepth << ", ";
            os << "solutionTime = " << result.solutionTime << ", ";
        } else {
            os << "Failure: ";
        }
        os << "maxDepth = " << result.maxDepth << ", ";
        os << "maxTime = " << result.maxTime << ", ";
        os << "nps = " << result.nps;
        return os;
    }
};

struct InfoData {
    int depth = 0;
    int time  = 0;
    U64 nps   = 0;
    std::string pvFirstMove;

    friend std::ostream& operator<<(std::ostream& os, const InfoData& info) {
        os << "info: ";
        os << "depth = " << info.depth << ", ";
        os << "time = " << info.time << ", ";
        os << "nps = " << info.nps << ", ";
        os << "pv = " << info.pvFirstMove << "\n";
        return os;
    }
};

InfoData parseInfoLine(const std::string& line) {
    InfoData data;
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        if (token == "depth") {
            if (iss >> token) data.depth = std::stoi(token);
        } else if (token == "time") {
            if (iss >> token) data.time = std::stoi(token);
        } else if (token == "nps") {
            if (iss >> token) data.nps = std::stoull(token);
        } else if (token == "pv") {
            if (iss >> token) data.pvFirstMove = token;
        }
    }
    return data;
}

TestCase buildCase(const std::string& line) {
    std::string fen, bestMove, avoidMove, testCase, token;

    auto pos = line.find(';');
    if (pos != std::string::npos) {
        testCase = line.substr(0, pos);
    }

    std::istringstream iss(testCase);
    for (int i = 0; i < 4; ++i) {
        iss >> token;
        fen += token + " ";
    }
    fen.pop_back();

    while (iss >> token) {
        if (token == "bm") iss >> bestMove;
        if (token == "am") iss >> avoidMove;
    }

    return {fen, bestMove, avoidMove, testCase};
}

std::string getEngineMove(std::string fen, std::string move) {
    Board board(fen);
    MoveGenerator<GenType::All> moves{board};

    auto moveMatches = [&](Move m) { return m.str() == move; };
    auto movePtr     = std::find_if(moves.begin(), moves.end(), moveMatches);

    return board.toSAN(*movePtr);
}

class BenchmarkTest : public ::testing::Test {
   private:
    const int MIN_DEPTH = 8;

    std::vector<TestCase> testCases;

    std::ostringstream oss;
    std::stringstream iss;
    Engine engine{oss, iss};

   protected:
    void SetUp() override {
        engine.execute("setoption name Threads value " + std::to_string(THREADS));

        std::ifstream file(TESTFILE);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                testCases.push_back(buildCase(line));
            }
            file.close();
        }
    }

    TestResult testSearch(TestCase& testCase) {
        bool success      = false;
        int solutionDepth = INT_MAX;
        int solutionTime  = INT_MAX;
        int maxDepth      = 0;
        int maxTime       = 0;
        U64 nps           = 0;

        // reset the output stream
        oss.str("");
        oss.clear();

        // run the search
        engine.execute("position fen " + testCase.fen);
        engine.execute("go movetime " + std::to_string(MOVETIME));
        while (true) {
            if (oss.str().find("bestmove") != std::string::npos) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // process the output stream line by line
        std::istringstream lines{oss.str()};
        std::string line;
        while (std::getline(lines, line)) {
            // parse line info, ignore irrelevant lines
            if (line.find("info") == std::string::npos) continue;
            InfoData infoData = parseInfoLine(line);
            if (infoData.pvFirstMove.empty() || infoData.depth < MIN_DEPTH) continue;

            // check engine move against the test case
            std::string engineMove = getEngineMove(testCase.fen, infoData.pvFirstMove);
            success = ((testCase.bestMove.empty() || engineMove == testCase.bestMove) &&
                       (testCase.avoidMove.empty() || engineMove != testCase.avoidMove));

            // update results
            solutionDepth = success ? std::min(infoData.depth, solutionDepth) : INT_MAX;
            solutionTime  = success ? std::min(infoData.time, solutionTime) : INT_MAX;
            maxDepth      = std::max(maxDepth, infoData.depth);
            maxTime       = std::max(maxTime, infoData.time);
            nps           = infoData.nps;
        }

        return {success, maxDepth, solutionDepth, maxTime, solutionTime, nps};
    }

    void testAll() {
        int successful = 0;
        std::vector<int> maxDepths, successDepths;
        std::vector<int> maxTimes, successTimes;
        std::vector<U64> maxNps;

        for (auto& testCase : testCases) {
            std::cout << testCase << std::endl;

            TestResult result = testSearch(testCase);
            std::cout << result << std::endl;

            maxDepths.push_back(result.maxDepth);
            maxTimes.push_back(result.maxTime);
            maxNps.push_back(result.nps);

            if (result.success) {
                successful++;
                successDepths.push_back(result.solutionDepth);
                successTimes.push_back(result.solutionTime);
            }
        }

        int avgAllDepth = std::reduce(maxDepths.begin(), maxDepths.end(), 0) / maxDepths.size();
        int avgAllTimes = std::reduce(maxTimes.begin(), maxTimes.end(), 0) / maxTimes.size();
        U64 avgNps      = std::reduce(maxNps.begin(), maxNps.end(), 0ULL) / maxNps.size();

        std::cout << "\nBenchmark Summary: ";
        std::cout << "Threads = " << THREADS << ", ";
        std::cout << "Movetime: " << MOVETIME << " ms" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Cases Passed: " << successful << " out of " << testCases.size() << std::endl;
        std::cout << "Average Depth: " << avgAllDepth << " ply" << std::endl;
        std::cout << "Average Time: " << avgAllTimes << " ms" << std::endl;
        std::cout << "Average NPS: " << avgNps << std::endl;

        if (successful > 0) {
            int avgSuccessDepth =
                std::reduce(successDepths.begin(), successDepths.end(), 0) / successDepths.size();
            int avgSuccessTimes =
                std::reduce(successTimes.begin(), successTimes.end(), 0) / successTimes.size();
            std::cout << "Average successful Depth: " << avgSuccessDepth << " ply" << std::endl;
            std::cout << "Average successful Time: " << avgSuccessTimes << " ms" << std::endl;
        }
    }
};

TEST_F(BenchmarkTest, ccr) { testAll(); }
