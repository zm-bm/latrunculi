#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include "board/board.hpp"

namespace movegen {

// Ordered modes also require search/ordering/picker.hpp.
enum class Mode { All, MainSearch, Quiescence };

template <Mode Which>
class Moves;

namespace detail {

inline constexpr std::size_t MoveCapacity = 256;

template <Mode Which>
class MoveSource;

// Ordinary traversal loads the complete batch before consuming it in generation order.
template <>
class MoveSource<Mode::All> {
    friend class Moves<Mode::All>;

    explicit MoveSource(const Board& board);
    Move next_candidate() { return next != last ? *next++ : NULL_MOVE; }

    const Board&                   board;
    std::array<Move, MoveCapacity> storage;
    Move*                          next;
    Move*                          last;
};

} // namespace detail

// A single-pass legal move stream. Restore the node's board before advancing.
// Selection precedes legality so ordered modes retain their swaps and tie order.
template <Mode Which>
class Moves {
public:
    template <typename... Args>
    explicit Moves(const Board& board, Args&&... args)
        : source(board, std::forward<Args>(args)...) {}

    Moves(const Moves&)            = delete;
    Moves(Moves&&)                 = delete;
    Moves& operator=(const Moves&) = delete;
    Moves& operator=(Moves&&)      = delete;

    Move next() {
        return find([](Move) { return true; });
    }

    // Consume through the match, checking cheap predicates before legality (e.g. UCI).
    template <typename Match>
    Move find(Match match) {
        for (Move move = source.next_candidate(); !move.is_null(); move = source.next_candidate())
            if (match(move) && source.board.is_legal_pseudo_move(move))
                return move;
        return NULL_MOVE;
    }

    void skip_nonchecking_quiets() requires(Which == Mode::MainSearch)
    {
        source.skip_nonchecking_quiets();
    }

    // Raw node-start snapshot, even if excluded from validated quiet hints.
    Move counter_hint() const requires(Which == Mode::MainSearch)
    {
        return source.counter_move;
    }

private:
    detail::MoveSource<Which> source;
};

} // namespace movegen
