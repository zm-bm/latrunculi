#include <chrono>
#include <filesystem>
#include <fstream>
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
const int   MIN_DEPTH  = BENCHMARK_MIN_TACTICAL_DEPTH; // minimum depth before tactical solve reporting is considered meaningful

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
            if (line.find("info") == std::string::npos)
                continue;

            UCIInfo info{line};

            if (info.first_move.empty())
                continue;

            result.observe(info);

            if (info.depth < MIN_DEPTH)
                continue;

            result.update_tactical(info);
        }

        return result;
    }

    void run_all_tests() {
        int                   successful = 0;
        int                   tactical_cases = 0;
        std::vector<int>      observed_depths, successful_depths;
        std::vector<int>      observed_times, successful_times;
        std::vector<uint64_t> observed_nps;
        std::vector<int>      tactical_depths;

        for (auto& test_case : test_cases) {
            if (observed_depths.size() >= MOVE_LIMIT)
                break;

            TestResult result = test_search(test_case);
            std::cout << result << std::endl;

            observed_depths.push_back(result.observed_max_depth);
            observed_times.push_back(result.observed_max_time);
            observed_nps.push_back(result.observed_nps);

            if (result.has_tactical_signal) {
                tactical_cases++;
                tactical_depths.push_back(result.tactical_max_depth);
            }

            if (result.success) {
                successful++;
                successful_depths.push_back(result.sol_depth);
                successful_times.push_back(result.sol_time);
            }
        }

        int avg_observed_depth = std::reduce(observed_depths.begin(), observed_depths.end(), 0) / observed_depths.size();
        int avg_observed_time  = std::reduce(observed_times.begin(), observed_times.end(), 0) / observed_times.size();
        uint64_t avg_observed_nps =
            std::reduce(observed_nps.begin(), observed_nps.end(), 0ULL) / observed_nps.size();

        std::cout << "\nSmoke Benchmark Summary\n";
        std::cout << "Threads = " << THREADS << ", ";
        std::cout << "movetime = " << SEARCHTIME << " ms, ";
        std::cout << "cases = " << observed_depths.size() << ", ";
        std::cout << "tactical threshold = depth " << MIN_DEPTH << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Observed averages: depth " << avg_observed_depth << " ply, time "
                  << avg_observed_time << " ms, nps " << avg_observed_nps << std::endl;

        if (tactical_cases == 0) {
            std::cout << "Tactical smoke signal: unavailable at this short setting"
                      << " (no case reached depth " << MIN_DEPTH << ")" << std::endl;
        } else {
            int avg_tactical_depth =
                std::reduce(tactical_depths.begin(), tactical_depths.end(), 0) / tactical_depths.size();
            std::cout << "Tactical smoke signal: solved " << successful << " / " << tactical_cases
                      << " qualifying cases, avg qualifying depth " << avg_tactical_depth << std::endl;
        }

        if (successful > 0) {
            int avg_success_depth =
                std::reduce(successful_depths.begin(), successful_depths.end(), 0) /
                successful_depths.size();
            int avg_success_time =
                std::reduce(successful_times.begin(), successful_times.end(), 0) /
                successful_times.size();
            std::cout << "Solved-case averages: depth " << avg_success_depth << " ply, time "
                      << avg_success_time << " ms" << std::endl;
        }

        std::cout << "Use this as a quick development smoke benchmark, not as a full engine comparison."
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
