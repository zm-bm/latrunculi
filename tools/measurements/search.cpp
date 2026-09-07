#include "search.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "search/limits.hpp"
#include "search/reporter.hpp"
#include "search/root_line.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"

namespace measurements {
namespace {

using MeasurementClock = std::chrono::steady_clock;

constexpr std::string_view result_format               = "search_measurement_v3";
constexpr int              default_depth               = 5;
constexpr std::size_t      default_threads             = 1;
constexpr std::size_t      default_hash_mb             = engine::default_hash_mb;
constexpr std::uint64_t    default_repetitions         = 1;
constexpr std::size_t      max_threads                 = 64;
constexpr std::size_t      max_hash_mb                 = 2048;
constexpr std::uint64_t    max_repetitions             = 100;
constexpr std::uint64_t    lmr_verifier_stride         = 64;
constexpr std::uint64_t    max_lmr_verifier_occurrence = 2048;

enum class OutputFormat { Text, Tsv };
enum class LimitType { Depth, Nodes, Movetime };

struct Options {
    LimitType                    limit_type{LimitType::Depth};
    std::uint64_t                limit_value{default_depth};
    std::optional<std::string>   case_id;
    std::size_t                  threads{default_threads};
    std::size_t                  hash_mb{default_hash_mb};
    std::uint64_t                repetitions{default_repetitions};
    OutputFormat                 format{OutputFormat::Text};
    std::optional<std::uint64_t> lmr_verify_occurrence;
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
    Position{"pilot14-g171-abrupt",
             "2q3k1/3nrpp1/p1p2n1p/P1Pp1Q2/1R1P3P/1N1B2P1/1P4K1/8 w - - 7 32"},
    Position{"pilot18-g154-abrupt",
             "1r3r2/1bp1q1k1/1pnp1pp1/p2Bp3/P2PP3/2P1P1R1/1P1N1R1P/5QK1 b - - 0 23"},
    Position{"pilot14-g061-gradual",
             "2r2rk1/p4p2/bp1p2pp/n1nPp3/1RP1P2q/4BPN1/P3B1PP/4RQK1 b - - 5 25"},
    Position{"pilot18-g093-gradual",
             "1Rbqkb1r/3n1ppp/4p3/1Bp5/3PPB2/2N2N2/1P3PPP/4K2R b Kk - 2 14"},
    Position{"pilot15-g078-secondary",
             "1r4k1/p2q2p1/b1pp1r2/4p1Q1/4Pp2/1PN3P1/P4P1P/R2R2K1 w - - 0 23"},
    Position{"objective-mate-1", "7R/8/8/8/8/1K6/8/1k6 w - - 0 1"},
    Position{"objective-mate-2", "8/8/8/8/8/3K4/4Q3/k7 w - - 0 1"},
    Position{"objective-rook-capture", "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"},
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

    void report_diagnostic(std::string_view text) override { diagnostic_text = text; }

    void reset() {
        progress.reset();
        diagnostic_text.clear();
    }

    [[nodiscard]] const Result& result() const {
        if (!progress || progress->best_move.is_null())
            throw std::runtime_error("search did not publish a final result");
        return *progress;
    }

    [[nodiscard]] const std::string& diagnostic() const { return diagnostic_text; }

private:
    std::optional<Result> progress;
    std::string           diagnostic_text;
};

struct Row {
    std::string   case_id;
    std::uint64_t repetition{0};
    std::uint64_t repetitions{0};
    LimitType     limit_type{LimitType::Depth};
    std::uint64_t limit_value{0};
    int           completed_depth{0};
    std::size_t   threads{0};
    std::size_t   hash_mb{0};
    EvalValue     static_score{0};
    EvalValue     score{0};
    NodeCount     nodes{0};
    std::uint64_t total_ns{0};
    double        nodes_per_second{0.0};
    Move          best_move{NULL_MOVE};
    std::string   pv;
    std::string   diagnostic;
};

std::string_view limit_type_name(LimitType type) {
    switch (type) {
    case LimitType::Depth:    return "depth";
    case LimitType::Nodes:    return "nodes";
    case LimitType::Movetime: return "movetime";
    }
    throw std::runtime_error("unknown search limit type");
}

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
    switch (options.limit_type) {
    case LimitType::Depth: limits.set_depth(static_cast<int>(options.limit_value)); break;
    case LimitType::Nodes: limits.set_nodes(options.limit_value); break;
    case LimitType::Movetime:
        limits.set_movetime(static_cast<Milliseconds::rep>(options.limit_value));
        break;
    }
#if LATRUNCULI_SEARCH_STATS
    limits.lmr_verify_occurrence = options.lmr_verify_occurrence;
#endif

    const EvalValue static_score = eval::evaluate(board);
    const auto      start        = MeasurementClock::now();
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
        .limit_type       = options.limit_type,
        .limit_value      = options.limit_value,
        .completed_depth  = result.line.depth,
        .threads          = options.threads,
        .hash_mb          = search::tt.capacity_mb(),
        .static_score     = static_score,
        .score            = result.line.value,
        .nodes            = result.nodes,
        .total_ns         = total_ns,
        .nodes_per_second = static_cast<double>(result.nodes) / seconds,
        .best_move        = result.best_move,
        .pv               = format_pv(result.line.pv),
        .diagnostic       = reporter.diagnostic(),
    };
}

void emit_tsv(const std::vector<Row>& rows) {
    std::cout << "result_format\tcase\trepetition\trepetitions\tlimit_type\tlimit_value\t"
                 "completed_depth\tthreads\thash_mb\tstatic_score\tscore\tnodes\ttotal_ns\t"
                 "nodes_per_second\tbest_move\tpv\n";
    for (const Row& row : rows) {
        std::cout << result_format << '\t' << row.case_id << '\t' << row.repetition << '\t'
                  << row.repetitions << '\t' << limit_type_name(row.limit_type) << '\t'
                  << row.limit_value << '\t' << row.completed_depth << '\t' << row.threads << '\t'
                  << row.hash_mb << '\t' << row.static_score << '\t' << row.score << '\t'
                  << row.nodes << '\t' << row.total_ns << '\t' << std::fixed << std::setprecision(3)
                  << row.nodes_per_second << '\t' << row.best_move.str() << '\t' << row.pv << '\n';
    }
}

void emit_text(const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        const double total_ms = static_cast<double>(row.total_ns) / 1'000'000.0;
        std::cout << row.case_id << ' ' << limit_type_name(row.limit_type) << ' ' << row.limit_value
                  << ", completed depth " << row.completed_depth << ": " << row.nodes
                  << " nodes in " << std::fixed << std::setprecision(3) << total_ms << " ms ("
                  << std::setprecision(0) << row.nodes_per_second << " nps), static score "
                  << row.static_score << ", score " << row.score << ", best move "
                  << row.best_move.str();
        if (!row.pv.empty())
            std::cout << ", pv " << row.pv;
        std::cout << '\n';
    }
}

void emit_diagnostics(const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        if (row.diagnostic.empty())
            continue;
        std::cerr << "search_diagnostic_v1 case=" << row.case_id << " repetition=" << row.repetition
                  << " limit_type=" << limit_type_name(row.limit_type)
                  << " limit_value=" << row.limit_value << " threads=" << row.threads
                  << " hash_mb=" << row.hash_mb << row.diagnostic;
        if (row.diagnostic.back() != '\n')
            std::cerr << '\n';
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
    std::cerr << "Integrated search measurement.\n";
    std::cerr << "Usage: " << argv0
              << " [--case ID] [--depth N | --nodes N | --movetime MS] [--hash MB]"
                 " [--threads N] [--repetitions N] [--format text|tsv]"
                 " [--lmr-verify-occurrence N]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;
    bool    limit_set  = false;
    int     case_count = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--depth" || argument == "--nodes" || argument == "--movetime") {
            if (++index >= argc)
                throw std::runtime_error("missing value for " + std::string(argument));
            if (limit_set)
                throw std::runtime_error("--depth, --nodes, and --movetime are mutually exclusive");

            const std::uint64_t value = parse_count(argv[index], argument);
            if (value == 0)
                throw std::runtime_error(std::string(argument) + " must be at least 1");
            if (argument == "--depth"
                && value > static_cast<std::uint64_t>(search::Limits::max_depth))
                throw std::runtime_error("--depth must be between 1 and "
                                         + std::to_string(search::Limits::max_depth));
            if (argument == "--movetime"
                && value
                       > static_cast<std::uint64_t>(std::numeric_limits<Milliseconds::rep>::max()))
                throw std::runtime_error("--movetime is out of range");

            options.limit_type  = argument == "--depth" ? LimitType::Depth
                                : argument == "--nodes" ? LimitType::Nodes
                                                        : LimitType::Movetime;
            options.limit_value = value;
            limit_set           = true;
            continue;
        }
        if (argument == "--case") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --case");
            ++case_count;
            options.case_id = argv[index];
            if (options.case_id->empty())
                throw std::runtime_error("--case must not be empty");
            continue;
        }
        if (argument == "--lmr-verify-occurrence") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --lmr-verify-occurrence");
            if (options.lmr_verify_occurrence)
                throw std::runtime_error("--lmr-verify-occurrence may be specified only once");

            const std::uint64_t occurrence = parse_count(argv[index], argument);
            if (occurrence != 0
                && (occurrence < lmr_verifier_stride || occurrence > max_lmr_verifier_occurrence
                    || occurrence % lmr_verifier_stride != 0)) {
                throw std::runtime_error(
                    "--lmr-verify-occurrence must be 0 or a multiple of 64 between 64 and 2048");
            }
            options.lmr_verify_occurrence = occurrence;
            continue;
        }
        if (argument == "--hash") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --hash");
            const std::uint64_t hash_mb = parse_count(argv[index], "--hash");
            if (hash_mb == 0 || hash_mb > max_hash_mb)
                throw std::runtime_error("--hash must be between 1 and "
                                         + std::to_string(max_hash_mb));
            options.hash_mb = static_cast<std::size_t>(hash_mb);
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

    if (options.lmr_verify_occurrence) {
        if constexpr (!search::stats_enabled)
            throw std::runtime_error("--lmr-verify-occurrence requires a release-stats build");
        if (case_count != 1)
            throw std::runtime_error("--lmr-verify-occurrence requires exactly one --case");
        if (!limit_set || options.limit_type != LimitType::Depth)
            throw std::runtime_error("--lmr-verify-occurrence requires an explicit --depth limit");
        if (options.threads != 1)
            throw std::runtime_error("--lmr-verify-occurrence requires --threads 1");
        if (options.repetitions != 1)
            throw std::runtime_error("--lmr-verify-occurrence requires --repetitions 1");
    }
    return options;
}

} // namespace

int run_search(int argc, char* argv[]) {
    try {
        const Options options = parse_args(argc, argv);

        const Position* selected = nullptr;
        if (options.case_id) {
            const auto found = std::find_if(positions.begin(), positions.end(), [&](const auto& p) {
                return p.id == *options.case_id;
            });
            if (found == positions.end())
                throw std::runtime_error("unknown search case: " + *options.case_id);
            selected = &*found;
        }

        Reporter           reporter;
        search::ThreadPool thread_pool(options.threads, reporter);
        std::vector<Row>   rows;
        const std::size_t  position_count = selected == nullptr ? positions.size() : 1;
        rows.reserve(position_count * static_cast<std::size_t>(options.repetitions));
        search::tt.resize(options.hash_mb);

        for (std::uint64_t repetition = 1; repetition <= options.repetitions; ++repetition) {
            if (selected != nullptr) {
                rows.push_back(measure(*selected, repetition, options, reporter, thread_pool));
            } else {
                for (const Position& position : positions)
                    rows.push_back(measure(position, repetition, options, reporter, thread_pool));
            }
        }

        if (options.format == OutputFormat::Tsv)
            emit_tsv(rows);
        else
            emit_text(rows);
        emit_diagnostics(rows);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}

} // namespace measurements
