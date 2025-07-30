#include "thread_pool.hpp"

#include "search_stats.hpp"
#include "thread.hpp"

ThreadPool::ThreadPool(size_t thread_count, uci::Protocol& protocol) : protocol(protocol) {
    for (size_t i = 0; i < thread_count; ++i) {
        threads.push_back(std::make_unique<Thread>(i, protocol, *this));
    }
}

void ThreadPool::start_all(SearchOptions& options) {
    for (auto& thread : threads) {
        thread->start(options);
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2));
    }
}

void ThreadPool::shutdown_all() {
    for (auto& thread : threads) {
        thread->shutdown();
    }
}

void ThreadPool::halt_all() {
    for (auto& thread : threads) {
        thread->halt();
    }
}

void ThreadPool::wait_all() {
    for (auto& thread : threads) {
        thread->wait();
    }
}

void ThreadPool::resize(size_t thread_count) {
    if (thread_count == threads.size())
        return;

    if (thread_count < threads.size()) {
        for (size_t i = thread_count; i < threads.size(); ++i) {
            threads[i]->shutdown();
        }
        threads.resize(thread_count);
    } else {
        for (size_t i = threads.size(); i < thread_count; ++i) {
            threads.push_back(std::make_unique<Thread>(i, protocol, *this));
        }
    }
}

int ThreadPool::size() const {
    return threads.size();
}

template <typename T>
T ThreadPool::accumulate(T Thread::* member) const {
    T total = T{};
    for (const auto& thread : threads) {
        total += (*thread).*member;
    }
    return total;
}

template uint64_t      ThreadPool::accumulate<uint64_t>(uint64_t Thread::* member) const;
template SearchStats<> ThreadPool::accumulate<SearchStats<>>(SearchStats<> Thread::* member) const;
