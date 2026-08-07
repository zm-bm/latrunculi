#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "board/board.hpp"

namespace bench {

struct EvaluationPosition {
    std::string id;
    std::string category;
    Board       board;
};

[[nodiscard]] std::vector<EvaluationPosition>
load_evaluation_corpus(const std::filesystem::path& path);

int run_evaluation(int argc, char* argv[]);

} // namespace bench
