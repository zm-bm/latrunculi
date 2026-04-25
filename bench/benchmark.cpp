#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "bench_defs.hpp"
#include "engine.hpp"
#include "magic.hpp"
#include "zobrist.hpp"

std::string TESTFILE;
int         SEARCHTIME = 1000; // smoke benchmark search time in milliseconds
const int   HASH_MB    = 16;
int         THREADS    = 1;
int         MOVE_LIMIT = 5;  // max moves to search in a smoke run

// Get test file path relative to the executable
static std::string get_test_file_path() {
    std::vector<std::filesystem::path> candidates;

#ifdef LATRUNCULI_SOURCE_DIR
    candidates.emplace_back(std::filesystem::path(LATRUNCULI_SOURCE_DIR) / "bench" / "arasan20.epd");
#endif

#ifndef _WIN32
    char    buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count != -1) {
        std::filesystem::path exe_path(std::string(buffer, count));
        std::filesystem::path exe_dir = exe_path.parent_path();
        candidates.emplace_back(exe_dir / "../../bench/arasan20.epd");
        candidates.emplace_back(exe_dir / "../bench/arasan20.epd");
    }
#endif

    candidates.emplace_back(std::filesystem::current_path() / "bench" / "arasan20.epd");

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::canonical(candidate).string();
        }
    }

    std::ostringstream message;
    message << "Test file not found. Checked:";
    for (const auto& candidate : candidates)
        message << "\n  - " << candidate.string();
    throw std::runtime_error(message.str());
}

template <typename T>
static double average_of(const std::vector<T>& values) {
    if (values.empty())
        return 0.0;

    const auto total = std::reduce(values.begin(), values.end(), static_cast<long double>(0));
    return static_cast<double>(total / values.size());
}

template <typename T>
static double median_of(std::vector<T> values) {
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    if (values.size() % 2 == 1)
        return static_cast<double>(values[middle]);

    return static_cast<double>(values[middle - 1] + values[middle]) / 2.0;
}

template <typename T>
static std::pair<T, T> minmax_of(const std::vector<T>& values) {
    if (values.empty())
        return {T{}, T{}};

    const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    return {*min_it, *max_it};
}

static std::string format_metric(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1) << value;
    return out.str();
}

class Benchmark {
private:
    std::vector<TestCase> test_cases;

    std::ostringstream oss;
    std::stringstream  iss;
    Engine             engine{oss, oss, iss};

public:
    Benchmark() {
        engine.execute("setoption name Threads value " + std::to_string(THREADS));
        engine.execute("setoption name Hash value " + std::to_string(HASH_MB));

        std::ifstream file(TESTFILE);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                test_cases.push_back(TestCase(line));
            }
            file.close();
        } else {
            std::cerr << "Error: Could not open test file: " << TESTFILE << std::endl;
        }
    }

    TestResult test_search(TestCase& testCase) {
        TestResult result{testCase};
        oss.str("");
        oss.clear();

        // run the search
        engine.execute("position fen " + testCase.fen);
        engine.execute("go movetime " + std::to_string(SEARCHTIME));
        while (true) {
            if (oss.str().find("bestmove") != std::string::npos)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // process the output stream
        std::istringstream lines{oss.str()};
        std::string        line;
        while (std::getline(lines, line)) {
            if (line.rfind("bestmove ", 0) == 0) {
                std::istringstream bestmove_line{line};
                std::string        token;
                bestmove_line >> token >> result.bestmove;
                result.has_bestmove = !result.bestmove.empty();
                result.has_plausible_bestmove = result.is_plausible_move(testCase.fen, result.bestmove);
                continue;
            }

            if (line.find("info") == std::string::npos)
                continue;

            UCIInfo info{line};

            if (info.first_move.empty())
                continue;

            result.observe(info);
        }

        return result;
    }

    void run_all_tests() {
        std::vector<int>      observed_depths;
        std::vector<int>      observed_times;
        std::vector<uint64_t> observed_nodes;
        std::vector<uint64_t> observed_nps;
        int                   completed_cases = 0;
        int                   bestmove_cases  = 0;
        int                   plausible_cases = 0;

        for (auto& test_case : test_cases) {
            if (observed_depths.size() >= MOVE_LIMIT)
                break;

            TestResult result = test_search(test_case);
            std::cout << result << std::endl;

            observed_depths.push_back(result.observed_max_depth);
            observed_times.push_back(result.observed_max_time);
            observed_nodes.push_back(result.observed_max_nodes);
            observed_nps.push_back(result.observed_nps);
            completed_cases += result.saw_observed_info ? 1 : 0;
            bestmove_cases += result.has_bestmove ? 1 : 0;
            plausible_cases += result.has_plausible_bestmove ? 1 : 0;
        }

        const auto [min_depth, max_depth] = minmax_of(observed_depths);
        const auto [min_time, max_time] = minmax_of(observed_times);
        const auto [min_nodes, max_nodes] = minmax_of(observed_nodes);
        const auto [min_nps, max_nps] = minmax_of(observed_nps);

        std::cout << "\nSmoke Benchmark Summary\n";
        std::cout << "Threads = " << THREADS << ", ";
        std::cout << "movetime = " << SEARCHTIME << " ms, ";
        std::cout << "cases = " << observed_depths.size() << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Completed with observed info: " << completed_cases << " / " << observed_depths.size()
                  << ", bestmove seen: " << bestmove_cases << " / " << observed_depths.size()
                  << ", plausible bestmove: " << plausible_cases << " / " << observed_depths.size() << std::endl;
        std::cout << "Observed averages: depth " << format_metric(average_of(observed_depths)) << " ply, nodes "
                  << format_metric(average_of(observed_nodes)) << ", time "
                  << format_metric(average_of(observed_times)) << " ms, nps "
                  << format_metric(average_of(observed_nps)) << std::endl;
        std::cout << "Observed medians: nps " << format_metric(median_of(observed_nps)) << ", depth "
                  << format_metric(median_of(observed_depths)) << " ply" << std::endl;
        std::cout << "Observed spread: depth " << min_depth << "-" << max_depth << " ply, nodes "
                  << min_nodes << "-" << max_nodes << ", time " << min_time << "-" << max_time
                  << " ms, nps " << min_nps << "-" << max_nps << std::endl;
        std::cout << "Use this as a quick stability/throughput smoke benchmark, not as a tactical or comparative suite."
                  << std::endl;
    }
};

static void print_usage(const char* argv0) {
    std::cerr << "Smoke benchmark for quick development sanity checks.\n";
    std::cerr << "Usage: " << argv0
              << " [--threads N] [--movetime MS] [--limit N] [path_to_test_file]\n";
}

int main(int argc, char* argv[]) {
    magic::init();
    zob::init();

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            }

            if (arg == "--threads") {
                if (++i >= argc)
                    throw std::runtime_error("missing value for --threads");
                THREADS = std::stoi(argv[i]);
                continue;
            }

            if (arg == "--movetime") {
                if (++i >= argc)
                    throw std::runtime_error("missing value for --movetime");
                SEARCHTIME = std::stoi(argv[i]);
                continue;
            }

            if (arg == "--limit") {
                if (++i >= argc)
                    throw std::runtime_error("missing value for --limit");
                MOVE_LIMIT = std::stoi(argv[i]);
                continue;
            }

            TESTFILE = arg;
        }

        if (THREADS < 1)
            throw std::runtime_error("--threads must be >= 1");
        if (SEARCHTIME < 1)
            throw std::runtime_error("--movetime must be >= 1");
        if (MOVE_LIMIT < 1)
            throw std::runtime_error("--limit must be >= 1");

        if (!TESTFILE.empty()) {
            if (!std::filesystem::exists(TESTFILE)) {
                std::cerr << "Warning: Provided test file does not exist: " << TESTFILE << std::endl;
                std::cerr << "Falling back to automatic path resolution." << std::endl;
                TESTFILE = get_test_file_path();
            }
        } else {
            TESTFILE = get_test_file_path();
        }

        std::cout << "Using test file: " << TESTFILE << std::endl;
        std::cout << "Threads: " << THREADS << ", movetime: " << SEARCHTIME
                  << " ms, move limit: " << MOVE_LIMIT << std::endl;

        Benchmark benchmark;
        benchmark.run_all_tests();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
