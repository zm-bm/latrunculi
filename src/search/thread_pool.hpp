#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/types.hpp"
#include "search/instrumentation.hpp"
#include "search/limits.hpp"
#include "search/reporter.hpp"
#include "search/root_line.hpp"
#include "search/worker.hpp"

class SearchThreadTestAccess;

namespace search {

class ThreadPool;

// Native thread wrapper. Owns a Worker and OS thread.
class Thread {
public:
    Thread() = delete;
    ~Thread();
    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&)                 = delete;
    Thread& operator=(Thread&&)      = delete;

private:
    Thread(int id, Reporter& reporter, ThreadPool& pool);

    // ThreadPool-facing lifecycle.
    void request_stop();
    void wait_for_idle();
    void shutdown();
    bool is_searching() const;

    Worker worker;

    // Parked thread state. Guarded by state_mutex.
    mutable std::mutex      state_mutex;
    std::condition_variable state_cv;
    bool                    shutdown_requested{false};
    bool                    searching{false};

    std::thread native_thread;

    // Internal state transitions.
    void idle_loop();
    void configure_search(const Board& root_board, Limits limits, TimePoint start_time);
    void wake_for_search();

    friend class ThreadPool;
    friend class ::SearchThreadTestAccess;
};

// External lifecycle and configuration calls must be serialized by the caller.
// Workers may request a stop, and progress may be queried during search.
// Structural observations require no concurrent external resize or destruction.
class ThreadPool {
public:
    ThreadPool() = delete;
    ThreadPool(size_t thread_count, Reporter& reporter);
    ~ThreadPool();
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    // Search lifecycle.
    bool start_search(const Board& root_board, Limits limits);
    void request_stop();
    void leave_pondering() noexcept;
    void wait();
    void clear_search_heuristics();
    void shutdown();

    // Worker configuration.
    bool   resize(size_t thread_count);
    size_t thread_count() const;

    // Search progress and results.
    bool      is_searching() const;
    NodeCount nodes_searched() const;

    friend class Worker;
    friend class ::SearchThreadTestAccess;

private:
    // Search thread 0 is the main worker; others are helpers.
    std::vector<std::unique_ptr<Thread>> threads;

    // Non-owning, lifetime-bound result sink. The caller must keep it alive for
    // the entire lifetime of the pool and its workers.
    Reporter& reporter;

    // Pool lifecycle state.
    bool shutdown_requested{false};

    // Mutable mode for the current search. The accepted Limits retains
    // the initial request; ponderhit only transitions this runtime flag.
    std::atomic<bool> pondering{false};

    // Helper release gate.
    std::mutex              helper_gate_mutex;
    std::condition_variable helper_gate_cv;
    bool                    helper_gate_open{false};

    // Helper release gate control.
    void close_helper_gate();
    void release_helper_searches();
    void wait_for_helper_release();

    // Helper worker control.
    void stop_helper_searches();

    // Ponder lifecycle.
    bool is_pondering() const noexcept;
    void wait_while_pondering() const noexcept;

    // Worker results and diagnostics.
    std::vector<RootLine> root_snapshots() const;
    Instrumentation<>     aggregate_instrumentation() const;
};

} // namespace search
