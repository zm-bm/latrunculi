#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "board.hpp"
#include "constants.hpp"
#include "search.hpp"
#include "types.hpp"

using TimePoint = std::chrono::high_resolution_clock::time_point;

struct HistoryTable {
    int history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    inline void update(Color c, Square from, Square to, int depth) {
        int bonus = std::clamp(1 << depth, -MAX_HISTORY, MAX_HISTORY);
        history[c][from][to] += bonus - (history[c][from][to] * bonus / MAX_HISTORY);
    }

    inline int get(Color c, Square from, Square to) const { return history[c][from][to]; }

    inline void age() {
        for (int c = 0; c < N_COLORS; ++c)
            for (int from = 0; from < N_SQUARES; ++from)
                for (int to = 0; to < N_SQUARES; ++to)
                    history[c][from][to] >>= 1;  // age history values
    }
};

struct KillerMoves {
    Move killers[MAX_DEPTH][2];

    inline void update(Move killer, int depth) {
        if (killers[depth][0] != killer) {
            killers[depth][1] = killers[depth][0];
            killers[depth][0] = killer;
        }
    }

    inline bool isKiller(Move move, int depth) const {
        return move == killers[depth][0] || move == killers[depth][1];
    }
};

struct Heuristics {
    HistoryTable history;
    KillerMoves killers;

    inline void updateBetaCutoff(Board& chess, Move move, int depth) {
        Square to = move.to();
        if (chess.pieceOn(to) == Piece::NONE) {
            killers.update(move, depth);
            history.update(chess.sideToMove(), move.from(), to, depth);
        }
    }
    inline void age() { history.age(); }
};

struct PrincipalVariation {
    using Line = std::vector<Move>;

    std::array<Line, MAX_DEPTH> lines;

    Line& operator[](int depth) { return lines[depth]; }

    void update(const Move& move, int depth) {
        Line& line = lines[depth];
        Line& prev = lines[depth + 1];

        line.clear();
        line.push_back(move);
        line.insert(line.end(), prev.begin(), prev.end());
    }

    void clear() {
        for (auto& line : lines) {
            line.clear();
        }
    }

    void printInfo(int score, int nodeCount, TimePoint startTime) {
        using namespace std::chrono;
        auto dur = high_resolution_clock::now() - startTime;
        auto sec = duration_cast<duration<double>>(dur).count();
        auto nps = (sec > 0) ? nodeCount / sec : 0;

        std::cout << "info depth " << lines[0].size();
        std::cout << " score cp " << score;
        std::cout << " time " << std::fixed << std::setprecision(1) << sec;
        std::cout << " nodes " << nodeCount;
        std::cout << " nps " << std::setprecision(0) << nps;
        std::cout << " pv";
        for (auto& move : lines[0]) std::cout << " " << move;
        std::cout << std::endl;
    }
};

class Thread {
   public:
    explicit Thread(unsigned int id) : threadId(id), thread(&Thread::loop, this) {}
    ~Thread();

    void start();
    void stop();
    void set(const std::string&, int);

    Board chess;
    PrincipalVariation pv;
    Heuristics heuristics;

    int searchDepth, currentDepth;
    int nodeCount;

   private:
    void loop();
    void search();
    void reset();

    TimePoint startTime;

    std::mutex mutex;
    std::condition_variable condition;

    std::atomic<bool> exitThread{false};
    std::atomic<bool> runThread{false};
    const unsigned int threadId;

    std::thread thread;
};

class ThreadPool {
   public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    void startAll(Board&, int);
    void stopAll();

    static inline std::atomic<bool> stopThreads{false};

   private:
    std::vector<std::unique_ptr<Thread>> searchThreads;
};

class Logger {
   public:
    Logger() { mutex_.lock(); }     // Lock when object is created
    ~Logger() { mutex_.unlock(); }  // Unlock when object is destroyed

    template <typename T>
    Logger& operator<<(const T& value) {
        std::cout << value;
        return *this;
    }

    Logger& operator<<(std::ostream& (*manip)(std::ostream&)) {
        std::cout << manip;  // Handle std::endl, std::flush, etc.
        return *this;
    }

   private:
    static inline std::mutex mutex_;
};
