#include <iostream>

#include "board/zobrist.hpp"
#include "core/attacks.hpp"
#include "uci/engine.hpp"

int main() {
    attacks::init();
    zob::init();

    Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
