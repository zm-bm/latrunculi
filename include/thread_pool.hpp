#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "search_options.hpp"
#include "search_stats.hpp"
#include "uci_output.hpp"

class Thread;

class ThreadPool {
   public:
    ThreadPool() = delete;
    ThreadPool(size_t numThreads, UCIOutput& uciOutput);
    ~ThreadPool();

    void startAll(SearchOptions&);
    void exitAll();
    void stopAll();
    void waitAll();
    void resize(size_t newSize);

    void age() const;
    int getNodeCount() const;
    SearchStats<> getStats() const;

    friend class ThreadTest;
    friend class SearchTest;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
    UCIOutput& uciOutput;
};
