#pragma once

#include <cassert>
#include <cstddef>
#include <mutex>

#include "search/search_thread_pool.hpp"

class SearchThreadTestAccess {
public:
    static SearchThread& thread(SearchThreadPool& pool, size_t index = 0) {
        assert(index < pool.thread_count());
        return *pool.threads[index];
    }

    static SearchWorker& worker(SearchThread& thread) { return thread.worker; }

    static SearchWorker& worker(SearchThreadPool& pool, size_t index = 0) {
        return worker(thread(pool, index));
    }

    static void
    configure_search(SearchThread& thread, const Board& root_board, SearchLimits limits) {
        thread.configure_search(root_board, limits, SearchClock::now());
    }

    static void start_search(SearchThread& thread, const Board& root_board, SearchLimits limits) {
        configure_search(thread, root_board, limits);
        thread.wake_for_search();
    }

    static void wake_for_search(SearchThread& thread) { thread.wake_for_search(); }

    static void request_stop(SearchThread& thread) { thread.request_stop(); }

    static void wait_for_idle(SearchThread& thread) { thread.wait_for_idle(); }

    static bool state_lock_is_held(SearchThreadPool& pool, size_t index = 0) {
        std::unique_lock<std::mutex> lock(thread(pool, index).state_mutex, std::try_to_lock);
        return !lock.owns_lock();
    }

    static NodeCount node_count(SearchThread& thread) { return worker(thread).node_count(); }

    static NodeCount node_count(SearchThreadPool& pool, size_t index) {
        return node_count(thread(pool, index));
    }
};
