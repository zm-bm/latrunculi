#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "stats.hpp"
#include "types.hpp"

struct SearchOptions {
    bool debug = true;
    int depth  = 14;
};

struct PrincipalVariation {
    using Line = std::vector<Move>;

    std::array<Line, MAX_DEPTH> lines;

    Line& operator[](const int ply) { return lines[ply]; }
    Move bestMove() const { return !lines[0].empty() ? lines[0][0] : NullMove; }

    void update(const int ply, const Move& move) {
        Line& line = lines[ply];
        Line& prev = lines[ply + 1];

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
    int ply;

   private:
    // thread.cpp
    void loop();
    void reset();

    // search.cpp
    void search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    std::mutex mutex;
    std::condition_variable condition;

    std::atomic<bool> exitThread{false};
    std::atomic<bool> runThread{false};
    const unsigned int threadId;

    std::thread thread;

    friend class SearchTest;
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
