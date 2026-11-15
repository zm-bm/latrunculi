#include <iostream>

#include "cli/cli.hpp"
#include "core/attacks.hpp"
#include "eval/nnue.hpp"

int main(int argc, char* argv[]) {
    attacks::init();
    try {
        nnue::initialize();
    } catch (const std::exception& e) {
        std::cerr << "NNUE initialization failed: " << e.what() << '\n';
        return 1;
    }
    return cli::run(argc, argv, std::cin, std::cout, std::cerr);
}
