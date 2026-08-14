#include <iostream>
#include <string_view>

#include "core/attacks.hpp"

#include "eval.hpp"
#include "perft.hpp"
#include "search.hpp"

namespace {

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable << " perft|eval|search [options]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    attacks::init();

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string_view command = argv[1];
    if (command == "--help" || command == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    if (command == "perft")
        return measurements::run_perft(argc - 1, argv + 1);
    if (command == "eval")
        return measurements::run_eval(argc - 1, argv + 1);
    if (command == "search")
        return measurements::run_search(argc - 1, argv + 1);

    std::cerr << "Unknown measurement: " << command << '\n';
    print_usage(argv[0]);
    return 1;
}
