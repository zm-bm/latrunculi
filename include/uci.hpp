#ifndef ANTONIUS_UCI_H
#define ANTONIUS_UCI_H

#include <string>
#include "board.hpp"
#include "search.hpp"

// Universal Chess Interface (UCI)
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
namespace UCI
{

	class Controller 
	{

	public:

		Controller(std::istream&, std::ostream&);
		void loop();
		bool execute(const std::string& input);

	private:

		Board 	board;
		Search 	search;
		bool 	_debug;
		std::istream& istream;
		std::ostream& ostream;

		void uci();
		void setdebug(VecStr& tokens);
		void position(VecStr& tokens);
		void go(VecStr& tokens);
		void move(VecStr& tokens);
		void moves();

	};

}

#endif