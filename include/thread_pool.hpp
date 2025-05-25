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
    void stopAll();
    void haltAll();
    void waitAll();
    void resize(size_t newSize);

    int getNodeCount() const;
    SearchStats<> getStats() const;

   private:
    std::vector<std::unique_ptr<Thread>> threads;
    UCIOutput& uciOutput;
};
