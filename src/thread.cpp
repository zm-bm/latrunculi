#include "thread.hpp"

Thread::Thread(int id, UCIOutput& uciOutput, ThreadPool* threadPool)
    : threadId(id), uciOutput(uciOutput), threadPool(threadPool), thread(&Thread::loop, this) {
    board.setThread(this);
}

Thread::~Thread() {
    stop();
    if (thread.joinable()) {
        thread.join();
    }
}

void Thread::start() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        runSignal = true;
    }
    condition.notify_all();
}

void Thread::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        exitSignal = true;
    }
    condition.notify_all();
}

void Thread::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [&] { return !runSignal; });
}

void Thread::set(SearchOptions& options, TimePoint startTime) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        board.loadFEN(options.fen);
        this->stats.debug = options.debug;
        this->options     = options;
        this->startTime   = startTime;
    }
}

void Thread::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]() { return runSignal || exitSignal; });

            if (exitSignal) return;
            runSignal = false;
        }

        if (!exitSignal) {
            search();
            condition.notify_all();
        }
    }
}
