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

#include "chess.hpp"
#include "constants.hpp"
#include "types.hpp"
#include "search.hpp"

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
    Move killers[2];

    inline void update(Move killer) {
        if (killers[0] != killer) {
            killers[1] = killers[0];
            killers[0] = killer;
        }
    }

    inline bool isKiller(Move move) const { return move == killers[0] || move == killers[1]; }
};

struct PrincipalVariation {
    std::vector<Move> moves;

    void update(const Move& move, const PrincipalVariation& childPV) {
        moves.clear();
        moves.push_back(move);
        moves.insert(moves.end(), childPV.moves.begin(), childPV.moves.end());
    }

    void clear() {
        moves.clear();
    }
};

class Thread {
   public:
    explicit Thread(unsigned int id) : threadId(id), thread(&Thread::loop, this) {}
    ~Thread();

    void start();
    void stop();
    void set(const std::string&, int);

    Chess chess;
    std::array<PrincipalVariation, MAX_DEPTH> pvTable;
    HistoryTable history;
    KillerMoves killers[MAX_DEPTH];

    std::chrono::high_resolution_clock::time_point startTime;
    int searchDepth, currentDepth;
    int nodeCount;

   private:
    void loop();
    void search();
    void ageHeuristics();
    void printPV(int score);

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

    void startAll(Chess&, int);
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
