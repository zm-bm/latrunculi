#include "thread.hpp"

Thread::Thread(int id, uci::Protocol& protocol, ThreadPool& pool)
    : board(Board::startfen),
      thread_id(id),
      protocol(protocol),
      thread_pool(pool),
      thread(&Thread::loop, this) {
    board.set_thread(this);
}

Thread::~Thread() {
    exit();
    if (thread.joinable()) {
        thread.join();
    }
}

void Thread::start(SearchOptions& search_options) {
    if (run_signal)
        return;
    {
        std::lock_guard<std::mutex> lock(mutex);

        board.load_board(search_options.board);
        this->options       = search_options;
        this->searchtime_ms = options.calc_searchtime_ms(board.side_to_move());

        run_signal  = true;
        stop_signal = false;
    }
    condition.notify_all();
}

void Thread::exit() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop_signal = true;
        exit_signal = true;
    }
    condition.notify_all();
}

void Thread::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop_signal = true;
    }
    condition.notify_all();
}

void Thread::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [&] { return !run_signal; });
}

void Thread::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]() { return run_signal || exit_signal; });

            if (exit_signal) {
                run_signal = false;
                condition.notify_all();
                return;
            }
        }

        if (!exit_signal) {
            search();
            run_signal  = false;
            stop_signal = false;
            condition.notify_all();
        }
    }
}
