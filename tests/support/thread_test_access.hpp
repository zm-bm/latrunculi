#pragma once

#include <atomic>
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

    static const Thread& thread(const ThreadPool& pool, size_t index = 0) {
        assert(index < pool.thread_count());
        return *pool.threads[index];
    }

    static SearchWorker& worker(Thread& thread) { return thread.worker; }

    static const SearchWorker& worker(const Thread& thread) { return thread.worker; }

    static SearchWorker& worker(ThreadPool& pool, size_t index = 0) {
        return worker(thread(pool, index));
    }

    static const SearchWorker& worker(const ThreadPool& pool, size_t index = 0) {
        return worker(thread(pool, index));
    }

    static bool worker_running(const Thread& thread) {
        return thread.worker_running.load(std::memory_order_acquire);
    }

    static bool worker_running(const ThreadPool& pool) {
        for (size_t index = 0; index < pool.thread_count(); ++index) {
            if (worker_running(thread(pool, index)))
                return true;
        }
        return false;
    }

    static void start_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        thread.start_search(root_board, limits, SearchClock::now());
    }

    static void request_stop(Thread& thread) { thread.request_stop(); }

    static void wait_for_idle(Thread& thread) { thread.wait_for_idle(); }

    static void shutdown(Thread& thread) { thread.shutdown(); }

    static void configure_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        thread.configure_search(root_board, limits, SearchClock::now());
    }

    static void reset_search_state(Thread& thread) { worker(thread).reset_search_state(); }

    static int ply(const Thread& thread) { return worker(thread).ply; }

    static NodeCount node_count(const Thread& thread) { return worker(thread).node_count(); }

    static NodeCount node_count(const ThreadPool& pool, size_t index) {
        return node_count(thread(pool, index));
    }

    static bool is_draw(const Thread& thread) {
        const SearchWorker& search = worker(thread);
        return search.board.is_draw(search.ply);
    }

    static void make(Thread& thread, Move move) {
        SearchWorker& search = worker(thread);
        search.board.make(move, search.ply_states.child(search.ply));
        ++search.ply;
    }

    static void unmake(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.unmake(search.ply_states.parent(search.ply));
        --search.ply;
    }

    static void make_null(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.make_null(search.ply_states.child(search.ply));
        ++search.ply;
    }

    static void unmake_null(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.unmake_null(search.ply_states.parent(search.ply));
        --search.ply;
    }

    static RootLine root_snapshot(const Thread& thread) { return worker(thread).root_snapshot(); }

    static RootLine root_snapshot(const ThreadPool& pool, size_t index) {
        return root_snapshot(thread(pool, index));
    }
};
