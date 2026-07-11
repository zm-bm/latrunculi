#include "search/search_limits.hpp"

std::optional<Milliseconds> SearchLimits::allocated_time(Color c) const {
    if (movetime) {
        return movetime;
    } else if (wtime && btime) {
        using Rep = Milliseconds::rep;

        constexpr Rep SearchBufferMs = 50;

        const auto time  = (c == WHITE) ? *wtime : *btime;
        const auto inc   = (c == WHITE) ? winc : binc;
        const auto moves = movestogo.value_or(30);
        const Rep  res =
            (time.count() / moves) + inc.value_or(Milliseconds{0}).count() - SearchBufferMs;
        return Milliseconds{std::max(Rep{10}, res)};
    }
    return std::nullopt;
}
