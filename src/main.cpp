#include <iostream>
#include "uci.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

int main()
{
    Magics::init();
    Zobrist::init();

	UCI::Engine engine(std::cin, std::cout);
	engine.loop();

	return 0;
}
