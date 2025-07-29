#pragma once

#include <functional>
#include <string>

#include "bb.hpp"
#include "board.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "score.hpp"

class EvaluatorDebug;

// Evaluates chess positions using material, mobility, king safety + other positional factors
class Evaluator {
public:
    using TermCallback = std::function<void(EvalTerm, Color, Score)>;

    Evaluator() = delete;
    Evaluator(const Board&, TermCallback callback = nullptr);
    int evaluate();

private:
    const Board& board;
    TermCallback callback = nullptr;

    struct AttackData {
        uint64_t by[N_COLORS][N_PIECES] = {{0}};
        uint64_t by2[N_COLORS]          = {0};
    } attacks;

    struct ZoneData {
        uint64_t outposts[N_COLORS] = {0};
        uint64_t mobility[N_COLORS] = {0};
        uint64_t king[N_COLORS]     = {0};
    } zones;

    struct KingAttackersData {
        int count[N_COLORS] = {0};
        int value[N_COLORS] = {0};
    } king_attackers;

    struct ScoreData {
        Score mobility[N_COLORS] = {Score::Zero};
        Score threats[N_COLORS]  = {Score::Zero};
        Score final_score        = Score::Zero;
        int   final_value        = 0;
    } scores;

    struct PieceContext {
        Square   square    = INVALID;
        uint64_t piece_bb  = 0;
        uint64_t occupied  = 0;
        uint64_t pawns     = 0;
        uint64_t opp_pawns = 0;
    };

    template <Color C>
    void initialize();

    template <Color C>
    uint64_t outposts_zone(const uint64_t pawns, const uint64_t opp_pawns) const;

    template <Color C>
    uint64_t
    mobility_zone(const uint64_t pawns, const uint64_t opp_pawns, const Square king_sq) const;

    template <Color C>
    uint64_t king_zone(const Square king_sq) const;

    // Core evaluation methods

    template <EvalTerm Term, Color C = WHITE>
    Score evaluate_term();

    template <Color C>
    Score evaluate_pawns();

    template <Color C, PieceType P>
    Score evaluate_pieces();

    template <Color>
    Score evaluate_king_safety() const;

    template <Color C, PieceType P>
    void update_attacks(const uint64_t moves);

    template <Color C, PieceType P>
    void update_mobility(const uint64_t moves);

    template <Color C, PieceType P>
    void update_threats(const PieceContext& ctx);

    template <Color C, PieceType P>
    Score update_attackers(const PieceContext& ctx, const uint64_t moves);

    // Piece specific evaluation methods

    template <Color C, PieceType P>
    uint64_t get_moves(const PieceContext& ctx) const;

    template <Color C, PieceType P>
    Score evaluate_minor_pieces(const PieceContext& ctx, const uint64_t moves) const;

    template <Color C>
    Score evaluate_bishops(const PieceContext& ctx) const;

    template <Color C>
    Score evaluate_bishop_blockers(const PieceContext& ctx) const;

    template <Color C>
    Score evaluate_rook(const PieceContext& ctx) const;

    template <Color C>
    Score evaluate_queen(const PieceContext& ctx) const;

    template <Color C, PieceType P>
    bool discovery_attack(const PieceContext& ctx) const;

    // King evaluation methods

    template <Color C>
    Score evaluate_shelter(const Square king_sq) const;

    template <Color C>
    Score
    evaluate_shelter_file(const uint64_t pawns, const uint64_t opp_pawns, const File file) const;

    template <Color C>
    Score evaluate_danger(const Square king_sq) const;

    template <Color C>
    int calculate_raw_danger(const Square king_sq) const;

    template <PieceType P>
    int calculate_check_danger(const uint64_t safe_checks, const uint64_t all_checks) const;

    // Helpers

    float scale_factor(const Color c) const;
    int   taper_score(const Score score) const;
    int   phase() const;

    friend class EvaluatorDebug;
    friend class EvaluatorTest;
    friend std::formatter<EvaluatorDebug>;
};

/// returns static evaluation of a chess board
inline int evaluate(const Board& board) {
    return Evaluator(board).evaluate();
}

inline Evaluator::Evaluator(const Board& b, TermCallback callback) : board{b}, callback{callback} {
    initialize<WHITE>();
    initialize<BLACK>();
}

inline int Evaluator::evaluate() {
    Score score;

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

    Color c   = board.side_to_move();
    score.eg *= scale_factor(c);

    scores.final_score  = score;
    scores.final_value  = taper_score(score) * (c == WHITE ? 1 : -1);
    scores.final_value += eval::tempo_bonus;

    return scores.final_value;
}

/// prep zone data + seed king attacks
template <Color C>
inline void Evaluator::initialize() {
    constexpr Color Opp = ~C;

    const Square   king_sq    = board.king_sq(C);
    const uint64_t king_moves = bb::moves<KING>(king_sq);
    update_attacks<C, KING>(king_moves);

    const uint64_t pawns     = board.pieces<PAWN>(C);
    const uint64_t opp_pawns = board.pieces<PAWN>(Opp);

    zones.outposts[C] = outposts_zone<C>(pawns, opp_pawns);
    zones.mobility[C] = mobility_zone<C>(pawns, opp_pawns, king_sq);
    zones.king[C]     = king_zone<C>(king_sq);
}

/// outposts mask: behind enemy pawns, supported by friendly pawns, on ranks 4-6
template <Color C>
inline uint64_t Evaluator::outposts_zone(const uint64_t pawns, const uint64_t opp_pawns) const {
    constexpr Color Opp = ~C;

    const uint64_t behind_pawns = ~bb::attack_span<Opp>(opp_pawns);
    const uint64_t supported    = bb::pawn_attacks<C>(pawns);
    constexpr auto outpost_mask = (C == WHITE) ? masks::w_outposts : masks::b_outposts;
    return (behind_pawns & supported & outpost_mask);
}

/// mobility mask: safe from enemy pawns, not occupied by the king or rank 2 pawns
template <Color C>
inline uint64_t Evaluator::mobility_zone(const uint64_t pawns,
                                         const uint64_t opp_pawns,
                                         const Square   king_sq) const {
    constexpr Color Opp = ~C;

    constexpr auto rank2    = (C == WHITE) ? bb::rank(RANK2) : bb::rank(RANK7);
    const uint64_t occupied = bb::set(king_sq) | (pawns & rank2);
    const uint64_t safe     = ~bb::pawn_attacks<Opp>(opp_pawns);
    return (safe & ~occupied);
}

/// king zone: 3x3 king neighborhood for king safety evaluation
template <Color C>
inline uint64_t Evaluator::king_zone(const Square king_sq) const {
    const File   file   = std::clamp(file_of(king_sq), FILE2, FILE7);
    const Rank   rank   = std::clamp(rank_of(king_sq), RANK2, RANK7);
    const Square center = make_square(file, rank);

    return bb::moves<KING>(center) | bb::set(center);
}

/// dispatch a single eval term -> score
template <EvalTerm Term, Color C>
inline Score Evaluator::evaluate_term() {
    Score score;

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
Score Evaluator::evaluate_pawns() {
    constexpr Color Opp = ~C;

    const uint64_t pawns         = board.pieces<PAWN>(C);
    const uint64_t opp_pawns     = board.pieces<PAWN>(Opp);
    const uint64_t left_attacks  = bb::pawn_moves<PAWN_LEFT, C>(pawns);
    const uint64_t right_attacks = bb::pawn_moves<PAWN_RIGHT, C>(pawns);
    const uint64_t pawn_attacks  = left_attacks | right_attacks;
    const uint64_t pawn_attacks2 = left_attacks & right_attacks;

    attacks.by2[C]            |= pawn_attacks2 | (attacks.by[C][ALL_PIECES] & pawn_attacks);
    attacks.by[C][ALL_PIECES] |= pawn_attacks;
    attacks.by[C][PAWN]       |= pawn_attacks;

    // isolated pawns: no friendly pawns on adjacent files
    const uint64_t pawn_files = bb::fill(pawns);
    const uint64_t iso_pawns =
        (pawns & ~bb::shift_west(pawn_files)) & (pawns & ~bb::shift_east(pawn_files));
    Score score = eval::iso_pawn * bb::count(iso_pawns);

    // backwards pawns: pawns that can't advance safely
    const uint64_t stops       = bb::pawn_moves<PAWN_PUSH, C>(pawns);
    const uint64_t attack_span = bb::attack_span<C>(pawns);
    const uint64_t opp_attacks = bb::pawn_attacks<Opp>(opp_pawns);
    const uint64_t backwards_pawns =
        bb::pawn_moves<PAWN_PUSH, Opp>(stops & opp_attacks & ~attack_span);
    score += eval::backward_pawn * bb::count(backwards_pawns);

    // doubled pawns: unsupported pawn with friendly pawns behind
    uint64_t pawns_behind   = pawns & bb::span_front<C>(pawns);
    uint64_t doubled_pawns  = pawns_behind & ~pawn_attacks;
    score                  += eval::doubled_pawn * bb::count(doubled_pawns);

    return score;
}

/// eval all pieces of type p for color c
template <Color C, PieceType P>
Score Evaluator::evaluate_pieces() {
    constexpr Color Opp = ~C;

    const uint64_t occupied  = board.occupancy();
    const uint64_t pawns     = board.pieces<PAWN>(C);
    const uint64_t opp_pawns = board.pieces<PAWN>(Opp);

    Score    score;
    uint64_t piece_bb = board.pieces<P>(C);
    bb::scan<C>(piece_bb, [&](Square sq) {
        const PieceContext context{.square    = sq,
                                   .piece_bb  = bb::set(sq),
                                   .occupied  = occupied,
                                   .pawns     = pawns,
                                   .opp_pawns = opp_pawns};

        const uint64_t moves = get_moves<C, P>(context);
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
Score Evaluator::evaluate_king_safety() const {
    constexpr Square kingside_sq  = C == WHITE ? G1 : G8;
    constexpr Square queenside_sq = C == WHITE ? C1 : C8;

    const Square king_sq = board.king_sq(C);
    Score        shelter = evaluate_shelter<C>(king_sq);

    if (board.can_castle_kingside(C))
        shelter = std::max(shelter, evaluate_shelter<C>(kingside_sq));
    if (board.can_castle_queenside(C))
        shelter = std::max(shelter, evaluate_shelter<C>(queenside_sq));

    const Score danger = evaluate_danger<C>(king_sq);

    return shelter - danger;
}

/// merge moves into attack bitboards
template <Color C, PieceType P>
inline void Evaluator::update_attacks(const uint64_t moves) {
    attacks.by2[C]            |= (attacks.by[C][ALL_PIECES] & moves);
    attacks.by[C][ALL_PIECES] |= moves;
    attacks.by[C][P]          |= moves;
}

/// add mobility bonus for # of moves
template <Color C, PieceType P>
inline void Evaluator::update_mobility(const uint64_t moves) {
    const uint64_t move_count  = bb::count(moves & zones.mobility[C]);
    scores.mobility[C]        += eval::mobility[P][move_count];
}

/// penalize weak pieces if attackers > defenders
template <Color C, PieceType P>
inline void Evaluator::update_threats(const PieceContext& ctx) {
    constexpr Color Opp = ~C;

    const uint64_t defenders = board.attacks_to(ctx.square, C);
    const uint64_t attackers = board.attacks_to(ctx.square, Opp);

    if (bb::count(attackers) > bb::count(defenders)) {
        scores.threats[C] += eval::weak_piece[P];
    }
}

/// update king attackers with attacks on enemy king zone
template <Color C, PieceType P>
inline Score Evaluator::update_attackers(const PieceContext& ctx, const uint64_t moves) {
    constexpr Color Opp = ~C;

    if (moves & zones.king[Opp]) {
        king_attackers.count[Opp]++;
        king_attackers.value[Opp] += eval::kingzone_att_danger[P];
    } else if constexpr (P == BISHOP || P == ROOK) {
        const uint64_t xray_moves = bb::moves<P>(ctx.square, ctx.pawns);
        if (zones.king[Opp] & xray_moves)
            return eval::kingzone_xray_att;
    }

    return Score::Zero;
}

template <Color C, PieceType P>
inline uint64_t Evaluator::get_moves(const PieceContext& ctx) const {
    uint64_t       moves  = bb::moves<P>(ctx.square, ctx.occupied);
    const uint64_t pinned = board.blockers(C) & ctx.piece_bb;
    if (pinned)
        moves &= bb::collinear(board.king_sq(C), ctx.square);

    return moves;
}

/// minor piece eval: outposts + pawn shields
template <Color C, PieceType P>
inline Score Evaluator::evaluate_minor_pieces(const PieceContext& ctx, const uint64_t moves) const {
    constexpr Color Opp = ~C;
    static_assert(P == KNIGHT || P == BISHOP);

    Score score;
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

    if (ctx.piece_bb & bb::pawn_moves<PAWN_PUSH, Opp>(ctx.pawns)) {
        score += eval::minor_pawn_shield;
    }

    return score;
}

/// bishop eval: long diagonals + pawn block penalty
template <Color C>
inline Score Evaluator::evaluate_bishops(const PieceContext& ctx) const {
    Score score;

    const uint64_t xray_moves = bb::moves<BISHOP>(ctx.square, ctx.pawns);
    if (bb::is_many(masks::center_squares & xray_moves))
        score += eval::bishop_long_diag;

    score += evaluate_bishop_blockers<C>(ctx);

    return score;
}

/// penalize bishops blocked by pawns on the same color squares
template <Color C>
inline Score Evaluator::evaluate_bishop_blockers(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const bool     dark_square = ctx.piece_bb & masks::dark_squares;
    const uint64_t color_mask  = dark_square ? masks::dark_squares : masks::light_squares;
    const uint64_t color_pawns = ctx.pawns & color_mask;
    const int      pawn_count  = bb::count(color_pawns);

    const uint64_t blocked_pawns   = ctx.pawns & bb::pawn_moves<PAWN_PUSH, Opp>(ctx.occupied);
    const uint64_t pawn_chain      = ctx.piece_bb & bb::pawn_attacks<C>(ctx.pawns);
    const int      blocking_factor = bb::count(blocked_pawns & masks::center_files) + !pawn_chain;

    return eval::bishop_blockers * (pawn_count * blocking_factor);
}

/// rook eval: open/semi-open file bonus, closed+blocked penalty
template <Color C>
inline Score Evaluator::evaluate_rook(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const uint64_t file_mask  = bb::file(file_of(ctx.square));
    const uint64_t file_pawns = ctx.pawns & file_mask;

    const bool semi_open = !file_pawns;
    if (semi_open) {
        bool fully_open = !(ctx.opp_pawns & file_mask);
        return eval::rook_open_file[fully_open];
    }

    const bool blocked = file_pawns & bb::pawn_moves<PAWN_PUSH, Opp>(ctx.occupied);
    if (blocked) {
        return eval::rook_closed_file;
    }

    return Score::Zero;
}

/// queen eval: penalize discovered attacks
template <Color C>
inline Score Evaluator::evaluate_queen(const PieceContext& ctx) const {
    if (discovery_attack<C, BISHOP>(ctx) || discovery_attack<C, ROOK>(ctx)) {
        return eval::queen_discover_att;
    }
    return Score::Zero;
}

/// penalize discovered attacks on the piece
template <Color C, PieceType P>
inline bool Evaluator::discovery_attack(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    uint64_t attackers = board.pieces<P>(Opp) & bb::moves<P>(ctx.square, 0);
    while (attackers) {
        const Square   attacker = bb::lsb_pop(attackers);
        const uint64_t between  = bb::between(ctx.square, attacker) & ctx.occupied;

        if (!bb::is_many(between))
            return true;
    }

    return false;
}

/// king pawn-shelter score: friendly pawn shield vs enemy pawn storm
template <Color C>
Score Evaluator::evaluate_shelter(const Square king_sq) const {
    constexpr Color Opp = ~C;

    const File king_file = file_of(king_sq);
    const Rank king_rank = rank_of(king_sq);
    const auto pawns     = board.pieces<PAWN>(C);
    const auto opp_pawns = board.pieces<PAWN>(Opp);
    const auto pawn_mask = bb::span_front<C>(bb::rank(king_rank));

    const uint64_t pawns_ahead     = pawns & pawn_mask & ~bb::pawn_attacks<Opp>(opp_pawns);
    const uint64_t opp_pawns_ahead = opp_pawns & pawn_mask;
    const File     file            = std::clamp(king_file, FILE2, FILE7);

    Score score;
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file - 1);
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file);
    score += evaluate_shelter_file<C>(pawns_ahead, opp_pawns_ahead, file + 1);

    score += eval::king_file[king_file];

    const uint64_t file_mask     = bb::file(king_file);
    const bool     open_file     = !(pawns & file_mask);
    const bool     opp_open_file = !(opp_pawns & file_mask);

    score += eval::king_open_file[open_file][opp_open_file];

    return score;
}

/// shelter score for one file: friendly pawn rank + enemy pawn rank
template <Color C>
inline Score Evaluator::evaluate_shelter_file(const uint64_t pawns,
                                              const uint64_t opp_pawns,
                                              const File     file) const {
    constexpr Color Opp = ~C;

    const uint64_t file_pawns     = pawns & bb::file(file);
    const uint64_t file_opp_pawns = opp_pawns & bb::file(file);

    const Square pawn_sq     = bb::select<Opp>(file_pawns);
    const Square opp_pawn_sq = bb::select<Opp>(file_opp_pawns);

    const Rank rank     = file_pawns ? relative_rank(pawn_sq, C) : RANK1;
    const Rank opp_rank = file_opp_pawns ? relative_rank(opp_pawn_sq, C) : RANK1;
    const bool blocked  = file_pawns && (rank + 1 == opp_rank);

    Score score;
    score += eval::pawn_shelter[rank];
    score += eval::pawn_storm[blocked][opp_rank];

    return score;
}

/// convert raw danger into score {mg=quadratic scaling, eg=linear scaling}
template <Color C>
inline Score Evaluator::evaluate_danger(Square king_sq) const {
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

    const uint64_t defended     = attacks.by[C][ALL_PIECES];
    const uint64_t attacked     = attacks.by[Opp][ALL_PIECES];
    const uint64_t kq_defense   = attacks.by[C][QUEEN] | attacks.by[C][KING];
    const uint64_t weak_defense = (~defended | kq_defense) & attacked & ~attacks.by2[C];
    const uint64_t our_pieces   = board.pieces<ALL_PIECES>(Opp);
    const uint64_t safe_checks  = ~our_pieces & (~defended | (weak_defense & attacks.by2[Opp]));

    const uint64_t knight_moves  = attacks.by[Opp][KNIGHT];
    const uint64_t knight_checks = bb::moves<KNIGHT>(king_sq) & knight_moves;
    danger += calculate_check_danger<KNIGHT>(knight_checks & safe_checks, knight_checks);

    const uint64_t occupancy   = board.occupancy();
    const uint64_t line_checks = bb::moves<ROOK>(king_sq, occupancy);
    const uint64_t diag_checks = bb::moves<BISHOP>(king_sq, occupancy);

    const uint64_t rook_checks = line_checks & attacks.by[Opp][ROOK];
    danger += calculate_check_danger<ROOK>(rook_checks & safe_checks, rook_checks);

    const uint64_t queen_checks      = (line_checks | diag_checks) & attacks.by[Opp][QUEEN];
    const uint64_t bad_queen_checks  = rook_checks | attacks.by[C][QUEEN];
    const uint64_t safe_queen_checks = queen_checks & safe_checks & ~bad_queen_checks;
    danger += calculate_check_danger<QUEEN>(safe_queen_checks, queen_checks);

    const uint64_t bishop_checks      = diag_checks & attacks.by[Opp][BISHOP];
    const uint64_t safe_bishop_checks = bishop_checks & safe_checks & ~queen_checks;
    danger += calculate_check_danger<BISHOP>(safe_bishop_checks, bishop_checks);

    danger += king_attackers.value[C] * king_attackers.count[C];
    danger += eval::weak_kingzone_danger * bb::count(zones.king[C] & weak_defense);
    danger += eval::pinned_piece_danger * bb::count(board.blockers(C));

    return danger;
}

/// scale danger non-linearly, 1 check = 1×, 2 checks = ~1.3×, 3 checks = 1.5×, asymptotes to 2×
template <PieceType P>
inline int Evaluator::calculate_check_danger(const uint64_t safe_checks,
                                             const uint64_t all_checks) const {
    const int  count  = bb::count(safe_checks ? safe_checks : all_checks);
    const auto danger = safe_checks ? eval::safe_check_danger : eval::unsafe_check_danger;

    return (danger[P] * count * 2) / (count + 1);
}

// scale endgame eval toward 0 in draw-ish pawn endings,
inline float Evaluator::scale_factor(const Color c) const {
    const int pawn_count = board.count(c, PAWN);
    return std::min(eval::scale_limit, 36 + 5 * pawn_count) / float(eval::scale_limit);
}

// blend midgame / endgame scores based on game phase
inline int Evaluator::taper_score(const Score score) const {
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
    void  add_score(Score score, Color color);
    Score white    = Score::Zero;
    Score black    = Score::Zero;
    bool  has_both = false;
};

struct ScoreTracker {
    void      add_score(EvalTerm term, Score score, Color color);
    TermScore scores[N_TERMS];
};

// evaluator + per-term score breakdown (for logging/debug)
class EvaluatorDebug {
public:
    EvaluatorDebug(const Board& b);
    int evaluate() { return evaluator.evaluate(); }
    friend std::formatter<EvaluatorDebug>;

private:
    ScoreTracker score_tracker;
    Evaluator    evaluator;
};

template <>
struct std::formatter<Score> : std::formatter<std::string_view> {
    auto format(const Score& score, std::format_context& ctx) const {
        return std::format_to(ctx.out(),
                              "{:5.2f} {:5.2f}",
                              double(score.mg) / int(PAWN_MG),
                              double(score.eg) / int(PAWN_MG));
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
        out = std::format_to(out, "Evaluation:\t{:.2f}\n", double(result) / int(PAWN_MG));
        return out;
    }
};
