#include <iostream>
#include <string_view>

#include "bench/benchmark.hpp"
#include "core/attacks.hpp"
#include "eval/features.hpp"
#include "uci/engine.hpp"

int main(int argc, char* argv[]) {
    attacks::init();

    if (argc == 2 && std::string_view{argv[1]} == "bench")
        return bench::run();

    if (argc == 2 && std::string_view{argv[1]} == "features")
        return eval::export_features(std::cin, std::cout, std::cerr);

    if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [bench|features]\n";
        return 1;
    }

    uci::Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
