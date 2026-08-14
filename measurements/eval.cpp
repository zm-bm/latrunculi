#include "eval.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "board/board.hpp"
#include "eval/evaluation.hpp"

namespace measurements {
namespace {

using MeasurementClock = std::chrono::steady_clock;

constexpr std::string_view result_format    = "evaluation_throughput_v1";
constexpr std::string_view workload_version = "1";

constexpr std::array fens = {
    std::string_view{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
    std::string_view{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"},
    std::string_view{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
    std::string_view{"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"},
    std::string_view{"r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1"},
    std::string_view{"rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 1"},
    std::string_view{"4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 1"},
    std::string_view{"4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 1"},
    std::string_view{"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 1"},
    std::string_view{"4k3/8/8/8/8/8/3R4/2b1K3 w - - 0 1"},
    std::string_view{"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1"},
    std::string_view{"k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1"},
    std::string_view{"6kr/8/8/8/8/8/8/RK6 w - - 0 1"},
    std::string_view{"8/5pkp/6p1/8/8/6P1/5PKP/8 w - - 0 1"},
    std::string_view{"r1n1kn1r/8/8/8/8/8/8/R2QKB2 w - - 0 1"},
    std::string_view{"4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1"},
    std::string_view{"8/3r4/pr1Pk1p1/8/7P/6P1/3R3K/5R2 w - - 0 1"},
    std::string_view{"8/5pk1/p4npp/1pPN4/1P2p3/1P4PP/5P2/5K2 w - - 0 1"},
    std::string_view{"rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR b KQkq - 0 1"},
    std::string_view{"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 b - - 0 1"},
    std::string_view{"3r3k/8/8/8/8/8/3R4/3K4 w - - 0 1"},
    std::string_view{"8/pkp5/1p6/8/8/1P6/PKP5/8 w - - 0 1"},
    std::string_view{"1k6/8/5p2/1p1pN3/1PnP4/5P2/8/1K6 b - - 0 1"},
    std::string_view{"3k1B2/4r3/8/8/8/8/8/3K4 b - - 0 1"},
};

constexpr std::uint64_t default_warmup_repetitions = 50'000;
constexpr std::uint64_t default_repetitions        = 100'000;
constexpr std::uint64_t default_samples            = 7;
constexpr std::uint64_t max_warmup_repetitions     = 1'000'000;
constexpr std::uint64_t max_repetitions            = 1'000'000;
constexpr std::uint64_t max_samples                = 25;

constexpr std::uint64_t fnv_offset_basis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime        = 1'099'511'628'211ULL;

struct Options {
    std::uint64_t warmup_repetitions{default_warmup_repetitions};
    std::uint64_t repetitions{default_repetitions};
    std::uint64_t samples{default_samples};
};

struct Sample {
    std::uint64_t checksum{0};
    std::uint64_t total_ns{0};
};

std::vector<Board> make_workload() {
    std::vector<Board> positions;
    positions.reserve(fens.size());
    for (const std::string_view fen : fens)
        positions.emplace_back(fen);
    return positions;
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

std::uint64_t update_checksum(std::uint64_t checksum, EvalValue value) noexcept {
    checksum ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    return checksum * fnv_prime;
}

std::uint64_t evaluate_repeated(const std::vector<Board>& positions, std::uint64_t repetitions) {
    std::uint64_t checksum = fnv_offset_basis;
    for (std::uint64_t repetition = 0; repetition < repetitions; ++repetition) {
        for (const Board& position : positions) {
            const EvalValue value = eval::evaluate(position);
            observe_evaluation(value);
            checksum = update_checksum(checksum, value);
        }
    }
    return checksum;
}

std::vector<Sample> measure(const std::vector<Board>& positions, const Options& options) {
    evaluate_repeated(positions, options.warmup_repetitions);

    std::uint64_t       expected_checksum = 0;
    std::vector<Sample> samples;
    samples.reserve(static_cast<std::size_t>(options.samples));

    for (std::uint64_t sample = 1; sample <= options.samples; ++sample) {
        const auto start    = MeasurementClock::now();
        const auto checksum = evaluate_repeated(positions, options.repetitions);
        const auto end      = MeasurementClock::now();
        const auto elapsed  = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        const auto total_ns = static_cast<std::uint64_t>(elapsed.count());

        if (sample == 1)
            expected_checksum = checksum;
        else if (checksum != expected_checksum)
            throw std::runtime_error("evaluation sample checksum mismatch");
        if (total_ns == 0)
            throw std::runtime_error("evaluation sample duration was zero");

        samples.push_back({.checksum = checksum, .total_ns = total_ns});
    }
    return samples;
}

void emit_tsv(const std::vector<Sample>& samples,
              const std::vector<Board>&  positions,
              const Options&             options) {
    const auto evaluations = static_cast<std::uint64_t>(positions.size()) * options.repetitions;
    std::cout << "result_format\tworkload_version\tcompiler\tbuild_mode\tsample\tsamples\t"
                 "workload_size\twarmup_repetitions\trepetitions\tevaluations\tchecksum\ttotal_ns\t"
                 "ns_per_evaluation\tevaluations_per_second\n";
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const Sample& sample = samples[index];
        const double  ns_per_evaluation =
            static_cast<double>(sample.total_ns) / static_cast<double>(evaluations);
        std::cout << result_format << '\t' << workload_version << '\t' << compiler_name() << '\t'
                  << build_mode() << '\t' << index + 1 << '\t' << options.samples << '\t'
                  << positions.size() << '\t' << options.warmup_repetitions << '\t'
                  << options.repetitions << '\t' << evaluations << '\t' << sample.checksum << '\t'
                  << sample.total_ns << '\t' << std::fixed << std::setprecision(3)
                  << ns_per_evaluation << '\t' << 1'000'000'000.0 / ns_per_evaluation << '\n';
    }
}

void print_usage(const char* argv0) {
    std::cerr << "Isolated handcrafted-evaluation throughput measurement.\n";
    std::cerr << "Usage: " << argv0 << " [--warmup N] [--repetitions N] [--samples N]\n";
}

std::uint64_t parse_count(std::string_view text, std::string_view option) {
    std::uint64_t value     = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error("invalid value for " + std::string(option) + ": "
                                 + std::string(text));
    return value;
}

Options parse_args(int argc, char* argv[]) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--warmup") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --warmup");
            options.warmup_repetitions = parse_count(argv[index], "--warmup");
            continue;
        }
        if (argument == "--repetitions") {
            if (++index >= argc)
                throw std::runtime_error("missing value for --repetitions");
            options.repetitions = parse_count(argv[index], "--repetitions");
            continue;
        }
        if (argument == "--samples") {
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

int run_eval(int argc, char* argv[]) {
    try {
        const Options options   = parse_args(argc, argv);
        const auto    positions = make_workload();
        const auto    samples   = measure(positions, options);
        emit_tsv(samples, positions, options);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}

} // namespace measurements
