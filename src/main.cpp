#include <iostream>

#include "board/zobrist.hpp"
#include "core/magic.hpp"
#include "uci/engine.hpp"

int main() {
    magic::init();
    zob::init();

    Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
