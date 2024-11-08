#ifndef LATRUNCULI_FEN_H
#define LATRUNCULI_FEN_H

#include <string>
#include <vector>
#include <stdexcept>
#include "types.hpp"
#include "defs.hpp"

// https://www.chessprogramming.org/Forsyth-Edwards_Notation
class FenParser {
   public:
    explicit FenParser(const std::string& fen);
    void parse();

    std::string getPiecePlacement() const;
    Color getActiveColor() const;
    CastleRights getCastlingRights() const;
    Square getEnPassantTarget() const;
    U8 getHalfmoveClock() const;
    U32 getFullmoveNumber() const;
    U64 getKey() const;

   private:
    std::string fen;

    std::string piece_placement_;
    Color active_color_;
    CastleRights castling_rights_ = ALL_CASTLE;
    Square en_passant_target_;
    U8 halfmove_clock_;
    U32 fullmove_number_;
    U64 zkey_;

    void parsePiecePlacement(const std::string& section);
    void parseActiveColor(const std::string& section);
    void parseCastlingRights(const std::string& section);
    void parseEnPassantTarget(const std::string& section);
    void parseHalfmove(const std::string& section);
    void parseFullmove(const std::string& section, Color turn);
};

#endif  // LATRUNCULI_FEN_H
