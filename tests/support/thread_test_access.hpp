#pragma once

#include <cassert>
#include <cstddef>

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

    static void start_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        thread.configure_search(root_board, limits, SearchClock::now());
        thread.wake_for_search();
    }

    static void request_stop(Thread& thread) { thread.request_stop(); }

    static void wait_for_idle(Thread& thread) { thread.wait_for_idle(); }

    static void configure_search(Thread& thread, const Board& root_board, SearchLimits limits) {
        thread.configure_search(root_board, limits, SearchClock::now());
    }

    static void reset_search_state(Thread& thread) { worker(thread).reset_search_state(); }

    static int ply(Thread& thread) { return worker(thread).ply; }

    static NodeCount node_count(Thread& thread) { return worker(thread).node_count(); }

    static NodeCount node_count(ThreadPool& pool, size_t index) {
        return node_count(thread(pool, index));
    }

    static MoveOrdering& move_ordering(Thread& thread) { return worker(thread).ordering; }

    static MoveOrdering& move_ordering(ThreadPool& pool, size_t index = 0) {
        return move_ordering(thread(pool, index));
    }

    static bool is_draw(Thread& thread) {
        const SearchWorker& search = worker(thread);
        return search.board.is_draw(search.ply);
    }

    static void make(Thread& thread, Move move) {
        SearchWorker& search = worker(thread);
        search.board.make(move);
        ++search.ply;
    }

    static void unmake(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.unmake();
        --search.ply;
    }

    static void make_null(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.make_null();
        ++search.ply;
    }

    static void unmake_null(Thread& thread) {
        SearchWorker& search = worker(thread);
        search.board.unmake_null();
        --search.ply;
    }
};
