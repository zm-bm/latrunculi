#include "thread.hpp"

#include <memory>

#include "search.hpp"
#include "uci.hpp"

constexpr int AspirationWindow = 33;

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
        board         = Board(fen, this);
        this->options = options;
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

    // 1. Iterative deepening loop
    int prevScore = 0;
    for (int searchDepth = 1; searchDepth <= options.depth && !ThreadPool::stopThreads;
         ++searchDepth) {
        // 2. Aspiration window from previous score
        int alpha = prevScore - AspirationWindow;
        int beta  = prevScore + AspirationWindow;

        // 3. First search
        int score = Search::search(*this, alpha, beta, searchDepth);

        // 4. If fail-low or fail-high, re-search with bigger bounds
        if (score <= alpha) {
            alpha = -MATE_VALUE;
            score = Search::search(*this, alpha, beta, searchDepth);
        } else if (score >= beta) {
            beta  = MATE_VALUE;
            score = Search::search(*this, alpha, beta, searchDepth);
        }

        prevScore = score;
        heuristics.age();

        UCI::printInfo(score, searchDepth, stats, pv);
    }

    std::cout << "bestmove " << pv[0].at(0) << std::endl;
    if (options.debug) UCI::printDebuggingInfo(stats);
}

void Thread::reset() {
    stats.reset();
    depth = 0;
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
