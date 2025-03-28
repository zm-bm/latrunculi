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
        runThread = true;
    }
    condition.notify_one();
}

void Thread::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        exitThread = true;
    }
    condition.notify_one();
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
            condition.wait(lock, [this]() { return runThread || exitThread; });

            if (exitThread) return;
            runThread = false;
        }

        if (!exitThread) {
            search();
        }
    }
}

void Thread::reset() {
    stats.reset();
    ply = 0;
    pv.clear();
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<Thread>(i + 1));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(Board& board, SearchOptions& options) {
    stopThreads = false;
    for (auto& searchThread : searchThreads) {
        searchThread->set(board.toFEN(), options);
        searchThread->start();
    }
}

void ThreadPool::stopAll() { stopThreads = true; }
