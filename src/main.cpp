#include <iostream>

#include "cli/cli.hpp"
#include "core/attacks.hpp"

int main(int argc, char* argv[]) {
    attacks::init();
    return cli::run(argc, argv, std::cin, std::cout, std::cerr);
}
