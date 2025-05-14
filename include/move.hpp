#pragma once

#include <sstream>

#include "types.hpp"

struct Move {
    /**
     * @brief move encoding
     * @details
     * 6 bits for from square (0-5)
     * 6 bits for to square (6-11)
     * 2 bits for move type (12-13)
     * 2 bits for promo piece (14-15)
     */
    U16 value{0};

    /// @brief move priority used for sorting
    U16 priority{0};

    constexpr Move() = default;
    constexpr Move(Square from, Square to, MoveType mtype = NORMAL, PieceType promoPiece = KNIGHT)
        : value{pack(from, to, mtype, promoPiece)} {}

    inline Square from() const { return unpackFrom(value); }
    inline Square to() const { return unpackTo(value); }
    inline MoveType type() const { return unpackType(value); }
    inline PieceType promoPiece() const { return unpackPromoPiece(value); }

    inline bool isNullMove() const { return value == 0; }
    inline bool operator==(const Move& rhs) const { return value == rhs.value; }

    static constexpr U16 pack(Square from, Square to, MoveType mtype, PieceType promoPiece) {
        auto promo = promoPiece - KNIGHT;
        return (from & 0x3F) |           // 6 bits for from
               ((to & 0x3F) << 6) |      // 6 bits for to
               ((mtype & 0x03) << 12) |  // 2 bits for move type
               ((promo & 0x03) << 14);   // 2 bits for promos
    }
    static constexpr Square unpackFrom(U16 packed) {
        return static_cast<Square>((packed >> 0) & 0x3F);
    }
    static constexpr Square unpackTo(U16 packed) {
        return static_cast<Square>((packed >> 6) & 0x3F);
    }
    static constexpr MoveType unpackType(U16 packed) {
        return static_cast<MoveType>((packed >> 12) & 0x03);
    }
    static constexpr PieceType unpackPromoPiece(U16 packed) {
        return static_cast<PieceType>(((packed >> 14) & 0x03) + KNIGHT);
    }

    std::string str() const;
};

constexpr Move NullMove{};

inline std::ostream& operator<<(std::ostream& os, const Move& mv) {
    if (mv.isNullMove()) {
        os << "none";
    } else {
        os << mv.from() << mv.to();
        if (mv.type() == PROMOTION) {
            switch (mv.promoPiece()) {
                case QUEEN: os << 'q'; break;
                case ROOK: os << 'r'; break;
                case BISHOP: os << 'b'; break;
                case KNIGHT: os << 'n'; break;
                default: break;
            }
        }
    }
    return os;
}
