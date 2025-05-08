#include <iostream>

#include "engine.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

int main() {
    Magics::init();
    Zobrist::init();

    Engine engine(std::cout, std::cin);
    engine.loop();

    return 0;
}
