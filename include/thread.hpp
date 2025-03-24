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

struct SearchOptions {
    bool debug = true;
    int depth  = 14;
};

struct HistoryTable {
    int history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    inline void update(Color c, Square from, Square to, int depth) {
        int bonus             = std::clamp(1 << depth, -MAX_HISTORY, MAX_HISTORY);
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

    inline void updateBetaCutoff(Board& board, Move move, int depth) {
        Square to = move.to();
        if (board.pieceOn(to) == Piece::NONE) {
            killers.update(move, depth);
            history.update(board.sideToMove(), move.from(), to, depth);
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
};

struct SearchStats {
    using TimePoint     = std::chrono::high_resolution_clock::time_point;
    TimePoint startTime = std::chrono::high_resolution_clock::now();
    U64 totalNodes      = 0;
    std::array<U64, MAX_DEPTH> nodes{};
    std::array<U64, MAX_DEPTH> qNodes{};
    std::array<U64, MAX_DEPTH> cutoffs{};
    std::array<U64, MAX_DEPTH> failHighEarly{};
    std::array<U64, MAX_DEPTH> failHighLate{};
    std::array<U64, MAX_DEPTH> ttProbes{};
    std::array<U64, MAX_DEPTH> ttHits{};
    std::array<U64, MAX_DEPTH> ttCutoffs{};

    int maxDepth() const {
        for (int d = MAX_DEPTH - 1; d >= 0; --d) {
            if (nodes[d] > 0 || qNodes[d] > 0) {
                return d;
            }
        }
        return 0;
    }

    void reset() { *this = {}; }

    void resetDepthStats() {
        nodes         = {};
        qNodes        = {};
        cutoffs       = {};
        failHighEarly = {};
        failHighLate  = {};
        ttProbes      = {};
        ttHits        = {};
        ttCutoffs     = {};
    }
};

class Thread {
   public:
    explicit Thread(unsigned int id) : threadId(id), thread(&Thread::loop, this) {}
    ~Thread();

    void start();
    void stop();
    void set(const std::string&, SearchOptions&);

    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    SearchStats stats;
    SearchOptions options;
    int depth;

   private:
    void loop();
    void search();
    void reset();

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

    void startAll(Board&, SearchOptions&);
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
