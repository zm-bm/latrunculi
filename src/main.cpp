#include <iostream>
#include <string_view>

#include "bench/benchmark.hpp"
#include "core/attacks.hpp"
#include "uci/engine.hpp"

int main(int argc, char* argv[]) {
    attacks::init();

    if (argc == 2 && std::string_view{argv[1]} == "bench")
        return bench::run();

    if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [bench]\n";
        return 1;
    }

    uci::Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
