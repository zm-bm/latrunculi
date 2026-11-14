#include <iostream>

#include "cli/cli.hpp"
#include "core/attacks.hpp"
#ifdef LATRUNCULI_NNUE
#include "eval/nnue.hpp"
#endif

int main(int argc, char* argv[]) {
    attacks::init();
#ifdef LATRUNCULI_NNUE
    try {
        (void)nnue::backend();
        (void)nnue::network();
    } catch (const std::exception& e) {
        std::cerr << "NNUE initialization failed: " << e.what() << '\n';
        return 1;
    }
#endif
    return cli::run(argc, argv, std::cin, std::cout, std::cerr);
}
