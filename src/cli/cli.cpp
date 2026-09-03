#include "cli/cli.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <ostream>
#include <stdexcept>
#include <string_view>

#include "board/board.hpp"
#include "eval/features.hpp"
#include "search/limits.hpp"
#include "search/reporter.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "uci/engine.hpp"

namespace cli {
namespace {

enum class FeatureMode { Raw, Settled };

constexpr int bench_depth   = 13;
constexpr int bench_hash_mb = 32;

constexpr std::array bench_positions = {
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

void print_usage(std::ostream& diagnostics, const char* executable) {
    diagnostics << "Usage: " << executable << " [bench|features [--settle]]\n";
}

void run_benchmark(std::ostream& output) {
    Reporter           reporter;
    search::ThreadPool thread_pool(1, reporter);
    search::Limits     limits;
    limits.set_depth(bench_depth);

    search::tt.resize(bench_hash_mb);

    NodeCount  aggregate_nodes = 0;
    const auto start           = std::chrono::steady_clock::now();

    for (const std::string_view fen : bench_positions) {
        Board board(fen);
        reporter.reset();
        thread_pool.clear_search_heuristics();
        search::tt.clear();

        if (!thread_pool.start_search(board, limits))
            throw std::runtime_error("failed to start benchmark search");
        thread_pool.wait();
        aggregate_nodes += reporter.nodes();
    }

    const auto elapsed    = std::chrono::steady_clock::now() - start;
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    if (elapsed_ns <= 0)
        throw std::runtime_error("benchmark duration was zero");

    const auto nps = static_cast<NodeCount>(static_cast<long double>(aggregate_nodes)
                                            * 1'000'000'000.0L / elapsed_ns);
    output << aggregate_nodes << " nodes " << elapsed_ms << " ms " << nps << " nps\n";
}

void run_features(FeatureMode mode, std::istream& input, std::ostream& output) {
    if (mode == FeatureMode::Raw) {
        eval::export_features(input, output);
    } else {
        Reporter           reporter;
        search::ThreadPool thread_pool(1, reporter);
        eval::export_features(
            input, output, [&](Board& board) { return thread_pool.settle(board); });
    }
}

} // namespace

int run(int           argument_count,
        char* const   arguments[],
        std::istream& input,
        std::ostream& output,
        std::ostream& diagnostics) {
    const std::string_view command = argument_count == 1 ? "uci" : arguments[1];

    try {
        if (argument_count == 1) {
            uci::Engine engine(output, diagnostics, input);
            engine.loop();
            return 0;
        }

        if (command == "bench" && argument_count == 2) {
            run_benchmark(output);
            return 0;
        }

        if (command == "features") {
            if (argument_count == 2) {
                run_features(FeatureMode::Raw, input, output);
                return 0;
            }
            if (argument_count == 3 && std::string_view{arguments[2]} == "--settle") {
                run_features(FeatureMode::Settled, input, output);
                return 0;
            }
        }
    } catch (const std::exception& error) {
        diagnostics << command << ": " << error.what() << '\n';
        return 1;
    }

    print_usage(diagnostics, arguments[0]);
    return 1;
}

} // namespace cli
