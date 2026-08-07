#include "evaluation.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "eval/evaluation.hpp"

namespace bench {
namespace {

namespace fs     = std::filesystem;
using BenchClock = std::chrono::steady_clock;

constexpr std::string_view corpus_header     = "corpus_version\tid\tcategory\tfen";
constexpr std::string_view corpus_version    = "1";
constexpr std::string_view result_format     = "evaluation_snapshot_v1";
constexpr std::string_view throughput_format = "evaluation_throughput_v1";

constexpr std::uint64_t default_warmup_repetitions = 50'000;
constexpr std::uint64_t default_repetitions        = 100'000;
constexpr std::uint64_t default_samples            = 7;
constexpr std::uint64_t max_warmup_repetitions     = 1'000'000;
constexpr std::uint64_t max_repetitions            = 1'000'000;
constexpr std::uint64_t max_samples                = 25;

constexpr std::array<std::pair<std::string_view, eval::Term>, 10> terms = {{
    {"material", eval::Term::Material},
    {"squares", eval::Term::Squares},
    {"pawns", eval::Term::Pawns},
    {"knights", eval::Term::Knights},
    {"bishops", eval::Term::Bishops},
    {"rooks", eval::Term::Rooks},
    {"queens", eval::Term::Queens},
    {"king", eval::Term::King},
    {"mobility", eval::Term::Mobility},
    {"threats", eval::Term::Threats},
}};

enum SnapshotColumn : std::size_t {
    ResultFormat,
    CorpusVersion,
    PositionId,
    Category,
    Fen,
    SideToMove,
    Term,
    PerColor,
    WhiteMg,
    WhiteEg,
    BlackMg,
    BlackEg,
    TotalMg,
    TotalEg,
    UnscaledMg,
    UnscaledEg,
    ScaledMg,
    ScaledEg,
    TaperedValue,
    SideToMoveValue,
    Value,
    WhiteValue,
    RecordType,
    SnapshotColumnCount,
};

constexpr std::array<std::string_view, SnapshotColumnCount> snapshot_columns = {
    "result_format", "corpus_version", "position_id", "category",      "fen",
    "side_to_move",  "term",           "per_color",   "white_mg",      "white_eg",
    "black_mg",      "black_eg",       "total_mg",    "total_eg",      "unscaled_mg",
    "unscaled_eg",   "scaled_mg",      "scaled_eg",   "tapered_value", "side_to_move_value",
    "value",         "white_value",    "record_type",
};

using SnapshotRow = std::array<std::string, SnapshotColumnCount>;

fs::path default_corpus_path() {
    return fs::path(LATRUNCULI_SOURCE_DIR) / "bench" / "eval" / "corpus.tsv";
}

struct EvaluatedPosition {
    const EvaluationPosition* position;
    eval::Trace               trace;
};

struct EvaluationOptions {
    fs::path      corpus = default_corpus_path();
    bool          throughput{false};
    std::uint64_t warmup_repetitions{default_warmup_repetitions};
    std::uint64_t repetitions{default_repetitions};
    std::uint64_t samples{default_samples};
};

struct ThroughputRow {
    std::uint64_t checksum{0};
    std::uint64_t total_ns{0};
};

[[noreturn]] void fail(std::size_t line, std::string_view message) {
    throw std::runtime_error("invalid evaluation corpus at line " + std::to_string(line) + ": "
                             + std::string(message));
}

std::vector<std::string_view> split_tsv(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t                   begin = 0;

    while (true) {
        const std::size_t separator = line.find('\t', begin);
        if (separator == std::string_view::npos) {
            fields.push_back(line.substr(begin));
            return fields;
        }
        fields.push_back(line.substr(begin, separator - begin));
        begin = separator + 1;
    }
}

void require_trace_condition(const EvaluationPosition& position,
                             bool                      condition,
                             std::string_view          message) {
    if (!condition)
        throw std::runtime_error("inconsistent evaluation trace for " + position.id + ": "
                                 + std::string(message));
}

void validate_trace(const EvaluationPosition& position, const eval::Trace& trace) {
    require_trace_condition(
        position, trace.value() == eval::evaluate(position.board), "normal value");
    require_trace_condition(position, trace.term_total() == trace.unscaled_score(), "term total");
}

void emit_row(std::ostream& output, const SnapshotRow& row) {
    for (std::size_t index = 0; index < row.size(); ++index) {
        if (index != 0)
            output << '\t';
        output << row[index];
    }
    output << '\n';
}

std::string snapshot_tsv(const std::vector<EvaluatedPosition>& results) {
    std::ostringstream output;
    for (std::size_t index = 0; index < snapshot_columns.size(); ++index) {
        if (index != 0)
            output << '\t';
        output << snapshot_columns[index];
    }
    output << '\n';

    for (const EvaluatedPosition& result : results) {
        const EvaluationPosition& position = *result.position;
        const eval::Trace&        trace    = result.trace;
        SnapshotRow               summary;
        summary[ResultFormat]    = result_format;
        summary[CorpusVersion]   = corpus_version;
        summary[PositionId]      = position.id;
        summary[RecordType]      = "summary";
        summary[Category]        = position.category;
        summary[Fen]             = position.board.to_fen();
        summary[SideToMove]      = position.board.side_to_move() == WHITE ? "w" : "b";
        summary[UnscaledMg]      = std::to_string(trace.unscaled_score().mg);
        summary[UnscaledEg]      = std::to_string(trace.unscaled_score().eg);
        summary[ScaledMg]        = std::to_string(trace.scaled_score().mg);
        summary[ScaledEg]        = std::to_string(trace.scaled_score().eg);
        summary[TaperedValue]    = std::to_string(trace.tapered_value());
        summary[SideToMoveValue] = std::to_string(trace.side_to_move_value());
        summary[Value]           = std::to_string(trace.value());
        summary[WhiteValue]      = std::to_string(trace.white_value());
        emit_row(output, summary);

        for (const auto& [name, term] : terms) {
            const eval::TermScore&   score = trace.term(term);
            const eval::TaperedScore total = score.total();
            SnapshotRow              term_row;
            term_row[ResultFormat]  = result_format;
            term_row[CorpusVersion] = corpus_version;
            term_row[PositionId]    = position.id;
            term_row[RecordType]    = "term";
            term_row[Term]          = name;
            term_row[PerColor]      = score.per_color ? "1" : "0";
            term_row[WhiteMg]       = std::to_string(score.white.mg);
            term_row[WhiteEg]       = std::to_string(score.white.eg);
            term_row[BlackMg]       = std::to_string(score.black.mg);
            term_row[BlackEg]       = std::to_string(score.black.eg);
            term_row[TotalMg]       = std::to_string(total.mg);
            term_row[TotalEg]       = std::to_string(total.eg);
            emit_row(output, term_row);
        }
    }
    return output.str();
}

std::string compiler_name() {
#if defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

constexpr std::string_view build_mode() {
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

void observe_evaluation(EvalValue value) {
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" : : "g"(value) : "memory");
#else
    static volatile EvalValue sink;
    sink = value;
#endif
}

std::uint64_t evaluate_repeated(const std::vector<EvaluationPosition>& positions,
                                std::uint64_t                          repetitions) {
    std::uint64_t checksum = 0;
    for (std::uint64_t repetition = 0; repetition < repetitions; ++repetition) {
        for (const EvaluationPosition& position : positions) {
            const EvalValue value = eval::evaluate(position.board);
            observe_evaluation(value);
            checksum += static_cast<std::uint64_t>(value);
        }
    }
    return checksum;
}

std::vector<ThroughputRow> measure_throughput(const std::vector<EvaluationPosition>& positions,
                                              const EvaluationOptions&               options) {
    const auto corpus_size = static_cast<std::uint64_t>(positions.size());
    if (corpus_size > std::numeric_limits<std::uint64_t>::max() / options.repetitions)
        throw std::runtime_error("evaluation count overflow");

    evaluate_repeated(positions, options.warmup_repetitions);

    std::uint64_t              expected_checksum = 0;
    std::vector<ThroughputRow> rows;
    rows.reserve(static_cast<std::size_t>(options.samples));

    for (std::uint64_t sample = 1; sample <= options.samples; ++sample) {
        const auto start    = BenchClock::now();
        const auto checksum = evaluate_repeated(positions, options.repetitions);
        const auto end      = BenchClock::now();
        const auto elapsed  = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        const auto total_ns = static_cast<std::uint64_t>(elapsed.count());

        if (sample == 1)
            expected_checksum = checksum;
        else if (checksum != expected_checksum)
            throw std::runtime_error("evaluation sample checksum mismatch");
        if (total_ns == 0)
            throw std::runtime_error("evaluation sample duration was zero");

        rows.push_back({.checksum = checksum, .total_ns = total_ns});
    }
    return rows;
}

std::string throughput_tsv(const std::vector<ThroughputRow>&      rows,
                           const std::vector<EvaluationPosition>& positions,
                           const EvaluationOptions&               options) {
    std::ostringstream output;
    const auto evaluations = static_cast<std::uint64_t>(positions.size()) * options.repetitions;
    output << "result_format\tcorpus_version\tcompiler\tbuild_mode\tsample\tsamples\t"
              "corpus_size\twarmup_repetitions\trepetitions\tevaluations\tchecksum\ttotal_ns\t"
              "ns_per_evaluation\tevaluations_per_second\n";
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const ThroughputRow& row = rows[index];
        const double         ns_per_evaluation =
            static_cast<double>(row.total_ns) / static_cast<double>(evaluations);
        output << throughput_format << '\t' << corpus_version << '\t' << compiler_name() << '\t'
               << build_mode() << '\t' << index + 1 << '\t' << options.samples << '\t'
               << positions.size() << '\t' << options.warmup_repetitions << '\t'
               << options.repetitions << '\t' << evaluations << '\t' << row.checksum << '\t'
               << row.total_ns << '\t' << std::fixed << std::setprecision(3) << ns_per_evaluation
               << '\t' << 1'000'000'000.0 / ns_per_evaluation << '\n';
    }
    return output.str();
}

void print_usage(const char* argv0) {
    std::cerr << "Handcrafted-evaluation snapshots and throughput measurement.\n";
    std::cerr << "Usage: " << argv0 << " [--corpus PATH]\n";
    std::cerr << "       " << argv0
              << " throughput [--corpus PATH] [--warmup N] [--repetitions N] [--samples N]\n";
}

std::uint64_t parse_count(std::string_view text, std::string_view option) {
    std::uint64_t value     = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error("invalid value for " + std::string(option) + ": "
                                 + std::string(text));
    return value;
}

EvaluationOptions parse_args(int argc, char* argv[]) {
    EvaluationOptions options;
    int               index = 1;
    if (index < argc && std::string_view(argv[index]) == "throughput") {
        options.throughput = true;
        ++index;
    }

    for (; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--corpus") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --corpus");
            options.corpus = argv[index];
            continue;
        }
        if (options.throughput && argument == "--warmup") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --warmup");
            options.warmup_repetitions = parse_count(argv[index], "--warmup");
            continue;
        }
        if (options.throughput && argument == "--repetitions") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --repetitions");
            options.repetitions = parse_count(argv[index], "--repetitions");
            continue;
        }
        if (options.throughput && argument == "--samples") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --samples");
            options.samples = parse_count(argv[index], "--samples");
            continue;
        }
        throw std::runtime_error("unknown argument: " + std::string(argument));
    }

    if (options.warmup_repetitions > max_warmup_repetitions)
        throw std::runtime_error("--warmup must be at most "
                                 + std::to_string(max_warmup_repetitions));
    if (options.repetitions == 0 || options.repetitions > max_repetitions)
        throw std::runtime_error("--repetitions must be between 1 and "
                                 + std::to_string(max_repetitions));
    if (options.samples == 0 || options.samples > max_samples)
        throw std::runtime_error("--samples must be between 1 and " + std::to_string(max_samples));
    return options;
}

} // namespace

std::vector<EvaluationPosition> load_evaluation_corpus(const fs::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("unable to open evaluation corpus: " + path.string());

    std::string line;
    if (!std::getline(input, line))
        throw std::runtime_error("evaluation corpus is empty: " + path.string());
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != corpus_header)
        fail(1, "unexpected header");

    std::vector<EvaluationPosition> positions;
    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> fens;
    std::size_t                     line_number = 1;

    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            fail(line_number, "blank row");

        const std::vector<std::string_view> fields = split_tsv(line);
        if (fields.size() != 4)
            fail(line_number, "expected four columns");
        const std::string_view version  = fields[0];
        const std::string_view id       = fields[1];
        const std::string_view category = fields[2];
        const std::string_view fen      = fields[3];

        if (version != corpus_version)
            fail(line_number, "unsupported corpus version");
        if (id.empty())
            fail(line_number, "missing id");
        if (category.empty())
            fail(line_number, "missing category");
        if (fen.empty())
            fail(line_number, "missing FEN");
        if (!ids.emplace(id).second)
            fail(line_number, "duplicate id");
        if (!fens.emplace(fen).second)
            fail(line_number, "duplicate FEN");

        Board board;
        try {
            board.load_fen(fen);
        } catch (const std::exception& error) {
            fail(line_number, error.what());
        }
        if (board.to_fen() != fen)
            fail(line_number, "FEN is not canonical");

        positions.push_back({
            .id       = std::string(id),
            .category = std::string(category),
            .board    = board,
        });
    }

    if (positions.empty())
        throw std::runtime_error("evaluation corpus contains no positions: " + path.string());
    return positions;
}

int run_evaluation(int argc, char* argv[]) {
    try {
        const EvaluationOptions options   = parse_args(argc, argv);
        const auto              positions = load_evaluation_corpus(options.corpus);
        if (options.throughput) {
            const auto rows = measure_throughput(positions, options);
            std::cout << throughput_tsv(rows, positions, options);
            return 0;
        }

        std::vector<EvaluatedPosition> results;
        results.reserve(positions.size());

        for (const EvaluationPosition& position : positions) {
            eval::Trace trace = eval::evaluate_trace(position.board);
            validate_trace(position, trace);
            results.push_back({.position = &position, .trace = std::move(trace)});
        }
        std::cout << snapshot_tsv(results);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}

} // namespace bench
