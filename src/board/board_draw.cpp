#include "board/board.hpp"

#include <algorithm>
#include <cstddef>

namespace {

constexpr int fifty_move_rule_halfmoves = 100;

} // namespace

bool Board::is_draw(int ply_from_search_root) const noexcept {
    if (halfmove_clock() >= fifty_move_rule_halfmoves)
        return true;

    const std::size_t reversible_history_plies =
        std::min<std::size_t>(halfmove_clock(), key_history.size());
    int prior_occurrences = 0;

    for (std::size_t plies_back = 2; plies_back <= reversible_history_plies; plies_back += 2) {
        const std::size_t index = key_history.size() - plies_back;

        if (key_history[index] != key())
            continue;

        // One prior occurrence strictly after the search root is a cycle draw.
        // At or before the root, two are required for threefold repetition.
        if (ply_from_search_root > 0 &&
            plies_back < static_cast<std::size_t>(ply_from_search_root)) {
            return true;
        }
        if (++prior_occurrences == 2)
            return true;
    }

    return false;
}
