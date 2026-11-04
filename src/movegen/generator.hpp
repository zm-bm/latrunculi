#pragma once

#include <cstddef>
#include <iterator>

#include "board/board.hpp"
#include "movegen/move_list.hpp"

namespace movegen {

// Unscored legal moves in generation order. Generation finishes before iteration;
// restore the board before advancing. Ordered search uses Picker instead.
class LegalMoves {
private:
    class Iterator {
    public:
        using value_type        = Move;
        using difference_type   = std::ptrdiff_t;
        using reference         = Move;
        using pointer           = void;
        using iterator_concept  = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;
        Iterator(const Board& board, const Move* next, const Move* last)
            : board(&board),
              next(next),
              last(last) {
            skip_illegal();
        }

        Move      operator*() const { return *next; }
        Iterator& operator++() {
            ++next;
            skip_illegal();
            return *this;
        }
        Iterator operator++(int) {
            Iterator previous = *this;
            ++*this;
            return previous;
        }
        bool operator==(const Iterator& other) const { return next == other.next; }
        bool operator==(std::default_sentinel_t) const { return next == last; }

    private:
        void skip_illegal() {
            while (next != last && !board->is_legal_pseudo_move(*next))
                ++next;
        }
        const Board* board{nullptr};
        const Move*  next{nullptr};
        const Move*  last{nullptr};
    };

public:
    explicit LegalMoves(const Board& board);

    Iterator                begin() const { return {board, moves.begin(), moves.end()}; }
    std::default_sentinel_t end() const { return {}; }
    bool                    empty() const { return begin() == end(); }

    // Match before testing legality, retaining cheap lookup for a specific UCI token.
    template <typename Match>
    Move find(Match match) const {
        for (Move move : moves)
            if (match(move) && board.is_legal_pseudo_move(move))
                return move;
        return NULL_MOVE;
    }

private:
    const Board& board;
    MoveList     moves;
};

} // namespace movegen
