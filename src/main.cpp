#include <iostream>

#include "magics.hpp"
#include "uci.hpp"
#include "zobrist.hpp"

int main() {
    Magics::init();
    Zobrist::init();

    UCI::Engine engine;
    engine.loop();

    return 0;
}
