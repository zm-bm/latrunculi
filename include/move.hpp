#ifndef LATRUNCULI_MOVE_H
#define LATRUNCULI_MOVE_H

#include <sstream>

#include "types.hpp"

class Move {
   public:
    Move();
    Move(Square, Square, MoveType, PieceRole);

    Square from() const;
    Square to() const;
    MoveType type() const;
    PieceRole promPiece() const;

    bool isNullMove() const;
    bool operator==(const Move&) const;
    bool operator<(const Move&) const;
    std::string DebugString() const;

    I16 score;

   private:
    U16 value;
};

inline Move::Move() : value{U16(NULLMOVE << 12)} {}

inline Move::Move(Square from,
                  Square to,
                  MoveType mtype = NORMAL,
                  PieceRole p = KNIGHT)
    : score(0),
      value{U16(from | (to << 6) | (mtype << 12) | ((p - KNIGHT) << 14))} {}

inline Square Move::from() const { return Square(value & 0x3f); }

inline Square Move::to() const { return Square((value >> 6) & 0x3f); }

inline MoveType Move::type() const { return MoveType((value >> 12) & 0x3); }

inline PieceRole Move::promPiece() const {
    return PieceRole(((value >> 14) & 0x3) + KNIGHT);
}

inline bool Move::isNullMove() const { return type() == NULLMOVE; }

inline bool Move::operator==(const Move& rhs) const {
    return value == rhs.value;
}

inline bool Move::operator<(const Move& rhs) const { return score < rhs.score; }

inline std::ostream& operator<<(std::ostream& os, const Move& mv) {
    os << mv.from() << mv.to();

    if (mv.type() == PROMOTION) {
        switch (mv.promPiece()) {
            case QUEEN:
                os << 'q';
                break;

            case ROOK:
                os << 'r';
                break;

            case BISHOP:
                os << 'b';
                break;

            case KNIGHT:
                os << 'n';
                break;

            default:
                break;
        }
    }

    return os;
}

inline std::string Move::DebugString() const {
    std::ostringstream oss;
    oss << this;
    return oss.str();
}

#endif