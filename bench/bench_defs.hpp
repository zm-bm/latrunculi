#pragma once

#include <climits>
#include <ostream>
#include <sstream>
#include <string>

#include "board.hpp"
#include "movegen.hpp"

struct TestCase {
    std::string fen;
    std::string bestMove;
    std::string avoidMove;
    std::string testString;

    TestCase(std::string line) {
        std::string token;

        auto pos = line.find(';');
        if (pos != std::string::npos) {
            testString = line.substr(0, pos);
        } else {
            testString = line;
        }

        std::istringstream iss(testString);
        for (int i = 0; i < 4; ++i) {
            iss >> token;
            fen += token + " ";
        }
        fen.pop_back();

        while (iss >> token) {
            if (token == "bm")
                iss >> bestMove;
            if (token == "am")
                iss >> avoidMove;
        }
    }
};

struct UCIInfo {
    int         depth = 0;
    int         time  = 0;
    uint64_t    nps   = 0;
    std::string pvFirstMove;

    UCIInfo(std::string line) {
        std::istringstream iss(line);
        std::string        token;

        while (iss >> token) {
            if (token == "depth") {
                if (iss >> token)
                    depth = std::stoi(token);
            } else if (token == "time") {
                if (iss >> token)
                    time = std::stoi(token);
            } else if (token == "nps") {
                if (iss >> token)
                    nps = std::stoull(token);
            } else if (token == "pv") {
                if (iss >> token)
                    pvFirstMove = token;
            }
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const UCIInfo& info) {
        os << "info: ";
        os << "depth = " << info.depth << ", ";
        os << "time = " << info.time << ", ";
        os << "nps = " << info.nps << ", ";
        os << "pv = " << info.pvFirstMove << "\n";
        return os;
    }
};

struct TestResult {
    bool     success       = false;
    int      maxDepth      = 0;
    int      solutionDepth = INT_MAX;
    int      maxTime       = 0;
    int      solutionTime  = INT_MAX;
    uint64_t nps           = 0;
    TestCase testCase;

    TestResult(TestCase tc) : testCase(tc) {}

    std::string getEngineMove(std::string fen, std::string move) {
        Board    board(fen);
        MoveList movelist = generate<ALL_MOVES>(board);

        auto moveMatches = [&](Move m) { return m.str() == move; };
        auto movePtr     = std::find_if(movelist.begin(), movelist.end(), moveMatches);

        return board.toSAN(*movePtr);
    }

    void update(const UCIInfo& info) {
        std::string engineMove = getEngineMove(testCase.fen, info.pvFirstMove);

        success = ((testCase.bestMove.empty() || engineMove == testCase.bestMove) &&
                   (testCase.avoidMove.empty() || engineMove != testCase.avoidMove));

        maxDepth      = std::max(maxDepth, info.depth);
        solutionDepth = success ? std::min(info.depth, solutionDepth) : INT_MAX;
        maxTime       = std::max(maxTime, info.time);
        solutionTime  = success ? std::min(info.time, solutionTime) : INT_MAX;
        nps           = info.nps;
    }

    friend std::ostream& operator<<(std::ostream& os, const TestResult& result) {
        os << (result.success ? "Pass: " : "Fail: ");
        os << "depth " << result.maxDepth << " ";
        os << "time " << result.maxTime << " ";
        os << "nps " << result.nps << " ";
        os << "fen " << result.testCase.testString;
        return os;
    }
};
