#include "search/thread_pool.hpp"

#include <algorithm>
#include <cassert>

namespace search {

Thread::Thread(int id, Reporter& reporter, ThreadPool& pool)
    : worker(id, reporter, pool),
      native_thread(&Thread::idle_loop, this) {}

Thread::~Thread() {
    shutdown();
    if (native_thread.joinable()) {
        native_thread.join();
    }
}

// ThreadPool-facing lifecycle.
void Thread::request_stop() {
    worker.request_stop();
}

void Thread::wait_for_idle() {
    {
        std::unique_lock<std::mutex> lock(state_mutex);
        state_cv.wait(lock, [&] { return !searching; });
    }
}

void Thread::shutdown() {
    request_stop();
    wait_for_idle();
    {
        std::lock_guard<std::mutex> lk(state_mutex);
        shutdown_requested = true;
    }
    state_cv.notify_one();
}

bool Thread::is_searching() const {
    std::lock_guard<std::mutex> lock(state_mutex);
    return searching;
}

// Internal state transitions.
// Unexpected search failures are intentionally fatal. Let exceptions escape
// the std::thread entry and invoke std::terminate rather than leave a partial
// engine alive.
void Thread::idle_loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&]() { return searching || shutdown_requested; });
            if (shutdown_requested)
                break;
        }
        worker.search();
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            // Keep final publication and the idle transition atomic for
            // lifecycle observers.
            worker.publish_final_result();
            searching = false;
        }
        state_cv.notify_all();
    }
}

void Thread::configure_search(const Board& root_board, Limits limits, TimePoint start_time) {
    std::lock_guard<std::mutex> lock(state_mutex);

    assert(!searching);
    assert(!shutdown_requested);

    worker.configure_search(root_board, limits, start_time);
}

void Thread::wake_for_search() {
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        assert(!searching);
        assert(!shutdown_requested);

        searching = true;
    }
    state_cv.notify_one();
}

ThreadPool::ThreadPool(size_t thread_count,
                       Reporter& reporter,
                       const tuning::Parameters& parameters)
    : reporter(reporter),
      parameters(parameters) {
    threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        threads.push_back(
            std::unique_ptr<Thread>{new Thread(static_cast<int>(i), reporter, *this)});
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// Search lifecycle.
bool ThreadPool::start_search(const Board& root_board, Limits limits) {
    if (shutdown_requested || threads.empty() || is_searching())
        return false;

    const TimePoint start_time = SearchClock::now();
    pondering.store(limits.ponder, std::memory_order_relaxed);

    for (auto& thread : threads)
        thread->configure_search(root_board, limits, start_time);

    // The main worker prepares the shared TT before waking configured helpers.
    threads.front()->wake_for_search();

    return true;
}

void ThreadPool::request_stop() {
    for (auto& thread : threads) {
        thread->request_stop();
    }

    leave_pondering();
}

void ThreadPool::leave_pondering() noexcept {
    pondering.store(false, std::memory_order_relaxed);
    pondering.notify_all();
}

void ThreadPool::wait() {
    for (auto& thread : threads) {
        thread->wait_for_idle();
    }
}

void ThreadPool::clear_search_heuristics() {
    assert(!is_searching());

    for (auto& thread : threads)
        thread->worker.clear_search_heuristics();
}

bool ThreadPool::settle(Board& position) {
    if (shutdown_requested || threads.empty() || is_searching())
        return false;

    return threads.front()->worker.settle(position);
}

void ThreadPool::shutdown() {
    if (shutdown_requested)
        return;

    shutdown_requested = true;
    request_stop();

    for (auto& thread : threads) {
        thread->shutdown();
    }
}

// Worker configuration.
bool ThreadPool::resize(size_t thread_count) {
    if (shutdown_requested || is_searching())
        return false;

    if (thread_count == threads.size())
        return true;

    if (thread_count < threads.size()) {
        for (size_t i = thread_count; i < threads.size(); ++i) {
            threads[i]->shutdown();
        }
        threads.resize(thread_count);
    } else {
        const size_t installed_count = threads.size();

        threads.reserve(thread_count);

        std::vector<std::unique_ptr<Thread>> additions;
        additions.reserve(thread_count - installed_count);
        for (size_t i = installed_count; i < thread_count; ++i)
            additions.push_back(
                std::unique_ptr<Thread>{new Thread(static_cast<int>(i), reporter, *this)});

        for (auto& thread : additions)
            threads.push_back(std::move(thread));
    }

    return true;
}

size_t ThreadPool::thread_count() const {
    return threads.size();
}

// Search progress and results.
bool ThreadPool::is_searching() const {
    auto is_searching = [](const auto& thread) { return thread->is_searching(); };
    return std::any_of(threads.begin(), threads.end(), is_searching);
}

NodeCount ThreadPool::nodes_searched() const {
    NodeCount total = 0;
    for (const auto& thread : threads) {
        total += thread->worker.node_count();
    }
    return total;
}

RootLine ThreadPool::select_root_result(RootLine fallback) const {
    for (const auto& thread : threads) {
        const RootLine candidate = thread->worker.root_snapshot();
        if (is_better_root_line(candidate, fallback))
            fallback = candidate;
    }
    return fallback;
}

// Helper worker control.
void ThreadPool::start_helper_searches() {
    // Start every configured helper, preserving stops requested before startup.
    for (size_t i = 1; i < threads.size(); ++i)
        threads[i]->wake_for_search();
}

void ThreadPool::stop_helper_searches() {
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->request_stop();
    }
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->wait_for_idle();
    }
}

// Ponder lifecycle.
bool ThreadPool::is_pondering() const noexcept {
    return pondering.load(std::memory_order_relaxed);
}

void ThreadPool::wait_while_pondering() const noexcept {
    pondering.wait(true, std::memory_order_relaxed);
}

// Diagnostics.
Instrumentation<> ThreadPool::aggregate_instrumentation() const {
    Instrumentation<> total{};
    for (const auto& thread : threads) {
        total += thread->worker.stats;
    }
    return total;
}

} // namespace search
