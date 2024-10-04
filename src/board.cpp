#include "board.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

Board::Board(const std::string& fen) {
    for (int i = 0; i < 7; i++) {
        pieces[BLACK][i] = 0;
        pieces[WHITE][i] = 0;
        pieceCount[BLACK][i] = 0;
        pieceCount[WHITE][i] = 0;
    }

    for (Square sq = A1; sq < INVALID; sq++) {
        squares[sq] = NO_PIECE;
    }

    loadFEN(fen);
}

void Board::loadFEN(const std::string& fen) {
    std::string pieces = G::split(fen, ' ').at(0);

    auto file = FILE1;
    auto rank = RANK8;

    for (auto iter = pieces.begin(); iter != pieces.end(); ++iter) {
        auto ch = *iter;

        if (((int)ch > 48) && ((int)ch < 57)) {
            file += File((int)ch - '0');
        } else {
            Square sq = Types::getSquare(file, rank);

            switch (ch) {
                case 'P':
                    addPiece<true>(sq, WHITE, PAWN);
                    file++;
                    break;
                case 'N':
                    addPiece<true>(sq, WHITE, KNIGHT);
                    file++;
                    break;
                case 'B':
                    addPiece<true>(sq, WHITE, BISHOP);
                    file++;
                    break;
                case 'R':
                    addPiece<true>(sq, WHITE, ROOK);
                    file++;
                    break;
                case 'Q':
                    addPiece<true>(sq, WHITE, QUEEN);
                    file++;
                    break;
                case 'K':
                    kingSq[WHITE] = sq;
                    addPiece<true>(sq, WHITE, KING);
                    file++;
                    break;
                case 'p':
                    addPiece<true>(sq, BLACK, PAWN);
                    file++;
                    break;
                case 'n':
                    addPiece<true>(sq, BLACK, KNIGHT);
                    file++;
                    break;
                case 'b':
                    addPiece<true>(sq, BLACK, BISHOP);
                    file++;
                    break;
                case 'r':
                    addPiece<true>(sq, BLACK, ROOK);
                    file++;
                    break;
                case 'q':
                    addPiece<true>(sq, BLACK, QUEEN);
                    file++;
                    break;
                case 'k':
                    kingSq[BLACK] = sq;
                    addPiece<true>(sq, BLACK, KING);
                    file++;
                    break;
                case '/':
                    rank--;
                    file = FILE1;
                    break;
            }
        }
    }
}

std::ostream& operator<<(std::ostream& os, const Board& b) {
    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = FILE1; file <= FILE8; file++) {
            os << " " << b.getPiece(Types::getSquare(file, rank)) << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";

    return os;
}

// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int score = 0;
//     score += calculateMobilityScore<KNIGHT>(opPhase, egPhase);
//     score += calculateMobilityScore<BISHOP>(opPhase, egPhase);
//     score += calculateMobilityScore<ROOK  >(opPhase, egPhase);
//     score += calculateMobilityScore<QUEEN >(opPhase, egPhase);
//     return score;
// }

// template<PieceRole p>
// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int mobilityScore = 0;
//     U64 occ = occupancy();

//     int whitemoves = calculateMobility<p>(getPieces<p>(WHITE),
//                                           ~getPieces<ALL_PIECE_ROLES>(WHITE), occ);
//     mobilityScore += whitemoves * (opPhase * G::MobilityScaling[OPENING][p-1]
//                                  + egPhase * G::MobilityScaling[ENDGAME][p-1]);

//     int blackmoves = calculateMobility<p>(getPieces<p>(BLACK),
//                                           ~getPieces<ALL_PIECE_ROLES>(BLACK), occ);
//     mobilityScore -= blackmoves * (opPhase * G::MobilityScaling[OPENING][p-1]
//                                  + egPhase * G::MobilityScaling[ENDGAME][p-1]);
//     return mobilityScore;
// }

// template <PieceRole p>
// int Board::calculateMobility(U64 bitboard, U64 targets, U64 occ) const {
//     int nmoves = 0;

//     while (bitboard) {
//         // Pop lsb bit and clear it from the bitboard
//         Square from = BB::lsb(bitboard);
//         bitboard &= BB::clear(from);

//         U64 mobility = BB::movesByPiece<p>(from, occ) & targets;
//         nmoves += BB::bitCount(mobility);
//     }

//     return nmoves;
// }