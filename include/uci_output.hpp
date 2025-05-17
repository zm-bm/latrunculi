#pragma once

#include <ostream>
#include <string>

#include "search_stats.hpp"
#include "types.hpp"

class UCIOutput {
   public:
    explicit UCIOutput(std::ostream& out) : out(out) {}

    void sendIdentity();

    void sendBestmove(std::string);
    void sendInfo(int, int, int, Milliseconds, std::string);
    void sendStats(SearchStats& stats);

   private:
    std::ostream& out;

    std::string formatScore(int score);
};
