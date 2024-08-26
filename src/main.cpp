#include <iostream>
#include "uci.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

int main()
{
    Magics::init();
    Zobrist::init();

	UCI::Controller controller(std::cin, std::cout);
	controller.loop();

	return 0;
}
