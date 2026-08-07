#include <string_view>

#include "core/attacks.hpp"

#include "evaluation.hpp"
#include "perft.hpp"

int main(int argc, char* argv[]) {
    attacks::init();

    if (argc > 1 && std::string_view(argv[1]) == "eval")
        return bench::run_evaluation(argc - 1, argv + 1);
    if (argc > 1 && std::string_view(argv[1]) == "perft")
        return bench::run_perft(argc - 1, argv + 1);

    // Preserve the original option-first perft interface.
    return bench::run_perft(argc, argv);
}
