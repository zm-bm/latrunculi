#pragma once

#include "core/move_geometry.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

namespace zob {

// Immutable backing storage; production code uses the accessors below.
namespace storage {

struct Table {
    // Fixed project seed keeps keys reproducible; xorshift64* requires nonzero state.
    static constexpr PositionKey key_seed          = 0x4C617472756E6375ULL;
    static constexpr PositionKey output_multiplier = 0x2545F4914F6CDD1DULL;

    static_assert(key_seed != 0);

    // Full-period xorshift64* transition with an odd output multiplier.
    static constexpr PositionKey next_key(PositionKey& state) noexcept {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * output_multiplier;
    }

    // Materialize the complete lookup layout during constant evaluation.
    consteval Table() noexcept {
        PositionKey state = key_seed;
        for (auto& color : piece_keys)
            for (auto& piece : color)
                for (auto& square : piece)
                    square = next_key(state);

        turn_key = next_key(state);

        for (auto& key : enpassant_keys)
            key = next_key(state);

        for (int color = 0; color < N_COLORS; ++color) {
            castle_keys[CASTLE_KINGSIDE][color]  = next_key(state);
            castle_keys[CASTLE_QUEENSIDE][color] = next_key(state);
        }
    }

    PositionKey piece_keys[N_COLORS][N_PIECETYPES][N_SQUARES]{};
    PositionKey turn_key{};
    PositionKey enpassant_keys[8]{};
    PositionKey castle_keys[N_CASTLES][N_COLORS]{};
};

inline constexpr Table table{};

} // namespace storage

[[nodiscard]] constexpr PositionKey
hash_piece(Color color, PieceType piece, Square square) noexcept {
    return storage::table.piece_keys[color][piece][square];
}

[[nodiscard]] constexpr PositionKey hash_turn() noexcept {
    return storage::table.turn_key;
}

[[nodiscard]] constexpr PositionKey hash_ep(Square square) noexcept {
    return storage::table.enpassant_keys[square::file_of(square)];
}

[[nodiscard]] constexpr PositionKey hash_castle(CastleSide side, Color color) noexcept {
    return storage::table.castle_keys[side][color];
}

} // namespace zob
