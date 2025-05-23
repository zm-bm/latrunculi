#pragma once

#include <ostream>
#include <string>

#include "search_stats.hpp"
#include "types.hpp"

class UCIOutput {
   public:
    explicit UCIOutput(std::ostream& out) : out(out) {}

    // UCI protocol commands
    void sendIdentity() const;
    void sendReady() const;
    void sendBestmove(std::string) const;
    void sendInfo(int, int, U64, Milliseconds, std::string, bool = false);

    // Non UCI protocol commands
    void sendStats(SearchStats stats) const;
    void toBeImplemented() const;

   private:
    std::ostream& out;
    int lastScore{0};
    std::string lastPV;

    std::string formatScore(int score);
};
