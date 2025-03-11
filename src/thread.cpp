#include <memory>
#include "thread.hpp"
#include "search.hpp"

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

void Thread::set(const std::string& fen, int depth) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        chess = Chess(fen, this);
        searchDepth = depth;
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

void Thread::search() {
    Search::startTime = std::chrono::high_resolution_clock::now();
    Search::Result result;

    // iterative deepening loop
    for (int depth = 1; depth <= searchDepth; ++depth) {
        result = Search::negamax<true>(*this, -MATESCORE, MATESCORE, depth);
        ageHeuristics();

        std::cout << Search::generateUCILine(depth, result) << std::endl;
    }

    std::cout << "bestmove " << result.pv.at(0) << " " << result.score << std::endl;

    // while (!ThreadPool::stopThreads) {}
}

void Thread::ageHeuristics() {
    history.age();
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<Thread>(i + 1));
    }
}

ThreadPool::~ThreadPool() {
    stopAll();
}

void ThreadPool::startAll(Chess& chess, int depth) {
    stopThreads = false;
    for (auto& searchThread : searchThreads) {
        searchThread->set(chess.toFEN(), depth);
        searchThread->start();
    }
}

void ThreadPool::stopAll() { stopThreads = true; }
