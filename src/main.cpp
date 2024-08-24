#include <iostream>
#include "uci.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

int main()
{
    Magic::init();
    Zobrist::init();

	UCI::Controller controller(std::cin, std::cout);
	controller.loop();

	return 0;
}
