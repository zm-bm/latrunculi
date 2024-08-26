#include "globals.hpp"

#include <sstream>

std::vector<std::string> G::split(const std::string &s, char delim) {
  std::vector<std::string> tokens;
  std::string token;
  std::stringstream ss(s);

  while (std::getline(ss, token, delim)) {
    tokens.push_back(token);
  }

  return tokens;
}
