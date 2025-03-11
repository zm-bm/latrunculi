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
    startTime = std::chrono::high_resolution_clock::now();
    nodeCount = 0;
    Search::Result result;

    // iterative deepening loop
    for (int depth = 1; depth <= searchDepth; ++depth) {
        result = Search::negamax<true>(*this, -MATESCORE, MATESCORE, depth);
        ageHeuristics();
        printPV(result, depth);
    }

    std::cout << "bestmove " << result.pv.at(0) << " " << result.score << std::endl;

    // while (!ThreadPool::stopThreads) {}
}

void Thread::ageHeuristics() { history.age(); }

void Thread::printPV(Search::Result result, int depth) {
    auto dur = std::chrono::high_resolution_clock::now() - startTime;
    auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(dur).count();
    auto nps = (sec > 0) ? nodeCount / sec : 0;

    std::cout << "info depth " << depth;
    std::cout << " score cp " << result.score;
    std::cout << " time " << std::fixed << std::setprecision(1) << sec;
    std::cout << " nodes " << nodeCount;
    std::cout << " nps " << std::setprecision(0) << nps;
    std::cout << " pv";
    for (auto& move : result.pv) std::cout << " " << move;
    std::cout << std::endl;
}

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        searchThreads.push_back(std::make_unique<Thread>(i + 1));
    }
}

ThreadPool::~ThreadPool() { stopAll(); }

void ThreadPool::startAll(Chess& chess, int depth) {
    stopThreads = false;
    for (auto& searchThread : searchThreads) {
        searchThread->set(chess.toFEN(), depth);
        searchThread->start();
    }
}

void ThreadPool::stopAll() { stopThreads = true; }
