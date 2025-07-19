#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "search_options.hpp"
#include "search_stats.hpp"
#include "uci.hpp"

class Thread;

class ThreadPool {
   public:
    ThreadPool() = delete;
    ThreadPool(size_t numThreads, UCIProtocolHandler& uciHandler);
    ~ThreadPool();

    void startAll(SearchOptions&);
    void exitAll();
    void stopAll();
    void waitAll();
    void resize(size_t newSize);

    int size() const;

    template <typename T>
    T accumulate(T Thread::* member) const;

    friend class ThreadTest;
    friend class SearchTest;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
    UCIProtocolHandler& uciHandler;
};
