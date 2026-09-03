#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <utility>

#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"
#include "eval/tapered_score.hpp"

class Board;

namespace eval {

inline constexpr int feature_schema_version = 1;

enum class Term : std::uint8_t {
    Material = 0,
    PieceSquare,
    Pawns,
    Knights,
    Bishops,
    Rooks,
    Queens,
    KingSafety,
    Mobility,
    Threats,
    Count,
};

struct TermScore {
    [[nodiscard]] TaperedScore total() const noexcept;

    TaperedScore white = TaperedScore::Zero;
    TaperedScore black = TaperedScore::Zero;
};

namespace feature {

using Id = std::size_t;

inline constexpr Id material_offset      = 0;
inline constexpr Id material_count       = 5;
inline constexpr Id piece_square_offset  = material_offset + material_count;
inline constexpr Id piece_square_count   = piece_slots * N_SQUARES;
inline constexpr Id isolated_pawn        = piece_square_offset + piece_square_count;
inline constexpr Id backward_pawn        = isolated_pawn + 1;
inline constexpr Id doubled_pawn         = backward_pawn + 1;
inline constexpr Id passed_pawn_offset   = doubled_pawn + 1;
inline constexpr Id passed_pawn_count    = 8;
inline constexpr Id piece_feature_offset = passed_pawn_offset + passed_pawn_count;

enum class PieceFeature : std::size_t {
    ReachableOutpost = 0,
    BishopOutpost,
    KnightOutpost,
    MinorPawnShield,
    BishopLongDiagonal,
    BishopPair,
    BishopBlockers,
    RookClosedFile,
    KingZoneXrayAttack,
    QueenDiscoveredAttack,
    Count,
};

inline constexpr Id piece_feature_count = std::to_underlying(PieceFeature::Count);
inline constexpr Id rook_open_offset    = piece_feature_offset + piece_feature_count;
inline constexpr Id rook_open_count     = 2;
inline constexpr Id mobility_offset     = rook_open_offset + rook_open_count;
inline constexpr Id knight_mob_count    = 9;
inline constexpr Id bishop_mob_count    = 14;
inline constexpr Id rook_mob_count      = 15;
inline constexpr Id queen_mob_count     = 28;
inline constexpr Id mobility_count =
    knight_mob_count + bishop_mob_count + rook_mob_count + queen_mob_count;
inline constexpr Id weak_piece_offset = mobility_offset + mobility_count;
inline constexpr Id weak_piece_count  = 4;
inline constexpr Id count             = weak_piece_offset + weak_piece_count;

constexpr Id material(PieceType piece) {
    return material_offset + std::size_t(piece - PAWN);
}

constexpr Id piece_square(PieceType piece, Square square) {
    return piece_square_offset + std::size_t(piece_slot(piece) * N_SQUARES + square);
}

constexpr Id passed_pawn(Rank rank) {
    return passed_pawn_offset + std::size_t(rank);
}

constexpr Id piece_feature(PieceFeature feature) {
    return piece_feature_offset + std::to_underlying(feature);
}

constexpr Id rook_open(bool fully_open) {
    return rook_open_offset + std::size_t(fully_open);
}

constexpr Id mobility(PieceType piece, int move_count) {
    switch (piece) {
    case KNIGHT: return mobility_offset + std::size_t(move_count);
    case BISHOP: return mobility_offset + knight_mob_count + std::size_t(move_count);
    case ROOK:
        return mobility_offset + knight_mob_count + bishop_mob_count + std::size_t(move_count);
    case QUEEN:
        return mobility_offset + knight_mob_count + bishop_mob_count + rook_mob_count
             + std::size_t(move_count);
    default: return count;
    }
}

constexpr Id weak_piece(PieceType piece) {
    return weak_piece_offset + std::size_t(piece - KNIGHT);
}

} // namespace feature

struct FeatureDefinition {
    std::string  name;
    TaperedScore weight;
};

struct FeatureRecord {
    std::array<TermScore, std::to_underlying(Term::Count)> terms{};
    std::array<int, feature::count>                        coefficients{};
    std::array<int, 4>                                     phase_counts{};
    std::array<int, N_COLORS>                              pawn_counts{};
    TaperedScore                                           fixed_score        = TaperedScore::Zero;
    TaperedScore                                           unscaled_score     = TaperedScore::Zero;
    TaperedScore                                           scaled_score       = TaperedScore::Zero;
    EvalValue                                              tapered_value      = 0;
    EvalValue                                              side_to_move_value = 0;
    EvalValue                                              value              = 0;
    Color                                                  turn               = WHITE;

    [[nodiscard]] const TermScore& term(Term term) const noexcept;
    [[nodiscard]] TaperedScore     term_total() const noexcept;
    [[nodiscard]] EvalValue        white_value() const noexcept;
    [[nodiscard]] EvalValue        reconstruct() const noexcept;

private:
    void add(feature::Id id, int coefficient) noexcept {
        assert(id < coefficients.size());
        coefficients[id] += coefficient;
    }

    void record(Term term, Color color, TaperedScore score) noexcept {
        TermScore& term_score = terms[std::to_underlying(term)];
        if (color == WHITE)
            term_score.white = score;
        else
            term_score.black = score;
    }

    void complete(const Board& board,
                  TaperedScore unscaled,
                  TaperedScore scaled,
                  EvalValue    tapered_value,
                  EvalValue    side_to_move_value,
                  EvalValue    value) noexcept;

    friend class Evaluator;
};

[[nodiscard]] const std::array<FeatureDefinition, feature::count>& feature_schema();
[[nodiscard]] FeatureRecord extract_features(const Board& board);
[[nodiscard]] std::string   format_evaluation(const FeatureRecord& record);

using PositionPreparer = std::function<bool(Board&)>;

// Exports source<TAB>result<TAB>fen records as versioned JSONL. The optional
// preparer may modify a board or return false to skip it.
void export_features(std::istream&           input,
                     std::ostream&           output,
                     const PositionPreparer& prepare = {});

} // namespace eval
