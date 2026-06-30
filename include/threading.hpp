#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "root_line.hpp"
#include "search_instrumentation.hpp"
#include "search_options.hpp"
#include "search_worker.hpp"
#include "uci.hpp"

class ThreadPool;
class ThreadTestAccess;

// Native thread wrapper. Owns a SearchWorker and OS thread.
class Thread {
public:
    Thread() = delete;
    ~Thread();

private:
    Thread(int id, uci::Protocol& protocol, ThreadPool& pool);

    // ThreadPool-facing lifecycle.
    void start_search(const SearchOptions& options);
    void request_stop();
    void wait_for_idle();
    void shutdown();
    bool is_searching() const;

    SearchWorker worker;

    // Parked thread state. Guarded by state_mutex.
    mutable std::mutex      state_mutex;
    std::condition_variable state_cv;
    bool                    shutdown_requested{false};
    bool                    searching{false};
    std::atomic<bool>       worker_running{false};

    std::thread native_thread;

    // Internal state transitions.
    void idle_loop();
    void configure_search(const SearchOptions& options);
    void wake_for_search();

    friend class ThreadPool;
    friend class ::ThreadTestAccess;
};

// Engine-facing search service.
class ThreadPool {
public:
    ThreadPool() = delete;
    ThreadPool(size_t thread_count, uci::Protocol& protocol);
    ~ThreadPool();

    // Search lifecycle.
    bool start_search(const SearchOptions&);
    void request_stop();
    void wait();
    void shutdown();

    // Worker configuration.
    bool   resize(size_t thread_count);
    size_t thread_count() const;

    // Search progress and results.
    bool                  is_searching() const;
    uint64_t              nodes_searched() const;
    std::vector<RootLine> root_lines() const;

    friend class SearchWorker;
    friend class ::ThreadTestAccess;

private:
    // Worker threads. Thread 0 is the main worker; others are helpers.
    std::vector<std::unique_ptr<Thread>> threads;

    // Output sink.
    uci::Protocol& protocol;

    // Pool lifecycle state.
    bool shutdown_requested{false};

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

    // Diagnostics.
    SearchInstrumentation<> aggregate_instrumentation() const;
};
