#include <iostream>

#include "uci/engine.hpp"
#include "core/magic.hpp"
#include "board/zobrist.hpp"

int main() {
    magic::init();
    zob::init();

    Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
