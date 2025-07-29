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
const int   SEARCHTIME = 10000; // search time in milliseconds
const int   HASH_MB    = 16;
const int   THREADS    = 1;
const int   MOVE_LIMIT = 5;  // max moves to search
const int   MIN_DEPTH  = 10; // minimum depth to consider a move valid

// Get test file path relative to the executable
static std::string get_test_file_path() {
    std::filesystem::path exe_path;

#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    exePath = buffer;
#else
    char    buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count != -1) {
        exe_path = std::string(buffer, count);
    }
#endif

    std::filesystem::path base_path      = std::filesystem::path(exe_path).parent_path();
    std::filesystem::path test_file_path = base_path / ".." / "bench" / "arasan20.epd";

    if (std::filesystem::exists(test_file_path)) {
        return test_file_path.string();
    }

    throw std::runtime_error("Test file not found: " + test_file_path.string());
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

            if (info.first_move.empty() || info.depth < MIN_DEPTH)
                continue;

            result.update(info);
        }

        return result;
    }

    void run_all_tests() {
        int                   successful = 0;
        std::vector<int>      max_depths, successful_depths;
        std::vector<int>      max_times, successful_times;
        std::vector<uint64_t> max_nps;

        for (auto& test_case : test_cases) {
            if (max_depths.size() >= MOVE_LIMIT)
                break;

            TestResult result = test_search(test_case);
            std::cout << result << std::endl;

            max_depths.push_back(result.max_depth);
            max_times.push_back(result.max_time);
            max_nps.push_back(result.nps);

            if (result.success) {
                successful++;
                successful_depths.push_back(result.sol_depth);
                successful_times.push_back(result.sol_time);
            }
        }

        int avg_depth    = std::reduce(max_depths.begin(), max_depths.end(), 0) / max_depths.size();
        int avg_time     = std::reduce(max_times.begin(), max_times.end(), 0) / max_times.size();
        uint64_t avg_nps = std::reduce(max_nps.begin(), max_nps.end(), 0ULL) / max_nps.size();

        std::cout << "\nBenchmark Summary: ";
        std::cout << "Threads = " << THREADS << ", ";
        std::cout << "Movetime: " << SEARCHTIME << " ms" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Cases Passed: " << successful << " out of " << test_cases.size() << std::endl;
        std::cout << "Average Depth: " << avg_depth << " ply" << std::endl;
        std::cout << "Average Time: " << avg_time << " ms" << std::endl;
        std::cout << "Average NPS: " << avg_nps << std::endl;

        if (successful > 0) {
            int avg_success_depth =
                std::reduce(successful_depths.begin(), successful_depths.end(), 0) /
                successful_depths.size();
            int avg_success_time =
                std::reduce(successful_times.begin(), successful_times.end(), 0) /
                successful_times.size();
            std::cout << "Average successful Depth: " << avg_success_depth << " ply" << std::endl;
            std::cout << "Average successful Time: " << avg_success_time << " ms" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    magic::init();
    zob::init();

    try {
        // Allow override via command line argument
        if (argc > 1) {
            TESTFILE = argv[1];
            if (!std::filesystem::exists(TESTFILE)) {
                std::cerr << "Warning: Provided test file does not exist: " << TESTFILE
                          << std::endl;
                std::cerr << "Falling back to automatic path resolution." << std::endl;
                TESTFILE = get_test_file_path();
            }
        } else {
            TESTFILE = get_test_file_path();
        }

        std::cout << "Using test file: " << TESTFILE << std::endl;

        Benchmark benchmark;
        benchmark.run_all_tests();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "To specify a test file path, use: " << argv[0] << " <path_to_test_file>"
                  << std::endl;
        return 1;
    }

    return 0;
}
