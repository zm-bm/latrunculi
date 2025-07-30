#include "thread.hpp"
#include "thread_pool.hpp"

Thread::Thread(int id, uci::Protocol& protocol, ThreadPool& pool)
    : board(Board::startfen),
      protocol(protocol),
      thread_id(id),
      thread_pool(pool),
      worker(&Thread::loop, this) {
    board.set_thread(this);
}

Thread::~Thread() {
    shutdown();
    if (worker.joinable()) {
        worker.join();
    }
}

void Thread::start(SearchOptions& options) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        set_options(options);
        busy_flag = true;
        run_flag  = true;
        halt_flag.store(false, std::memory_order_relaxed);
    }
    cv.notify_one();
}

void Thread::halt() {
    halt_flag.store(true, std::memory_order_relaxed);
}

void Thread::wait() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        done_cv.wait(lock, [&] { return !busy_flag; });
    }
}

void Thread::shutdown() {
    halt();
    wait();
    {
        std::lock_guard<std::mutex> lk(mutex);
        exit_flag = true;
    }
    cv.notify_one();
}

void Thread::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lk(mutex);
            cv.wait(lk, [&]() { return run_flag || exit_flag; });
            if (exit_flag)
                break;
            run_flag = false;
        }
        search();
        {
            std::lock_guard<std::mutex> lk(mutex);
            busy_flag = false;
        }
        done_cv.notify_all();
    }
}

uint64_t Thread::get_nodes() const {
    return thread_pool.accumulate(&Thread::nodes);
}

void Thread::check_halt_conditions() {
    if (thread_id != 0)
        return;

    if (nodes & 0xFFF)
        return;

    if (options.nodes != OPTION_NOT_SET) {
        auto total_nodes = get_nodes();
        if (total_nodes >= options.nodes)
            thread_pool.halt_all();
    }

    if (searchtime_ms != OPTION_NOT_SET) {
        auto runtime_ms = get_runtime().count();
        if (runtime_ms >= searchtime_ms)
            thread_pool.halt_all();
    }
}
