#include "board/board.hpp"

#include <algorithm>

bool Board::is_draw(int search_ply) const {
    if (halfmove() >= 100)
        return true;

    const int rewind      = std::min<int>(halfmove(), position_key_history.count());
    int       repetitions = 0;

    for (int distance = 2; distance <= rewind; distance += 2) {
        const int index = position_key_history.count() - distance;
        if (index < 0)
            break;

        if (position_key_history[index] != key())
            continue;

        if (distance < search_ply)
            return true;
        if (++repetitions == 2)
            return true;
    }

    return false;
}
