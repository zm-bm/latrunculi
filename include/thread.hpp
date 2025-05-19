#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "board.hpp"
#include "constants.hpp"
#include "heuristics.hpp"
#include "pv.hpp"
#include "search_options.hpp"
#include "search_stats.hpp"
#include "thread_pool.hpp"
#include "types.hpp"
#include "uci_output.hpp"

class Thread {
   public:
    Thread() = delete;
    Thread(int, UCIOutput&, ThreadPool* threadPool = nullptr);
    ~Thread();

    void start();
    void stop();
    void haltSearch();
    void wait();
    void set(SearchOptions&, TimePoint startTime);

   private:
    // search state
    Board board;
    PrincipalVariation pv;
    Heuristics heuristics;
    SearchStats stats;
    SearchOptions options;
    TimePoint startTime;
    int ply;

    // thread state
    std::mutex mutex;
    std::condition_variable condition;
    bool stopSignal{false};
    bool runSignal{false};
    std::atomic<bool> haltSearchSignal{false};
    const int threadId;

    UCIOutput& uciOutput;
    ThreadPool* threadPool{nullptr};
    std::thread thread;

    // main thread loop
    void loop();

    // search.cpp
    int search();
    template <NodeType = NodeType::Root>
    int alphabeta(int, int, int);
    int quiescence(int, int);

    // helpers
    SearchStats getStats() const;
    Milliseconds getElapsedTime() const;
    bool isTimeUp() const;
    bool isMainThread() const;
    bool isHaltingSearch() const;

    friend class SearchTest;
    friend class SearchBenchmark;

    friend class ThreadPool;
    friend class MovePriority;
    friend class Board;
};

inline SearchStats Thread::getStats() const {
    return threadPool == nullptr ? this->stats : threadPool->getStats();
}

inline Milliseconds Thread::getElapsedTime() const {
    return std::chrono::duration_cast<Milliseconds>(Clock::now() - startTime);
};

inline bool Thread::isTimeUp() const { return getElapsedTime().count() > options.movetime; }
inline bool Thread::isMainThread() const { return threadId == 0; }
inline bool Thread::isHaltingSearch() const { return haltSearchSignal || stopSignal; }
