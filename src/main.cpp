#include <iostream>

#include "engine.hpp"
#include "magic.hpp"
#include "zobrist.hpp"

int main() {
    Magic::init();
    Zobrist::init();

    Engine engine(std::cout, std::cerr, std::cin);
    engine.loop();

    return 0;
}
