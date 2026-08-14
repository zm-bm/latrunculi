#include "search.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "board/board.hpp"
#include "search/limits.hpp"
#include "search/reporter.hpp"
#include "search/root_line.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"

namespace measurements {
namespace {

using MeasurementClock = std::chrono::steady_clock;

constexpr std::string_view result_format       = "search_measurement_v1";
constexpr int              default_depth       = 5;
constexpr std::size_t      default_threads     = 1;
constexpr std::uint64_t    default_repetitions = 1;
constexpr std::size_t      max_threads         = 64;
constexpr std::uint64_t    max_repetitions     = 100;

enum class OutputFormat { Text, Tsv };

struct Options {
    int           depth{default_depth};
    std::size_t   threads{default_threads};
    std::uint64_t repetitions{default_repetitions};
    OutputFormat  format{OutputFormat::Text};
};

struct Position {
    std::string_view id;
    std::string_view fen;
};

constexpr std::array positions = {
    Position{"startpos", Board::start_fen},
    Position{"arasan20-01", "r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1"},
    Position{"arasan20-08", "r1r3k1/p3bppp/2bp3Q/q2pP1P1/1p1BP3/8/PPP1B2P/2KR2R1 w - - 0 1"},
    Position{"arasan20-16", "8/3r4/pr1Pk1p1/8/7P/6P1/3R3K/5R2 w - - 0 1"},
    Position{"arasan20-21", "8/5pk1/p4npp/1pPN4/1P2p3/1P4PP/5P2/5K2 w - - 0 1"},
    Position{"arasan20-30", "b2rk3/r4p2/p3p3/P3Q1Np/2Pp3P/8/6P1/6K1 w - - 0 1"},
};

struct Result {
    search::RootLine line;
    NodeCount        nodes{0};
    Move             best_move{NULL_MOVE};
};

class Reporter final : public search::Reporter {
public:
    void report_progress(const search::RootLine& line,
                         const Board&,
                         NodeCount nodes,
                         Milliseconds) override {
        progress = Result{.line = line, .nodes = nodes};
    }

    void report_best_move(Move move) override {
        if (!progress)
            throw std::runtime_error("search published a best move without a final result");
        progress->best_move = move;
    }

    void report_diagnostic(std::string_view) override {}

    void reset() { progress.reset(); }

    [[nodiscard]] const Result& result() const {
        if (!progress || progress->best_move.is_null())
            throw std::runtime_error("search did not publish a final result");
        return *progress;
    }

private:
    std::optional<Result> progress;
};

struct Row {
    std::string   case_id;
    std::uint64_t repetition{0};
    std::uint64_t repetitions{0};
    int           requested_depth{0};
    int           completed_depth{0};
    std::size_t   threads{0};
    std::size_t   hash_mb{0};
    EvalValue     score{0};
    NodeCount     nodes{0};
    std::uint64_t total_ns{0};
    double        nodes_per_second{0.0};
    Move          best_move{NULL_MOVE};
    std::string   pv;
};

std::string format_pv(const search::PrincipalVariation& pv) {
    std::string result;
    for (int index = 0; index < pv.size(); ++index) {
        if (!result.empty())
            result += ' ';
        result += pv.move_at(index).str();
    }
    return result;
}

Row measure(const Position&     position,
            std::uint64_t       repetition,
            const Options&      options,
            Reporter&           reporter,
            search::ThreadPool& thread_pool) {
    Board board(position.fen);
    reporter.reset();
    thread_pool.clear_search_heuristics();
    search::tt.clear();

    search::Limits limits;
    limits.set_depth(options.depth);

    const auto start = MeasurementClock::now();
    if (!thread_pool.start_search(board, limits))
        throw std::runtime_error("failed to start search for " + std::string(position.id));
    thread_pool.wait();
    const auto end = MeasurementClock::now();

    const auto total_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    if (total_ns == 0)
        throw std::runtime_error("search duration was zero for " + std::string(position.id));

    const Result& result  = reporter.result();
    const double  seconds = static_cast<double>(total_ns) / 1'000'000'000.0;
    return {
        .case_id          = std::string(position.id),
        .repetition       = repetition,
        .repetitions      = options.repetitions,
        .requested_depth  = options.depth,
        .completed_depth  = result.line.depth,
        .threads          = options.threads,
        .hash_mb          = search::tt.capacity_mb(),
        .score            = result.line.value,
        .nodes            = result.nodes,
        .total_ns         = total_ns,
        .nodes_per_second = static_cast<double>(result.nodes) / seconds,
        .best_move        = result.best_move,
        .pv               = format_pv(result.line.pv),
    };
}

void emit_tsv(const std::vector<Row>& rows) {
    std::cout << "result_format\tcase\trepetition\trepetitions\trequested_depth\t"
                 "completed_depth\tthreads\thash_mb\tscore\tnodes\ttotal_ns\t"
                 "nodes_per_second\tbest_move\tpv\n";
    for (const Row& row : rows) {
        std::cout << result_format << '\t' << row.case_id << '\t' << row.repetition << '\t'
                  << row.repetitions << '\t' << row.requested_depth << '\t' << row.completed_depth
                  << '\t' << row.threads << '\t' << row.hash_mb << '\t' << row.score << '\t'
                  << row.nodes << '\t' << row.total_ns << '\t' << std::fixed << std::setprecision(3)
                  << row.nodes_per_second << '\t' << row.best_move.str() << '\t' << row.pv << '\n';
    }
}

void emit_text(const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        const double total_ms = static_cast<double>(row.total_ns) / 1'000'000.0;
        std::cout << row.case_id << " depth " << row.completed_depth << ": " << row.nodes
                  << " nodes in " << std::fixed << std::setprecision(3) << total_ms << " ms ("
                  << std::setprecision(0) << row.nodes_per_second << " nps), score " << row.score
                  << ", best move " << row.best_move.str();
        if (!row.pv.empty())
            std::cout << ", pv " << row.pv;
        std::cout << '\n';
    }
}

std::uint64_t parse_count(std::string_view text, std::string_view option) {
    std::uint64_t value     = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error("invalid value for " + std::string(option) + ": "
                                 + std::string(text));
    return value;
}

OutputFormat parse_format(std::string_view value) {
    if (value == "text")
        return OutputFormat::Text;
    if (value == "tsv")
        return OutputFormat::Tsv;
    throw std::runtime_error("unknown format: " + std::string(value));
}

void print_usage(const char* argv0) {
    std::cerr << "Fixed-depth integrated search measurement.\n";
    std::cerr << "Usage: " << argv0
              << " [--depth N] [--threads N] [--repetitions N] [--format text|tsv]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--depth") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --depth");
            const std::uint64_t depth = parse_count(argv[index], "--depth");
            if (depth == 0 || depth > static_cast<std::uint64_t>(search::Limits::max_depth))
                throw std::runtime_error("--depth must be between 1 and "
                                         + std::to_string(search::Limits::max_depth));
            options.depth = static_cast<int>(depth);
            continue;
        }
        if (argument == "--threads") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --threads");
            const std::uint64_t threads = parse_count(argv[index], "--threads");
            if (threads == 0 || threads > max_threads)
                throw std::runtime_error("--threads must be between 1 and "
                                         + std::to_string(max_threads));
            options.threads = static_cast<std::size_t>(threads);
            continue;
        }
        if (argument == "--repetitions") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --repetitions");
            options.repetitions = parse_count(argv[index], "--repetitions");
            if (options.repetitions == 0 || options.repetitions > max_repetitions)
                throw std::runtime_error("--repetitions must be between 1 and "
                                         + std::to_string(max_repetitions));
            continue;
        }
        if (argument == "--format") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --format");
            options.format = parse_format(argv[index]);
            continue;
        }
        throw std::runtime_error("unknown argument: " + std::string(argument));
    }
    return options;
}

} // namespace

int run_search(int argc, char* argv[]) {
    try {
        const Options options = parse_args(argc, argv);

        Reporter           reporter;
        search::ThreadPool thread_pool(options.threads, reporter);
        std::vector<Row>   rows;
        rows.reserve(positions.size() * static_cast<std::size_t>(options.repetitions));

        for (std::uint64_t repetition = 1; repetition <= options.repetitions; ++repetition) {
            for (const Position& position : positions)
                rows.push_back(measure(position, repetition, options, reporter, thread_pool));
        }

        if (options.format == OutputFormat::Tsv)
            emit_tsv(rows);
        else
            emit_text(rows);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}

} // namespace measurements
