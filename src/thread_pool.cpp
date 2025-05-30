#include "thread_pool.hpp"

#include "thread.hpp"

ThreadPool::ThreadPool(size_t numThreads, UCIOutput& uciOutput) : uciOutput(uciOutput) {
    for (size_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<Thread>(i, uciOutput, *this));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(SearchOptions& options) {
    TimePoint startTime = Clock::now();

    for (auto& thread : threads) {
        thread->set(options, startTime);
        thread->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2));
    }
}

void ThreadPool::stopAll() {
    for (auto& thread : threads) {
        thread->stop();
    }
}

void ThreadPool::haltAll() {
    for (auto& thread : threads) {
        thread->haltSearch();
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
            threads[i]->stop();
        }
        threads.resize(newSize);
    } else {
        for (size_t i = threads.size(); i < newSize; ++i) {
            threads.push_back(std::make_unique<Thread>(i, uciOutput, *this));
            std::cout << "Thread " << i << " created." << std::endl;
        }
    }
}

int ThreadPool::getNodeCount() const {
    int totalNodes = 0;

    for (const auto& thread : threads) {
        totalNodes += thread->stats.totalNodes;
    }

    return totalNodes;
}

SearchStats<> ThreadPool::getStats() const {
    SearchStats stats;

    for (const auto& thread : threads) {
        stats += thread->stats;
    }

    return stats;
}
