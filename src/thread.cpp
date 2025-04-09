#include "thread.hpp"

#include <memory>

#include "uci.hpp"

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
    condition.wait(lock, [&]{ return !runSignal; });
}

void Thread::set(const std::string& fen, SearchOptions& options) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        board             = Board(fen, this);
        this->options     = options;
        this->stats.debug = options.debug;
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

void Thread::reset() {
    stats.reset();
    ply = 0;
    pv.clear();
}

ThreadPool::ThreadPool(size_t numThreads, std::ostream& output) {
    for (size_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<Thread>(i + 1, output));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(Board& board, SearchOptions& options) {
    stopSignal = false;
    for (auto& thread : threads) {
        thread->set(board.toFEN(), options);
        thread->start();
    }
}

void ThreadPool::stopAll() { stopSignal = true; }

void ThreadPool::waitAll() {
    for (auto& thread : threads) {
        thread->wait();
    }
}
