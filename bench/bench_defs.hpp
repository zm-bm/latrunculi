#pragma once

#include <climits>
#include <ostream>
#include <sstream>
#include <string>

#include "board.hpp"
#include "movegen.hpp"

struct TestCase {
    std::string fen;
    std::string best_move;
    std::string avoid_move;
    std::string test_string;

    TestCase(std::string line);
};

struct UCIInfo {
    int         depth = 0;
    int         time  = 0;
    uint64_t    nps   = 0;
    std::string first_move;

    UCIInfo(std::string line);

    friend std::ostream& operator<<(std::ostream& os, const UCIInfo& info);
};

struct TestResult {
    bool     success   = false;
    int      max_depth = 0;
    int      sol_depth = INT_MAX;
    int      max_time  = 0;
    int      sol_time  = INT_MAX;
    uint64_t nps       = 0;
    TestCase test_case;

    TestResult(TestCase tc) : test_case(tc) {}

    std::string get_engine_move(std::string fen, std::string move);
    void        update(const UCIInfo& info);

    friend std::ostream& operator<<(std::ostream& os, const TestResult& result);
};

inline TestCase::TestCase(std::string line) {
    std::string token;

    auto pos = line.find(';');
    if (pos != std::string::npos) {
        test_string = line.substr(0, pos);
    } else {
        test_string = line;
    }

    std::istringstream iss(test_string);
    for (int i = 0; i < 4; ++i) {
        iss >> token;
        fen += token + " ";
    }
    fen.pop_back();

    while (iss >> token) {
        if (token == "bm")
            iss >> best_move;
        if (token == "am")
            iss >> avoid_move;
    }
}

inline UCIInfo::UCIInfo(std::string line) {
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
                first_move = token;
        }
    }
}

inline std::ostream& operator<<(std::ostream& os, const UCIInfo& info) {
    os << "info: ";
    os << "depth = " << info.depth << ", ";
    os << "time = " << info.time << ", ";
    os << "nps = " << info.nps << ", ";
    os << "pv = " << info.first_move << "\n";
    return os;
}

inline std::string TestResult::get_engine_move(std::string fen, std::string move) {
    Board    board(fen);
    MoveList movelist = generate<ALL_MOVES>(board);

    auto moveMatches = [&](Move m) { return m.str() == move; };
    auto movePtr     = std::find_if(movelist.begin(), movelist.end(), moveMatches);

    return board.toSAN(*movePtr);
}

inline void TestResult::update(const UCIInfo& info) {
    std::string engineMove = get_engine_move(test_case.fen, info.first_move);

    success = ((test_case.best_move.empty() || engineMove == test_case.best_move) &&
               (test_case.avoid_move.empty() || engineMove != test_case.avoid_move));

    max_depth = std::max(max_depth, info.depth);
    sol_depth = success ? std::min(info.depth, sol_depth) : INT_MAX;
    max_time  = std::max(max_time, info.time);
    sol_time  = success ? std::min(info.time, sol_time) : INT_MAX;
    nps       = info.nps;
}

inline std::ostream& operator<<(std::ostream& os, const TestResult& result) {
    os << (result.success ? "Pass: " : "Fail: ");
    os << "depth " << result.max_depth << " ";
    os << "time " << result.max_time << " ";
    os << "nps " << result.nps << " ";
    os << "fen " << result.test_case.test_string;
    return os;
}