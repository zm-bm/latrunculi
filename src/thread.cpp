#include <memory>
#include "thread.hpp"
#include "search.hpp"

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
}

void SearchThread::set(const std::string& fen, int depth) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        chess = Chess(fen);
        searchDepth = depth;
    }
}

void SearchThread::loop() {
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

void SearchThread::search() {
    Search::Result result = Search::negamax<true>(chess, -MATESCORE, MATESCORE, searchDepth);

    Logger logger;
    logger << "info nodes " << Search::nodeCount;
    logger << " score " << result.score << " ";
    for (auto& move : result.pv) {
        logger << move << " ";
    }
    logger << std::endl;
    logger << "bestmove " << result.pv.at(0) << " " << result.score << std::endl;

    // while (!ThreadPool::stopThreads) {}
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<SearchThread>(i + 1));
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
