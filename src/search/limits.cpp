#include "search/limits.hpp"

#include <limits>

namespace search {

std::optional<Milliseconds> Limits::allocated_time(Color c) const {
    if (movetime) {
        return movetime;
    } else if (wtime && btime) {
        using Rep = Milliseconds::rep;

        constexpr Rep SearchBufferMs = 50;
        constexpr Rep MaxTimeMs      = std::numeric_limits<Rep>::max();

        const auto time      = (c == WHITE) ? *wtime : *btime;
        const auto inc       = (c == WHITE) ? winc : binc;
        const auto moves     = std::max(movestogo.value_or(30), 1);
        const Rep  share     = std::max(Rep{0}, time.count() / moves);
        const Rep  increment = std::max(Rep{0}, inc.value_or(Milliseconds{0}).count());
        const Rep  available = increment > MaxTimeMs - share ? MaxTimeMs : share + increment;
        const Rep  buffered  = available > SearchBufferMs ? available - SearchBufferMs : Rep{0};
        return Milliseconds{std::max(Rep{10}, buffered)};
    }
    return std::nullopt;
}

} // namespace search
