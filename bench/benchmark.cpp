#include "benchmark.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "search/limits.hpp"
#include "search/reporter.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"

namespace bench {
namespace {

constexpr int depth = 13;

constexpr std::array positions = {
    std::string_view{Board::start_fen},
    std::string_view{"r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1"},
    std::string_view{"r1r3k1/p3bppp/2bp3Q/q2pP1P1/1p1BP3/8/PPP1B2P/2KR2R1 w - - 0 1"},
    std::string_view{"8/3r4/pr1Pk1p1/8/7P/6P1/3R3K/5R2 w - - 0 1"},
    std::string_view{"8/5pk1/p4npp/1pPN4/1P2p3/1P4PP/5P2/5K2 w - - 0 1"},
    std::string_view{"b2rk3/r4p2/p3p3/P3Q1Np/2Pp3P/8/6P1/6K1 w - - 0 1"},
};

class Reporter final : public search::Reporter {
public:
    void
    report_progress(const search::RootLine&, const Board&, NodeCount nodes, Milliseconds) override {
        final_nodes = nodes;
        reported    = true;
    }

    void report_best_move(Move move) override { best_move = move; }
    void report_diagnostic(std::string_view) override {}

    void reset() {
        final_nodes = 0;
        best_move   = NULL_MOVE;
        reported    = false;
    }

    [[nodiscard]] NodeCount nodes() const {
        if (!reported || best_move.is_null())
            throw std::runtime_error("search did not publish a complete result");
        return final_nodes;
    }

private:
    NodeCount final_nodes{0};
    Move      best_move{NULL_MOVE};
    bool      reported{false};
};

} // namespace

int run() {
    try {
        Reporter           reporter;
        search::ThreadPool thread_pool(1, reporter);
        search::Limits     limits;
        limits.set_depth(depth);

        search::tt.resize(engine::default_hash_mb);

        NodeCount  aggregate_nodes = 0;
        const auto start           = std::chrono::steady_clock::now();

        for (const std::string_view fen : positions) {
            Board board(fen);
            reporter.reset();
            thread_pool.clear_search_heuristics();
            search::tt.clear();

            if (!thread_pool.start_search(board, limits))
                throw std::runtime_error("failed to start benchmark search");
            thread_pool.wait();
            aggregate_nodes += reporter.nodes();
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        const auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        if (elapsed_ns <= 0)
            throw std::runtime_error("benchmark duration was zero");

        const auto nps = static_cast<NodeCount>(static_cast<long double>(aggregate_nodes)
                                                * 1'000'000'000.0L / elapsed_ns);
        std::cout << aggregate_nodes << " nodes " << elapsed_ms << " ms " << nps << " nps\n";
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

} // namespace bench
