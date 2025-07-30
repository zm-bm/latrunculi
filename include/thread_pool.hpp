#pragma once

#include <memory>
#include <vector>

#include "search_options.hpp"
#include "thread.hpp"
#include "uci.hpp"

class ThreadPool {
public:
    ThreadPool() = delete;
    ThreadPool(size_t thread_count, uci::Protocol& protocol);
    ~ThreadPool() = default;

    void start_all(SearchOptions&);
    void shutdown_all();
    void halt_all();
    void wait_all();
    void resize(size_t thread_count);

    int size() const;

    template <typename T>
    T accumulate(T Thread::* member) const;

    friend class ThreadTest;
    friend class SearchTest;

private:
    std::vector<std::unique_ptr<Thread>> threads;

    uci::Protocol& protocol;
};
