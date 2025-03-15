#include "thread.hpp"

#include <memory>

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
        chess       = Board(fen, this);
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
    reset();
    startTime = std::chrono::high_resolution_clock::now();

    int prevScore = 0;

    // 1. Iterative deepening loop
    for (int depth = 1; depth <= searchDepth && !ThreadPool::stopThreads; ++depth) {
        // 2. Aspiration window from previous score
        int alpha = prevScore - ASP_WINDOW;
        int beta  = prevScore + ASP_WINDOW;

        // 3. First search
        int score = Search::search(*this, alpha, beta, depth);

        // 4. If fail-low or fail-high, re-search with bigger bounds
        if (score <= alpha) {
            alpha = -MATESCORE;
        } else if (score >= beta) {
            beta = MATESCORE;
        }
        score = Search::search(*this, alpha, beta, depth);

        prevScore = score;
        heuristics.age();
        pv.printInfo(score, nodeCount, startTime);
    }

    std::cout << "bestmove " << pv.lines[0].at(0) << std::endl;
}

void Thread::reset() {
    nodeCount    = 0;
    currentDepth = 0;
    pv.clear();
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<Thread>(i + 1));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(Board& chess, int depth) {
    stopThreads = false;
    for (auto& searchThread : searchThreads) {
        searchThread->set(chess.toFEN(), depth);
        searchThread->start();
    }
}

void ThreadPool::stopAll() { stopThreads = true; }
