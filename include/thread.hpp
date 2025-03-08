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
#include "types.hpp"

struct HistoryTable {
    int history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    inline void update(Color c, Square from, Square to, int depth) {
        history[c][from][to] += (1 << depth);
    }

    inline int get(Color c, Square from, Square to) const {
        return history[c][from][to];
    }
};

class SearchThread {
   public:
    explicit SearchThread(unsigned int id) : threadId(id), thread(&SearchThread::loop, this) {}
    ~SearchThread();

    void start();
    void stop();
    void set(const std::string&, int);

    Chess chess{STARTFEN};
    HistoryTable history;
    int searchDepth;

   private:
    void loop();
    void search();

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
    std::vector<std::unique_ptr<SearchThread>> searchThreads;
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
