#pragma once

#include <cassert>
#include <cstddef>
#include <mutex>

#include "uci/threading.hpp"

class ThreadTestAccess {
public:
    static Thread& thread(ThreadPool& pool, size_t index = 0) {
        assert(index < pool.thread_count());
        return *pool.threads[index];
    }

    static SearchWorker& worker(Thread& thread) { return thread.worker; }

    static SearchWorker& worker(ThreadPool& pool, size_t index = 0) {
        return worker(thread(pool, index));
    }

    static void configure_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        thread.configure_search(root_board, limits, SearchClock::now());
    }

    static void start_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        configure_search(thread, root_board, limits);
        thread.wake_for_search();
    }

    static void wake_for_search(Thread& thread) { thread.wake_for_search(); }

    static void request_stop(Thread& thread) { thread.request_stop(); }

    static void wait_for_idle(Thread& thread) { thread.wait_for_idle(); }

    static bool state_lock_is_held(ThreadPool& pool, size_t index = 0) {
        std::unique_lock<std::mutex> lock(thread(pool, index).state_mutex, std::try_to_lock);
        return !lock.owns_lock();
    }

    static NodeCount node_count(Thread& thread) { return worker(thread).node_count(); }

    static NodeCount node_count(ThreadPool& pool, size_t index) {
        return node_count(thread(pool, index));
    }
};
