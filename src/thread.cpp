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
    reset();
    startTime = std::chrono::high_resolution_clock::now();

    int score = 0;
    int depth = 0;

    // iterative deepening loop
    while (++depth <= searchDepth && !ThreadPool::stopThreads) {
        score = Search::negamax<true>(*this, -MATESCORE, MATESCORE, depth);
        ageHeuristics();
        printPV(score);
    }

    std::cout << "bestmove " << pvTable[0].moves.at(0) << std::endl;
}

void Thread::reset() {
    nodeCount = 0;
    currentDepth = 0;
    for (auto& pv : pvTable) pv.clear();
}

void Thread::ageHeuristics() { history.age(); }

void Thread::printPV(int score) {
    auto dur = std::chrono::high_resolution_clock::now() - startTime;
    auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(dur).count();
    auto nps = (sec > 0) ? nodeCount / sec : 0;

    std::cout << "info depth " << pvTable[0].moves.size();
    std::cout << " score cp " << score;
    std::cout << " time " << std::fixed << std::setprecision(1) << sec;
    std::cout << " nodes " << nodeCount;
    std::cout << " nps " << std::setprecision(0) << nps;
    std::cout << " pv";
    for (auto& move : pvTable[0].moves) std::cout << " " << move;
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
