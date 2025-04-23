#include "thread.hpp"

#include <memory>

#include "uci.hpp"

Thread::Thread(int id, UCI::Engine* engine)
    : threadId(id), thread(&Thread::loop, this), engine(engine) {}

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

bool Thread::isMainThread() { return threadId == 0; }

ThreadPool::ThreadPool(size_t numThreads, UCI::Engine* engine) {
    for (size_t i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<Thread>(i, engine));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(Board& board, SearchOptions& options) {
    stopSignal = false;
    for (auto& thread : threads) {
        thread->set(board.toFEN(), options);
        thread->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2));
    }
}

void ThreadPool::stopAll() { stopSignal = true; }

void ThreadPool::waitAll() {
    for (auto& thread : threads) {
        thread->wait();
    }
}
