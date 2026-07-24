#include "board/board.hpp"

#include <algorithm>
#include <cstddef>

namespace {

constexpr int fifty_move_rule_halfmoves = 100;

} // namespace

bool Board::is_draw(int search_ply) const noexcept {
    if (halfmove() >= fifty_move_rule_halfmoves)
        return true;

    const std::size_t rewind      = std::min<std::size_t>(halfmove(), key_history.size());
    int               repetitions = 0;

    for (std::size_t distance = 2; distance <= rewind; distance += 2) {
        const std::size_t index = key_history.size() - distance;

        if (key_history[index] != key())
            continue;

        if (search_ply > 0 && distance < static_cast<std::size_t>(search_ply))
            return true;
        if (++repetitions == 2)
            return true;
    }

    return false;
}
