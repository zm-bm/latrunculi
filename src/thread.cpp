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
        stopSignal = true;
    }
    condition.notify_all();
}

void Thread::haltSearch() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        haltSearchSignal = true;
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
        this->options   = options;
        this->startTime = startTime;
    }
}

void Thread::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]() { return runSignal || stopSignal; });

            if (stopSignal) return;
        }

        if (!stopSignal) {
            search();
            runSignal        = false;
            haltSearchSignal = false;
            condition.notify_all();
        }
    }
}
