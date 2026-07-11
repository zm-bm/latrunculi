#pragma once

#include <functional>
#include <string>

#include "board/board.hpp"
#include "core/attacks.hpp"
#include "core/constants.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/eval.hpp"
#include "eval/tapered_score.hpp"
#include "eval/types.hpp"

class EvaluatorDebug;

// Evaluates chess positions using material, mobility, king safety + other positional factors
class Evaluator {
public:
    using TermCallback = std::function<void(EvalTerm, Color, TaperedScore)>;

    Evaluator() = delete;
    Evaluator(const Board&, TermCallback callback = nullptr);
    EvalValue evaluate();

private:
    const Board& board;
    TermCallback callback = nullptr;

    struct AttackData {
        Bitboard by[N_COLORS][N_PIECETYPES] = {{0}};
        Bitboard by2[N_COLORS]              = {0};
    } attacks;

    struct ZoneData {
        Bitboard outposts[N_COLORS] = {0};
        Bitboard mobility[N_COLORS] = {0};
        Bitboard king[N_COLORS]     = {0};
    } zones;

    struct KingAttackersData {
        int       count[N_COLORS] = {0};
        EvalValue value[N_COLORS] = {0};
    } king_attackers;

    struct ScoreData {
        TaperedScore mobility[N_COLORS] = {TaperedScore::Zero};
        TaperedScore threats[N_COLORS]  = {TaperedScore::Zero};
        TaperedScore final_score        = TaperedScore::Zero;
        EvalValue    final_value        = 0;
    } scores;

    struct PieceContext {
        Square   square    = INVALID;
        Bitboard piece_bb  = 0;
        Bitboard occupied  = 0;
        Bitboard pawns     = 0;
        Bitboard opp_pawns = 0;
    };

    template <Color C>
    void initialize();

    template <Color C>
    Bitboard outposts_zone(const Bitboard pawns, const Bitboard opp_pawns) const;

    template <Color C>
    Bitboard
    mobility_zone(const Bitboard pawns, const Bitboard opp_pawns, const Square king_sq) const;

    template <Color C>
    Bitboard king_zone(const Square king_sq) const;

    // Core evaluation methods

    template <EvalTerm Term, Color C = WHITE>
    TaperedScore evaluate_term();

    template <Color C>
    TaperedScore evaluate_pawns();

    template <Color C, PieceType P>
    TaperedScore evaluate_pieces();

    template <Color>
    TaperedScore evaluate_king_safety() const;

    template <Color C, PieceType P>
    void update_attacks(const Bitboard moves);

    template <Color C, PieceType P>
    void update_mobility(const Bitboard moves);

    template <Color C, PieceType P>
    void update_threats(const PieceContext& ctx);

    template <Color C, PieceType P>
    TaperedScore update_attackers(const PieceContext& ctx, const Bitboard moves);

    // Piece specific evaluation methods

    template <Color C, PieceType P>
    Bitboard get_moves(const PieceContext& ctx) const;

    template <Color C, PieceType P>
    TaperedScore evaluate_minor_pieces(const PieceContext& ctx, const Bitboard moves) const;

    template <Color C>
    TaperedScore evaluate_bishops(const PieceContext& ctx) const;

    template <Color C>
    TaperedScore evaluate_bishop_blockers(const PieceContext& ctx) const;

    template <Color C>
    TaperedScore evaluate_rook(const PieceContext& ctx) const;

    template <Color C>
    TaperedScore evaluate_queen(const PieceContext& ctx) const;

    template <Color C, PieceType P>
    bool discovery_attack(const PieceContext& ctx) const;

    // King evaluation methods

    template <Color C>
    TaperedScore evaluate_shelter(const Square king_sq) const;

    template <Color C>
    TaperedScore
    evaluate_shelter_file(const Bitboard pawns, const Bitboard opp_pawns, const File file) const;

    template <Color C>
    TaperedScore evaluate_danger(const Square king_sq) const;

    template <Color C>
    int calculate_raw_danger(const Square king_sq) const;

    template <PieceType P>
    int calculate_check_danger(const Bitboard safe_checks, const Bitboard all_checks) const;

    // Helpers

    int       scale_factor(const Color c) const;
    EvalValue taper_score(const TaperedScore score) const;
    int       phase() const;

    friend class EvaluatorDebug;
    friend class EvaluatorTest;
    friend std::formatter<EvaluatorDebug>;
};

/// returns static evaluation of a chess board
inline EvalValue evaluate(const Board& board) {
    return Evaluator(board).evaluate();
}

inline Evaluator::Evaluator(const Board& b, TermCallback callback) : board{b}, callback{callback} {
    initialize<WHITE>();
    initialize<BLACK>();
}

inline EvalValue Evaluator::evaluate() {
    TaperedScore score;

    // basic terms
    score += evaluate_term<TERM_MATERIAL>();
    score += evaluate_term<TERM_SQUARES>();
    score += evaluate_term<TERM_PAWNS, WHITE>() - evaluate_term<TERM_PAWNS, BLACK>();
    score += evaluate_term<TERM_KNIGHTS, WHITE>() - evaluate_term<TERM_KNIGHTS, BLACK>();
    score += evaluate_term<TERM_BISHOPS, WHITE>() - evaluate_term<TERM_BISHOPS, BLACK>();
    score += evaluate_term<TERM_ROOKS, WHITE>() - evaluate_term<TERM_ROOKS, BLACK>();
    score += evaluate_term<TERM_QUEENS, WHITE>() - evaluate_term<TERM_QUEENS, BLACK>();

    // terms requiring data accumulated by basic terms
    score += evaluate_term<TERM_KING, WHITE>() - evaluate_term<TERM_KING, BLACK>();
    score += evaluate_term<TERM_MOBILITY, WHITE>() - evaluate_term<TERM_MOBILITY, BLACK>();
    score += evaluate_term<TERM_THREATS, WHITE>() - evaluate_term<TERM_THREATS, BLACK>();

    const Color side_to_move  = board.side_to_move();
    const Color stronger_side = score.eg < 0 ? BLACK : WHITE;
    score.eg                  = (score.eg * scale_factor(stronger_side)) / eval::scale_limit;

    scores.final_score  = score;
    scores.final_value  = taper_score(score) * (side_to_move == WHITE ? 1 : -1);
    scores.final_value += eval::tempo_bonus;

    return scores.final_value;
}

/// prep zone data + seed king attacks
template <Color C>
inline void Evaluator::initialize() {
    constexpr Color Opp = ~C;

    const Square   king_sq    = board.king_sq(C);
    const Bitboard king_moves = attacks::piece_moves<KING>(king_sq);
    update_attacks<C, KING>(king_moves);

    const Bitboard pawns     = board.pieces<PAWN>(C);
    const Bitboard opp_pawns = board.pieces<PAWN>(Opp);

    zones.outposts[C] = outposts_zone<C>(pawns, opp_pawns);
    zones.mobility[C] = mobility_zone<C>(pawns, opp_pawns, king_sq);
    zones.king[C]     = king_zone<C>(king_sq);
}

/// outposts mask: behind enemy pawns, supported by friendly pawns, on ranks 4-6
template <Color C>
inline Bitboard Evaluator::outposts_zone(const Bitboard pawns, const Bitboard opp_pawns) const {
    constexpr Color Opp = ~C;

    const Bitboard behind_pawns = ~bb::attack_span<Opp>(opp_pawns);
    const Bitboard supported    = attacks::pawn_attacks<C>(pawns);
    constexpr auto outpost_mask = (C == WHITE) ? eval::masks::w_outposts : eval::masks::b_outposts;
    return (behind_pawns & supported & outpost_mask);
}

/// mobility mask: safe from enemy pawns, not occupied by the king or rank 2 pawns
template <Color C>
inline Bitboard Evaluator::mobility_zone(const Bitboard pawns,
                                         const Bitboard opp_pawns,
                                         const Square   king_sq) const {
    constexpr Color Opp = ~C;

    constexpr auto rank2    = (C == WHITE) ? bb::rank(RANK2) : bb::rank(RANK7);
    const Bitboard occupied = bb::set(king_sq) | (pawns & rank2);
    const Bitboard safe     = ~attacks::pawn_attacks<Opp>(opp_pawns);
    return (safe & ~occupied);
}

/// king zone: 3x3 king neighborhood for king safety evaluation
template <Color C>
inline Bitboard Evaluator::king_zone(const Square king_sq) const {
    const File   file   = std::clamp(square::file_of(king_sq), FILE2, FILE7);
    const Rank   rank   = std::clamp(square::rank_of(king_sq), RANK2, RANK7);
    const Square center = square::make(file, rank);

    return attacks::piece_moves<KING>(center) | bb::set(center);
}

/// dispatch a single eval term -> score
template <EvalTerm Term, Color C>
inline TaperedScore Evaluator::evaluate_term() {
    TaperedScore score;

    switch (Term) {
    case TERM_MATERIAL: score = board.material_score(); break;
    case TERM_SQUARES:  score = board.psq_bonus_score(); break;
    case TERM_PAWNS:    score = evaluate_pawns<C>(); break;
    case TERM_KNIGHTS:  score = evaluate_pieces<C, KNIGHT>(); break;
    case TERM_BISHOPS:  score = evaluate_pieces<C, BISHOP>(); break;
    case TERM_ROOKS:    score = evaluate_pieces<C, ROOK>(); break;
    case TERM_QUEENS:   score = evaluate_pieces<C, QUEEN>(); break;
    case TERM_KING:     score = evaluate_king_safety<C>(); break;
    case TERM_MOBILITY: score = scores.mobility[C]; break;
    case TERM_THREATS:  score = scores.threats[C]; break;
    default:            break;
    }

    if (callback)
        callback(Term, C, score);

    return score;
}

/// eval pawn structure: isolated + backward + doubled
template <Color C>
TaperedScore Evaluator::evaluate_pawns() {
    constexpr Color Opp = ~C;

    const Bitboard pawns         = board.pieces<PAWN>(C);
    const Bitboard opp_pawns     = board.pieces<PAWN>(Opp);
    const Bitboard left_attacks  = attacks::pawn_moves<PAWN_LEFT, C>(pawns);
    const Bitboard right_attacks = attacks::pawn_moves<PAWN_RIGHT, C>(pawns);
    const Bitboard pawn_attacks  = left_attacks | right_attacks;
    const Bitboard pawn_attacks2 = left_attacks & right_attacks;

    attacks.by2[C] |= pawn_attacks2 | (attacks.by[C][all_pieces_slot] & pawn_attacks);
    attacks.by[C][all_pieces_slot] |= pawn_attacks;
    attacks.by[C][PAWN]            |= pawn_attacks;

    // isolated pawns: no friendly pawns on adjacent files
    const Bitboard pawn_files = bb::fill(pawns);
    const Bitboard iso_pawns =
        (pawns & ~bb::shift_west(pawn_files)) & (pawns & ~bb::shift_east(pawn_files));
    TaperedScore score = eval::iso_pawn * bb::count(iso_pawns);

    // backwards pawns: pawns that can't advance safely
    const Bitboard stops       = attacks::pawn_moves<PAWN_PUSH, C>(pawns);
    const Bitboard attack_span = bb::attack_span<C>(pawns);
    const Bitboard opp_attacks = attacks::pawn_attacks<Opp>(opp_pawns);
    const Bitboard backwards_pawns =
        attacks::pawn_moves<PAWN_PUSH, Opp>(stops & opp_attacks & ~attack_span);
    score += eval::backward_pawn * bb::count(backwards_pawns);

    // doubled pawns: unsupported pawn with friendly pawns behind
    Bitboard pawns_behind   = pawns & bb::span_front<C>(pawns);
    Bitboard doubled_pawns  = pawns_behind & ~pawn_attacks;
    score                  += eval::doubled_pawn * bb::count(doubled_pawns);

    return score;
}

/// eval all pieces of type p for color c
template <Color C, PieceType P>
TaperedScore Evaluator::evaluate_pieces() {
    constexpr Color Opp = ~C;

    const Bitboard occupied  = board.occupancy();
    const Bitboard pawns     = board.pieces<PAWN>(C);
    const Bitboard opp_pawns = board.pieces<PAWN>(Opp);

    TaperedScore score;
    Bitboard     piece_bb = board.pieces<P>(C);
    bb::scan<C>(piece_bb, [&](Square sq) {
        const PieceContext context{.square    = sq,
                                   .piece_bb  = bb::set(sq),
                                   .occupied  = occupied,
                                   .pawns     = pawns,
                                   .opp_pawns = opp_pawns};

        const Bitboard moves = get_moves<C, P>(context);
        update_attacks<C, P>(moves);
        update_mobility<C, P>(moves);
        update_threats<C, P>(context);
        score += update_attackers<C, P>(context, moves);

        if constexpr (P == BISHOP || P == KNIGHT) {
            score += evaluate_minor_pieces<C, P>(context, moves);
        }

        if constexpr (P == BISHOP) {
            score += evaluate_bishops<C>(context);
        } else if constexpr (P == ROOK) {
            score += evaluate_rook<C>(context);
        } else if constexpr (P == QUEEN) {
            score += evaluate_queen<C>(context);
        }
    });

    if constexpr (P == BISHOP) {
        if (board.count(C, BISHOP) > 1)
            score += eval::bishop_pair;
    }

    return score;
}

/// king safety: pawn shelter and king danger
template <Color C>
TaperedScore Evaluator::evaluate_king_safety() const {
    constexpr Square kingside_sq  = C == WHITE ? G1 : G8;
    constexpr Square queenside_sq = C == WHITE ? C1 : C8;

    const Square king_sq = board.king_sq(C);
    TaperedScore shelter = evaluate_shelter<C>(king_sq);

    if (board.can_castle_kingside(C))
        shelter = std::max(shelter, evaluate_shelter<C>(kingside_sq));
    if (board.can_castle_queenside(C))
        shelter = std::max(shelter, evaluate_shelter<C>(queenside_sq));

    const TaperedScore danger = evaluate_danger<C>(king_sq);

    return shelter - danger;
}

/// merge moves into attack bitboards
template <Color C, PieceType P>
inline void Evaluator::update_attacks(const Bitboard moves) {
    attacks.by2[C]                 |= (attacks.by[C][all_pieces_slot] & moves);
    attacks.by[C][all_pieces_slot] |= moves;
    attacks.by[C][P]               |= moves;
}

/// add mobility bonus for # of moves
template <Color C, PieceType P>
inline void Evaluator::update_mobility(const Bitboard moves) {
    const int move_count  = bb::count(moves & zones.mobility[C]);
    scores.mobility[C]   += eval::mobility[P][move_count];
}

/// penalize weak pieces if attackers > defenders
template <Color C, PieceType P>
inline void Evaluator::update_threats(const PieceContext& ctx) {
    constexpr Color Opp = ~C;

    const Bitboard defenders = board.attacks_to(ctx.square, C);
    const Bitboard attackers = board.attacks_to(ctx.square, Opp);

    if (bb::count(attackers) > bb::count(defenders)) {
        scores.threats[C] += eval::weak_piece[P];
    }
}

/// update king attackers with attacks on enemy king zone
template <Color C, PieceType P>
inline TaperedScore Evaluator::update_attackers(const PieceContext& ctx, const Bitboard moves) {
    constexpr Color Opp = ~C;

    if (moves & zones.king[Opp]) {
        king_attackers.count[Opp]++;
        king_attackers.value[Opp] += eval::kingzone_att_danger[P];
    } else if constexpr (P == BISHOP || P == ROOK) {
        const Bitboard xray_moves = attacks::piece_moves<P>(ctx.square, ctx.pawns);
        if (zones.king[Opp] & xray_moves)
            return eval::kingzone_xray_att;
    }

    return TaperedScore::Zero;
}

template <Color C, PieceType P>
inline Bitboard Evaluator::get_moves(const PieceContext& ctx) const {
    Bitboard       moves  = attacks::piece_moves<P>(ctx.square, ctx.occupied);
    const Bitboard pinned = board.blockers(C) & ctx.piece_bb;
    if (pinned)
        moves &= square::collinear(board.king_sq(C), ctx.square);

    return moves;
}

/// minor piece eval: outposts + pawn shields
template <Color C, PieceType P>
inline TaperedScore Evaluator::evaluate_minor_pieces(const PieceContext& ctx,
                                                     const Bitboard      moves) const {
    constexpr Color Opp = ~C;
    static_assert(P == KNIGHT || P == BISHOP);

    TaperedScore score;
    if (ctx.piece_bb & zones.outposts[C]) {
        if constexpr (P == KNIGHT)
            score += eval::knight_outpost;
        else
            score += eval::bishop_outpost;
    } else if constexpr (P == KNIGHT) {
        if (moves & zones.outposts[C]) {
            score += eval::reachable_outpost;
        }
    }

    if (ctx.piece_bb & attacks::pawn_moves<PAWN_PUSH, Opp>(ctx.pawns)) {
        score += eval::minor_pawn_shield;
    }

    return score;
}

/// bishop eval: long diagonals + pawn block penalty
template <Color C>
inline TaperedScore Evaluator::evaluate_bishops(const PieceContext& ctx) const {
    TaperedScore score;

    const Bitboard xray_moves = attacks::piece_moves<BISHOP>(ctx.square, ctx.pawns);
    if (bb::is_many(eval::masks::center_squares & xray_moves))
        score += eval::bishop_long_diag;

    score += evaluate_bishop_blockers<C>(ctx);

    return score;
}

/// penalize bishops blocked by pawns on the same color squares
template <Color C>
inline TaperedScore Evaluator::evaluate_bishop_blockers(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const bool     dark_square = ctx.piece_bb & eval::masks::dark_squares;
    const Bitboard color_mask =
        dark_square ? eval::masks::dark_squares : eval::masks::light_squares;
    const Bitboard color_pawns = ctx.pawns & color_mask;
    const int      pawn_count  = bb::count(color_pawns);

    const Bitboard blocked_pawns = ctx.pawns & attacks::pawn_moves<PAWN_PUSH, Opp>(ctx.occupied);
    const Bitboard pawn_chain    = ctx.piece_bb & attacks::pawn_attacks<C>(ctx.pawns);
    const int blocking_factor = bb::count(blocked_pawns & eval::masks::center_files) + !pawn_chain;

    return eval::bishop_blockers * (pawn_count * blocking_factor);
}

/// rook eval: open/semi-open file bonus, closed+blocked penalty
template <Color C>
inline TaperedScore Evaluator::evaluate_rook(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const Bitboard file_mask  = bb::file(square::file_of(ctx.square));
    const Bitboard file_pawns = ctx.pawns & file_mask;

    const bool semi_open = !file_pawns;
    if (semi_open) {
        bool fully_open = !(ctx.opp_pawns & file_mask);
        return eval::rook_open_file[fully_open];
    }

    const bool blocked = file_pawns & attacks::pawn_moves<PAWN_PUSH, Opp>(ctx.occupied);
    if (blocked) {
        return eval::rook_closed_file;
    }

    return TaperedScore::Zero;
}

/// queen eval: penalize discovered attacks
template <Color C>
inline TaperedScore Evaluator::evaluate_queen(const PieceContext& ctx) const {
    if (discovery_attack<C, BISHOP>(ctx) || discovery_attack<C, ROOK>(ctx)) {
        return eval::queen_discover_att;
    }
    return TaperedScore::Zero;
}

/// penalize discovered attacks on the piece
template <Color C, PieceType P>
inline bool Evaluator::discovery_attack(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    Bitboard attackers = board.pieces<P>(Opp) & attacks::piece_moves<P>(ctx.square, 0);
    while (attackers) {
        const Square   attacker       = bb::lsb_pop(attackers);
        const Bitboard pieces_between = square::between(ctx.square, attacker) & ctx.occupied;

        if (!bb::is_many(pieces_between))
            return true;
    }

    return false;
}

/// king pawn-shelter score: friendly pawn shield vs enemy pawn storm
template <Color C>
TaperedScore Evaluator::evaluate_shelter(const Square king_sq) const {
    constexpr Color Opp = ~C;

    const File king_file = square::file_of(king_sq);
    const Rank king_rank = square::rank_of(king_sq);
    const auto pawns     = board.pieces<PAWN>(C);
    const auto opp_pawns = board.pieces<PAWN>(Opp);
    const auto pawn_mask = bb::span_front<C>(bb::rank(king_rank));

    const Bitboard pawns_ahead     = pawns & pawn_mask & ~attacks::pawn_attacks<Opp>(opp_pawns);
    const Bitboard opp_pawns_ahead = opp_pawns & pawn_mask;
    const File     file            = std::clamp(king_file, FILE2, FILE7);

    TaperedScore score;
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file - 1);
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file);
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file + 1);

    score += eval::king_file[king_file];

    const Bitboard file_mask     = bb::file(king_file);
    const bool     open_file     = !(pawns & file_mask);
    const bool     opp_open_file = !(opp_pawns & file_mask);

    score += eval::king_open_file[open_file][opp_open_file];

    return score;
}

/// shelter score for one file: friendly pawn rank + enemy pawn rank
template <Color C>
inline TaperedScore Evaluator::evaluate_shelter_file(const Bitboard pawns,
                                                     const Bitboard opp_pawns,
                                                     const File     file) const {
    constexpr Color Opp = ~C;

    const Bitboard file_pawns     = pawns & bb::file(file);
    const Bitboard file_opp_pawns = opp_pawns & bb::file(file);

    const Square pawn_sq     = bb::select<Opp>(file_pawns);
    const Square opp_pawn_sq = bb::select<Opp>(file_opp_pawns);

    const Rank rank     = file_pawns ? square::relative_rank(pawn_sq, C) : RANK1;
    const Rank opp_rank = file_opp_pawns ? square::relative_rank(opp_pawn_sq, C) : RANK1;
    const bool blocked  = file_pawns && (rank + 1 == opp_rank);

    TaperedScore score;
    score += eval::pawn_shelter[rank];
    score += eval::pawn_storm[blocked][opp_rank];

    return score;
}

/// convert raw danger into score {mg=quadratic scaling, eg=linear scaling}
template <Color C>
inline TaperedScore Evaluator::evaluate_danger(Square king_sq) const {
    const int danger  = calculate_raw_danger<C>(king_sq);
    const int midgame = danger * danger / 2048;
    const int endgame = danger / 8;
    return {midgame, endgame};
}

/// return raw king danger metric (checks, weak defenses, pins, etc)
template <Color C>
int Evaluator::calculate_raw_danger(Square king_sq) const {
    constexpr Color Opp = ~C;

    int danger = 0;

    const Bitboard defended     = attacks.by[C][all_pieces_slot];
    const Bitboard attacked     = attacks.by[Opp][all_pieces_slot];
    const Bitboard kq_defense   = attacks.by[C][QUEEN] | attacks.by[C][KING];
    const Bitboard weak_defense = (~defended | kq_defense) & attacked & ~attacks.by2[C];
    const Bitboard our_pieces   = board.pieces(Opp);
    const Bitboard safe_checks  = ~our_pieces & (~defended | (weak_defense & attacks.by2[Opp]));

    const Bitboard knight_moves  = attacks.by[Opp][KNIGHT];
    const Bitboard knight_checks = attacks::piece_moves<KNIGHT>(king_sq) & knight_moves;
    danger += calculate_check_danger<KNIGHT>(knight_checks & safe_checks, knight_checks);

    const Bitboard occupancy   = board.occupancy();
    const Bitboard line_checks = attacks::piece_moves<ROOK>(king_sq, occupancy);
    const Bitboard diag_checks = attacks::piece_moves<BISHOP>(king_sq, occupancy);

    const Bitboard rook_checks = line_checks & attacks.by[Opp][ROOK];
    danger += calculate_check_danger<ROOK>(rook_checks & safe_checks, rook_checks);

    const Bitboard queen_checks      = (line_checks | diag_checks) & attacks.by[Opp][QUEEN];
    const Bitboard bad_queen_checks  = rook_checks | attacks.by[C][QUEEN];
    const Bitboard safe_queen_checks = queen_checks & safe_checks & ~bad_queen_checks;
    danger += calculate_check_danger<QUEEN>(safe_queen_checks, queen_checks);

    const Bitboard bishop_checks      = diag_checks & attacks.by[Opp][BISHOP];
    const Bitboard safe_bishop_checks = bishop_checks & safe_checks & ~queen_checks;
    danger += calculate_check_danger<BISHOP>(safe_bishop_checks, bishop_checks);

    danger += king_attackers.value[C] * king_attackers.count[C];
    danger += eval::weak_kingzone_danger * bb::count(zones.king[C] & weak_defense);
    danger += eval::pinned_piece_danger * bb::count(board.blockers(C));

    return danger;
}

/// scale danger non-linearly, 1 check = 1×, 2 checks = ~1.3×, 3 checks = 1.5×, asymptotes to 2×
template <PieceType P>
inline int Evaluator::calculate_check_danger(const Bitboard safe_checks,
                                             const Bitboard all_checks) const {
    const int  count  = bb::count(safe_checks ? safe_checks : all_checks);
    const auto danger = safe_checks ? eval::safe_check_danger : eval::unsafe_check_danger;

    return (danger[P] * count * 2) / (count + 1);
}

// integer numerator for scaling endgame eval toward 0 in draw-ish pawn endings
inline int Evaluator::scale_factor(const Color c) const {
    const int pawn_count = board.count(c, PAWN);
    return std::min(eval::scale_limit, 36 + 5 * pawn_count);
}

// blend midgame / endgame scores based on game phase
inline EvalValue Evaluator::taper_score(const TaperedScore score) const {
    int mg_phase = phase();
    int eg_phase = eval::phase_limit - mg_phase;

    return ((score.mg * mg_phase) + (score.eg * eg_phase)) / eval::phase_limit;
}

// game phase: 0 = endgame, phase_limit = middlegame
inline int Evaluator::phase() const {
    int npm      = board.nonPawnMaterial(WHITE) + board.nonPawnMaterial(BLACK);
    int material = std::clamp(npm, eval::material_eg, eval::material_mg);

    return ((material - eval::material_eg) * eval::phase_limit) /
           (eval::material_mg - eval::material_eg);
}

// track scores for a single evaluation term
struct TermScore {
    void         add_score(TaperedScore score, Color color);
    TaperedScore white    = TaperedScore::Zero;
    TaperedScore black    = TaperedScore::Zero;
    bool         has_both = false;
};

struct ScoreTracker {
    void      add_score(EvalTerm term, TaperedScore score, Color color);
    TermScore scores[N_TERMS];
};

// evaluator + per-term score breakdown (for logging/debug)
class EvaluatorDebug {
public:
    EvaluatorDebug(const Board& b);
    EvalValue evaluate() { return evaluator.evaluate(); }
    friend std::formatter<EvaluatorDebug>;

private:
    ScoreTracker score_tracker;
    Evaluator    evaluator;
};

template <>
struct std::formatter<TaperedScore> : std::formatter<std::string_view> {
    auto format(const TaperedScore& score, std::format_context& ctx) const {
        return std::format_to(ctx.out(),
                              "{:5.2f} {:5.2f}",
                              double(score.mg) / int(piece_value::pawn_mg),
                              double(score.eg) / int(piece_value::pawn_mg));
    }
};

template <>
struct std::formatter<TermScore> : std::formatter<std::string_view> {
    static constexpr auto both_format   = " | {} | {} | {} | ";
    static constexpr auto single_format = " |  ----  ---- |  ----  ---- | {} | ";

    auto format(const TermScore& t, std::format_context& ctx) const {
        return t.has_both
                   ? std::format_to(ctx.out(), both_format, t.white, t.black, t.white - t.black)
                   : std::format_to(ctx.out(), single_format, t.white);
    }
};

template <>
struct std::formatter<EvaluatorDebug> : std::formatter<std::string_view> {
    auto format(const EvaluatorDebug& ev, std::format_context& ctx) const {
        auto& evaluator = ev.evaluator;
        auto& tracker   = ev.score_tracker;
        auto  score     = evaluator.scores.final_score;
        auto  value     = evaluator.scores.final_value;
        Color color     = evaluator.board.side_to_move();
        auto  result    = color == WHITE ? value : -value;
        auto  out       = ctx.out();

        out = std::format_to(out, "     Term    |    White    |    Black    |    Total   \n");
        out = std::format_to(out, "             |   MG    EG  |   MG    EG  |   MG    EG \n");
        out = std::format_to(out, " ------------+-------------+-------------+------------\n");
        out = std::format_to(out, "{:>12}{}\n", "Material", tracker.scores[TERM_MATERIAL]);
        out = std::format_to(out, "{:>12}{}\n", "Piece Sq.", tracker.scores[TERM_SQUARES]);
        out = std::format_to(out, "{:>12}{}\n", "Pawns", tracker.scores[TERM_PAWNS]);
        out = std::format_to(out, "{:>12}{}\n", "Knights", tracker.scores[TERM_KNIGHTS]);
        out = std::format_to(out, "{:>12}{}\n", "Bishops", tracker.scores[TERM_BISHOPS]);
        out = std::format_to(out, "{:>12}{}\n", "Rooks", tracker.scores[TERM_ROOKS]);
        out = std::format_to(out, "{:>12}{}\n", "Queens", tracker.scores[TERM_QUEENS]);
        out = std::format_to(out, "{:>12}{}\n", "Kings", tracker.scores[TERM_KING]);
        out = std::format_to(out, "{:>12}{}\n", "Mobility", tracker.scores[TERM_MOBILITY]);
        out = std::format_to(out, "{:>12}{}\n", "Threats", tracker.scores[TERM_THREATS]);
        out = std::format_to(out, " ------------+-------------+-------------+------------\n");
        out = std::format_to(out, "{:>12}{}\n\n", "Total", TermScore{score});
        out = std::format_to(
            out, "Evaluation:\t{:.2f}\n", double(result) / int(piece_value::pawn_mg));
        return out;
    }
};
