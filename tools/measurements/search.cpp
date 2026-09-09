#include "search.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

constexpr std::string_view result_format       = "search_measurement_v3";
constexpr int              default_depth       = 5;
constexpr std::size_t      default_threads     = 1;
constexpr std::size_t      default_hash_mb     = engine::default_hash_mb;
constexpr std::uint64_t    default_repetitions = 1;
constexpr std::size_t      max_threads         = 64;
constexpr std::size_t      max_hash_mb         = 2048;
constexpr std::uint64_t    max_repetitions     = 100;

enum class OutputFormat { Text, Tsv };
enum class LimitType { Depth, Nodes, Movetime };

struct Options {
    LimitType                  limit_type{LimitType::Depth};
    std::uint64_t              limit_value{default_depth};
    std::string                suite{LATRUNCULI_SEARCH_SUITE};
    std::optional<std::string> case_id;
    std::size_t                threads{default_threads};
    std::size_t                hash_mb{default_hash_mb};
    std::uint64_t              repetitions{default_repetitions};
    OutputFormat               format{OutputFormat::Text};
};

struct Position {
    std::string id;
    std::string fen;
};

std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n";
    const std::size_t          first      = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos)
        return {};
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

std::string extract_id(std::string_view operations) {
    std::optional<std::string> id;
    while (!operations.empty()) {
        const std::size_t      separator = operations.find(';');
        const std::string_view operation = trim(operations.substr(0, separator));
        if (operation.starts_with("id")
            && (operation.size() == 2 || operation[2] == ' ' || operation[2] == '\t')) {
            const std::string_view value = trim(operation.substr(2));
            if (value.empty() || value.front() != '"')
                throw std::runtime_error("id must be a quoted string");
            const std::size_t closing_quote = value.find('"', 1);
            if (closing_quote == std::string_view::npos || closing_quote != value.size() - 1)
                throw std::runtime_error("id must contain one quoted string");
            const std::string_view parsed = value.substr(1, closing_quote - 1);
            if (parsed.find_first_of("\t\r\n") != std::string_view::npos)
                throw std::runtime_error("id must not contain control characters");
            if (id)
                throw std::runtime_error("multiple id operations");
            id = parsed;
        }
        if (separator == std::string_view::npos)
            break;
        operations.remove_prefix(separator + 1);
    }
    if (!id)
        throw std::runtime_error("missing id operation");
    return *id;
}

std::vector<Position> load_positions(const std::string& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("cannot open search suite: " + path);

    std::vector<Position> positions;
    std::string           line;
    for (std::size_t line_number = 1; std::getline(input, line); ++line_number) {
        const std::string_view record = trim(line);
        if (record.empty() || record.starts_with('#'))
            continue;

        try {
            std::istringstream fields{std::string(record)};
            std::string        placement;
            std::string        side;
            std::string        castling;
            std::string        en_passant;
            if (!(fields >> placement >> side >> castling >> en_passant))
                throw std::runtime_error("expected four FEN fields");

            std::string operations;
            std::getline(fields, operations);
            Position position{
                .id  = extract_id(operations),
                .fen = placement + ' ' + side + ' ' + castling + ' ' + en_passant + " 0 1",
            };
            if (position.id.empty())
                throw std::runtime_error("id must not be empty");
            if (std::ranges::find(positions, position.id, &Position::id) != positions.end())
                throw std::runtime_error("duplicate id: " + position.id);

            (void)Board(position.fen);
            positions.push_back(std::move(position));
        } catch (const std::exception& error) {
            throw std::runtime_error("invalid search suite record " + path + ':'
                                     + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (!input.eof())
        throw std::runtime_error("failed while reading search suite: " + path);
    if (positions.empty())
        throw std::runtime_error("search suite contains no positions: " + path);
    return positions;
}

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
              << " [--suite EPD] [--case ID] [--depth N | --nodes N | --movetime MS]"
                 " [--hash MB]"
                 " [--threads N] [--repetitions N] [--format text|tsv]\n";
}

Options parse_args(int argc, char* argv[]) {
    Options options;
    bool    limit_set = false;
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
        if (argument == "--suite") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --suite");
            options.suite = argv[index];
            if (options.suite.empty())
                throw std::runtime_error("--suite must not be empty");
            continue;
        }
        if (argument == "--case") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --case");
            options.case_id = argv[index];
            if (options.case_id->empty())
                throw std::runtime_error("--case must not be empty");
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
    return options;
}

} // namespace

int run_search(int argc, char* argv[]) {
    try {
        const Options options   = parse_args(argc, argv);
        const auto    positions = load_positions(options.suite);

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
