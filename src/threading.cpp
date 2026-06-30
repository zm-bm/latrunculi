#include "threading.hpp"

#include <algorithm>
#include <cassert>

#include "tt.hpp"

Thread::Thread(int id, uci::Protocol& protocol, ThreadPool& pool)
    : worker(id, protocol, pool),
      native_thread(&Thread::idle_loop, this) {}

Thread::~Thread() {
    shutdown();
    if (native_thread.joinable()) {
        native_thread.join();
    }
}

// ThreadPool-facing lifecycle.
void Thread::start_search(const SearchOptions& options) {
    configure_search(options);
    wake_for_search();
}

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
void Thread::idle_loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lk(state_mutex);
            state_cv.wait(lk, [&]() { return searching || shutdown_requested; });
            if (shutdown_requested)
                break;
        }
        worker_running.store(true, std::memory_order_release);
        worker.search();
        {
            std::lock_guard<std::mutex> lk(state_mutex);
            searching = false;
            worker_running.store(false, std::memory_order_release);
        }
        state_cv.notify_all();
    }
}

void Thread::configure_search(const SearchOptions& options) {
    std::lock_guard<std::mutex> lock(state_mutex);

    assert(!searching);
    assert(!shutdown_requested);

    worker.configure_search(options);
    worker_running.store(false, std::memory_order_release);
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

ThreadPool::ThreadPool(size_t thread_count, uci::Protocol& protocol) : protocol(protocol) {
    for (size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back(new Thread(static_cast<int>(i), protocol, *this));
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

// Search lifecycle.
bool ThreadPool::start_search(const SearchOptions& options) {
    if (shutdown_requested || threads.empty() || is_searching())
        return false;

    tt.age_table();

    // Close the gate so helpers wait for the main worker.
    close_helper_gate();

    for (auto& thread : threads) {
        thread->start_search(options);
    }

    return true;
}

void ThreadPool::request_stop() {
    for (auto& thread : threads) {
        thread->request_stop();
    }

    // Release the gate in case helpers are waiting.
    release_helper_searches();
}

void ThreadPool::wait() {
    for (auto& thread : threads) {
        thread->wait_for_idle();
    }
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
        for (size_t i = threads.size(); i < thread_count; ++i) {
            threads.emplace_back(new Thread(static_cast<int>(i), protocol, *this));
        }
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

uint64_t ThreadPool::nodes_searched() const {
    uint64_t total = 0;
    for (const auto& thread : threads) {
        total += thread->worker.node_count();
    }
    return total;
}

std::vector<RootLine> ThreadPool::root_lines() const {
    std::vector<RootLine> lines;
    lines.reserve(threads.size());
    for (const auto& thread : threads) {
        lines.push_back(thread->worker.root_snapshot());
    }
    return lines;
}

// Helper release gate control.
void ThreadPool::close_helper_gate() {
    std::lock_guard<std::mutex> lock(helper_gate_mutex);
    helper_gate_open = false;
}

void ThreadPool::release_helper_searches() {
    {
        std::lock_guard<std::mutex> lock(helper_gate_mutex);
        helper_gate_open = true;
    }
    helper_gate_cv.notify_all();
}

void ThreadPool::wait_for_helper_release() {
    std::unique_lock<std::mutex> lock(helper_gate_mutex);
    helper_gate_cv.wait(lock, [&] { return helper_gate_open; });
}

// Helper worker control.
void ThreadPool::stop_helper_searches() {
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->request_stop();
    }
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->wait_for_idle();
    }
}

// Diagnostics.
SearchInstrumentation<> ThreadPool::aggregate_instrumentation() const {
    SearchInstrumentation<> total{};
    for (const auto& thread : threads) {
        total += thread->worker.stats;
    }
    return total;
}
