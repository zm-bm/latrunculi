#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/types.hpp"
#include "search/root_line.hpp"
#include "search/search_instrumentation.hpp"
#include "search/search_limits.hpp"
#include "search/search_reporter.hpp"
#include "search/search_worker.hpp"

class SearchThreadPool;
class SearchThreadTestAccess;

// Native thread wrapper. Owns a SearchWorker and OS thread.
class SearchThread {
public:
    SearchThread() = delete;
    ~SearchThread();
    SearchThread(const SearchThread&)            = delete;
    SearchThread& operator=(const SearchThread&) = delete;
    SearchThread(SearchThread&&)                 = delete;
    SearchThread& operator=(SearchThread&&)      = delete;

private:
    SearchThread(int id, SearchReporter& reporter, SearchThreadPool& pool);

    // SearchThreadPool-facing lifecycle.
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

    std::thread native_thread;

    // Internal state transitions.
    void idle_loop();
    void configure_search(const Board& root_board, SearchLimits limits, TimePoint start_time);
    void wake_for_search();

    friend class SearchThreadPool;
    friend class ::SearchThreadTestAccess;
};

// External lifecycle and configuration calls must be serialized by the caller.
// Workers may request a stop, and progress may be queried during search.
// Structural observations require no concurrent external resize or destruction.
class SearchThreadPool {
public:
    SearchThreadPool() = delete;
    SearchThreadPool(size_t thread_count, SearchReporter& reporter);
    ~SearchThreadPool();
    SearchThreadPool(const SearchThreadPool&)            = delete;
    SearchThreadPool& operator=(const SearchThreadPool&) = delete;
    SearchThreadPool(SearchThreadPool&&)                 = delete;
    SearchThreadPool& operator=(SearchThreadPool&&)      = delete;

    // Search lifecycle.
    bool start_search(const Board& root_board, SearchLimits limits);
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

    friend class SearchWorker;
    friend class ::SearchThreadTestAccess;

private:
    // Search thread 0 is the main worker; others are helpers.
    std::vector<std::unique_ptr<SearchThread>> threads;

    // Non-owning, lifetime-bound result sink. The caller must keep it alive for
    // the entire lifetime of the pool and its workers.
    SearchReporter& reporter;

    // Pool lifecycle state.
    bool shutdown_requested{false};

    // Mutable mode for the current search. The accepted SearchLimits retains
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
    std::vector<RootLine>   root_snapshots() const;
    SearchInstrumentation<> aggregate_instrumentation() const;
};
