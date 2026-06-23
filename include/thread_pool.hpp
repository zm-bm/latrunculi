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

    bool start_all(SearchOptions&);
    void shutdown_all();
    void halt_all();
    void wait_all();
    void halt_helpers();
    void wait_helpers();
    bool resize(size_t thread_count);

    bool is_busy() const;
    int  size() const;

    template <typename T>
    T accumulate(T Thread::*member) const;

    friend class Thread;
    friend class ThreadTest;
    friend class ThreadPoolTest;
    friend class SearchTest;

private:
    std::vector<std::unique_ptr<Thread>> threads;

    uci::Protocol& protocol;

    Move best_voted_move(const Board& board, Move fallback) const;
};
