#include <memory>
#include "thread.hpp"

SearchThread::~SearchThread() {
    stop();
    if (thread.joinable()) {
        thread.join();
    }
}

void SearchThread::start() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        runThread = true;
    }
    condition.notify_one();
}

void SearchThread::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        exitThread = true;
    }
    condition.notify_one();
    Logger() << threadId << " stopping" << std::endl;
}

void SearchThread::set(const std::string& fen) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        chess = Chess(fen);
    }
}

void SearchThread::loop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            Logger() << threadId << " waiting" << std::endl;
            condition.wait(lock, [this]() { return runThread || exitThread; });

            if (exitThread) return;
            runThread = false;
        }

        if (!exitThread) {
            search();
        }
    }
}

void SearchThread::search() {
    while (!ThreadPool::stopThreads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Logger() << threadId << " searching" << std::endl;
    }
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<SearchThread>(i + 1));
    }
}

ThreadPool::~ThreadPool() {
    stopAll();
}

void ThreadPool::startAll(Chess& chess) {
    stopThreads = false;
    for (auto& searchThread : searchThreads) {
        searchThread->set(chess.toFEN());
        searchThread->start();
    }
}

void ThreadPool::stopAll() { stopThreads = true; }
