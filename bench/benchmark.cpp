#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "defs.hpp"
#include "engine.hpp"
#include "magic.hpp"
#include "thread.hpp"
#include "zobrist.hpp"

std::string TESTFILE;
const int MOVETIME   = 10000;  // in milliseconds
const int HASH_MB    = 16;
const int THREADS    = 1;
const int MOVE_LIMIT = 10000;  // max moves to search

// Get test file path relative to the executable
static std::string getTestFilePath() {
    std::filesystem::path exePath;

#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    exePath = buffer;
#else
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
    if (count != -1) {
        exePath = std::string(buffer, count);
    }
#endif

    // Starting from executable directory, look for the test file
    std::filesystem::path basePath = std::filesystem::path(exePath).parent_path();

    // Check for arasan20.epd in the bench directory next to the executable
    std::filesystem::path testFilePath = basePath / ".." / "bench" / "arasan20.epd";

    if (std::filesystem::exists(testFilePath)) {
        return testFilePath.string();
    }

    throw std::runtime_error("Test file not found: " + testFilePath.string());
}

class Benchmark {
   private:
    const int MIN_DEPTH = 8;

    std::vector<TestCase> testCases;

    std::ostringstream oss;
    std::stringstream iss;
    Engine engine{oss, oss, iss};

   public:
    Benchmark() {
        engine.execute("setoption name Threads value " + std::to_string(THREADS));
        engine.execute("setoption name Hash value " + std::to_string(HASH_MB));

        std::ifstream file(TESTFILE);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                testCases.push_back(TestCase(line));
            }
            file.close();
        } else {
            std::cerr << "Error: Could not open test file: " << TESTFILE << std::endl;
        }
    }

    TestResult testSearch(TestCase& testCase) {
        TestResult result{testCase};
        oss.str("");
        oss.clear();

        // run the search
        engine.execute("position fen " + testCase.fen);
        engine.execute("go movetime " + std::to_string(MOVETIME));
        while (true) {
            if (oss.str().find("bestmove") != std::string::npos) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // process the output stream
        std::istringstream lines{oss.str()};
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("info") == std::string::npos) continue;

            UCIInfo info{line};

            if (info.pvFirstMove.empty() || info.depth < MIN_DEPTH) continue;

            result.update(info);
        }

        return result;
    }

    void runAll() {
        int successful = 0;
        std::vector<int> maxDepths, successDepths;
        std::vector<int> maxTimes, successTimes;
        std::vector<U64> maxNps;

        for (auto& testCase : testCases) {
            if (maxDepths.size() >= MOVE_LIMIT) break;

            TestResult result = testSearch(testCase);
            std::cout << result << std::endl;

            maxDepths.push_back(result.maxDepth);
            maxTimes.push_back(result.maxTime);
            maxNps.push_back(result.nps);

            if (result.success) {
                successful++;
                successDepths.push_back(result.solutionDepth);
                successTimes.push_back(result.solutionTime);
            }
        }

        int avgAllDepth = std::reduce(maxDepths.begin(), maxDepths.end(), 0) / maxDepths.size();
        int avgAllTimes = std::reduce(maxTimes.begin(), maxTimes.end(), 0) / maxTimes.size();
        U64 avgNps      = std::reduce(maxNps.begin(), maxNps.end(), 0ULL) / maxNps.size();

        std::cout << "\nBenchmark Summary: ";
        std::cout << "Threads = " << THREADS << ", ";
        std::cout << "Movetime: " << MOVETIME << " ms" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Cases Passed: " << successful << " out of " << testCases.size() << std::endl;
        std::cout << "Average Depth: " << avgAllDepth << " ply" << std::endl;
        std::cout << "Average Time: " << avgAllTimes << " ms" << std::endl;
        std::cout << "Average NPS: " << avgNps << std::endl;

        if (successful > 0) {
            int avgSuccessDepth =
                std::reduce(successDepths.begin(), successDepths.end(), 0) / successDepths.size();
            int avgSuccessTimes =
                std::reduce(successTimes.begin(), successTimes.end(), 0) / successTimes.size();
            std::cout << "Average successful Depth: " << avgSuccessDepth << " ply" << std::endl;
            std::cout << "Average successful Time: " << avgSuccessTimes << " ms" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    Magic::init();
    Zobrist::init();

    try {
        // Allow override via command line argument
        if (argc > 1) {
            TESTFILE = argv[1];
            if (!std::filesystem::exists(TESTFILE)) {
                std::cerr << "Warning: Provided test file does not exist: " << TESTFILE
                          << std::endl;
                std::cerr << "Falling back to automatic path resolution." << std::endl;
                TESTFILE = getTestFilePath();
            }
        } else {
            TESTFILE = getTestFilePath();
        }

        std::cout << "Using test file: " << TESTFILE << std::endl;

        Benchmark benchmark;
        benchmark.runAll();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "To specify a test file path, use: " << argv[0] << " <path_to_test_file>"
                  << std::endl;
        return 1;
    }

    return 0;
}
