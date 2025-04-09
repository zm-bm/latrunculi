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
    bool debug   = true;
    int depth    = 14;
    int movetime = INT32_MAX;
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
    explicit Thread(unsigned int id, std::ostream& output)
        : threadId(id), thread(&Thread::loop, this), output(output) {}
    ~Thread();

    void start();
    void stop();
    void wait();
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
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    std::mutex mutex;
    std::condition_variable condition;
    std::condition_variable condition2;

    std::atomic<bool> exitSignal{false};
    std::atomic<bool> runSignal{false};
    const unsigned int threadId;

    std::ostream& output;
    std::thread thread;

    friend class SearchTests;
    friend class SearchBenchmark;
};

class ThreadPool {
   public:
    ThreadPool(size_t numThreads, std::ostream& output);
    ~ThreadPool();

    void startAll(Board&, SearchOptions&);
    void stopAll();
    void waitAll();

    static inline std::atomic<bool> stopSignal{false};
    friend class SearchBenchmark;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
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
