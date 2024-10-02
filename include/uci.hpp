#ifndef LATRUNCULI_UCI_H
#define LATRUNCULI_UCI_H

#include <string>

#include "board.hpp"
#include "search.hpp"

// Universal Chess Interface (UCI)
// http://wbec-ridderkerk.nl/html/UCIProtocol.html
namespace UCI {

class Controller {
   public:
    Controller(std::istream&, std::ostream&);
    void loop();
    bool execute(const std::string& input);

   private:
    Board board;
    Search search;
    bool _debug;
    std::istream& istream;
    std::ostream& ostream;

    void uci();
    void setdebug(std::vector<std::string>& tokens);
    void position(std::vector<std::string>& tokens);
    void go(std::vector<std::string>& tokens);
    void move(std::vector<std::string>& tokens);
    void moves();
};

}  // namespace UCI

#endif