#include "thread_pool.hpp"

#include "search_stats.hpp"
#include "thread.hpp"

ThreadPool::ThreadPool(size_t numThreads, UCIProtocolHandler& uciHandler) : uciHandler(uciHandler) {
    for (size_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<Thread>(i, uciHandler, *this));
    }
}

ThreadPool::~ThreadPool() { exitAll(); }

void ThreadPool::startAll(SearchOptions& options) {
    TimePoint startTime = Clock::now();

    for (auto& thread : threads) {
        thread->set(options, startTime);
        thread->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2));
    }
}

void ThreadPool::exitAll() {
    for (auto& thread : threads) {
        thread->exit();
    }
}

void ThreadPool::stopAll() {
    for (auto& thread : threads) {
        thread->stop();
    }
}

void ThreadPool::waitAll() {
    for (auto& thread : threads) {
        thread->wait();
    }
}

void ThreadPool::resize(size_t newSize) {
    if (newSize == threads.size()) return;

    if (newSize < threads.size()) {
        for (size_t i = newSize; i < threads.size(); ++i) {
            threads[i]->exit();
        }
        threads.resize(newSize);
    } else {
        for (size_t i = threads.size(); i < newSize; ++i) {
            threads.push_back(std::make_unique<Thread>(i, uciHandler, *this));
        }
    }
}

void ThreadPool::age() const {
    for (const auto& thread : threads) {
        thread->heuristics.age();
    }
}

int ThreadPool::size() const { return threads.size(); }

template <typename T>
T ThreadPool::accumulate(T Thread::* member) const {
    T total = T{};
    for (const auto& thread : threads) {
        total += (*thread).*member;
    }
    return total;
}

// intantiate accumulate for nodes and stats
template U64 ThreadPool::accumulate<U64>(U64 Thread::* member) const;
template SearchStats<> ThreadPool::accumulate<SearchStats<>>(SearchStats<> Thread::* member) const;
