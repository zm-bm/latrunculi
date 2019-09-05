#include <iostream>
#include "globals.hpp"
#include "uci.hpp"

int main()
{
	G::init();

	UCI::Controller controller(std::cin, std::cout);
	controller.loop();

	return 0;
}
