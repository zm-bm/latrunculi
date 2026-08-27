#include "eval/evaluation.hpp"

#include <algorithm>

#include "board/board.hpp"
#include "core/attacks.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/features.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"

namespace eval {

// Internal single-use implementation of the public evaluation entry points.
class Evaluator {
private:
    explicit Evaluator(const Board&);
    Evaluator(const Evaluator&) = delete;

    [[nodiscard]] EvalValue     evaluate();
    [[nodiscard]] FeatureRecord extract_features();

    const Board& board;

    struct AttackData {
        Bitboard by[N_COLORS][N_PIECETYPES]{};
        Bitboard by2[N_COLORS]{};
    } attacks;

    struct ZoneData {
        Bitboard outposts[N_COLORS]{};
        Bitboard mobility[N_COLORS]{};
        Bitboard king[N_COLORS]{};
    } zones;

    struct KingAttackersData {
        int       count[N_COLORS]{};
        EvalValue danger[N_COLORS]{};
    } king_attackers;

    struct ScoreData {
        TaperedScore mobility[N_COLORS]{};
        TaperedScore threats[N_COLORS]{};
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
    Bitboard outposts_zone(Bitboard pawns, Bitboard opp_pawns) const;

    template <Color C>
    Bitboard mobility_zone(Bitboard pawns, Bitboard opp_pawns, Square king_sq) const;

    Bitboard king_zone(Square king_sq) const;

    FeatureRecord* feature_record = nullptr;

    template <bool Collecting>
    EvalValue evaluate_impl();

    template <bool Collecting, Term term, Color C = WHITE>
    TaperedScore evaluate_term();

    template <bool Collecting, Color C>
    TaperedScore evaluate_pawns();

    template <bool Collecting, Color C, PieceType P>
    TaperedScore evaluate_pieces();

    template <Color C>
    TaperedScore evaluate_king_safety() const;

    template <Color C, PieceType P>
    void update_attacks(Bitboard moves);

    template <bool Collecting, Color C, PieceType P>
    void update_mobility(Bitboard moves);

    template <bool Collecting, Color C, PieceType P>
    void update_threats(const PieceContext& context);

    template <bool Collecting, Color C, PieceType P>
    TaperedScore update_attackers(const PieceContext& context, Bitboard moves);

    template <Color C, PieceType P>
    Bitboard piece_moves(const PieceContext& context) const;

    template <bool Collecting, Color C, PieceType P>
    TaperedScore evaluate_minor_piece(const PieceContext& context, Bitboard moves) const;

    template <bool Collecting, Color C>
    TaperedScore evaluate_bishop(const PieceContext& context) const;

    template <bool Collecting, Color C>
    TaperedScore evaluate_bishop_blockers(const PieceContext& context) const;

    template <bool Collecting, Color C>
    TaperedScore evaluate_rook(const PieceContext& context) const;

    template <bool Collecting, Color C>
    TaperedScore evaluate_queen(const PieceContext& context) const;

    template <Color C, PieceType P>
    bool discovery_attack(const PieceContext& context) const;

    template <Color C>
    TaperedScore evaluate_shelter(Square king_sq) const;

    template <Color C>
    TaperedScore evaluate_shelter_file(Bitboard pawns, Bitboard opp_pawns, File file) const;

    template <Color C>
    TaperedScore evaluate_danger(Square king_sq) const;

    template <Color C>
    int calculate_raw_danger(Square king_sq) const;

    template <PieceType P>
    int calculate_check_danger(Bitboard safe_checks, Bitboard all_checks) const;

    int       scale_factor(Color color) const;
    EvalValue taper_score(TaperedScore score) const;
    int       phase() const;

    template <Color C>
    void record_base_features();

    template <bool Collecting, Color C>
    TaperedScore linear_feature(feature::Id id, TaperedScore weight, int coefficient = 1) const;

    friend EvalValue     evaluate(const Board& board);
    friend FeatureRecord extract_features(const Board& board);
};

Evaluator::Evaluator(const Board& board) : board{board} {
    initialize<WHITE>();
    initialize<BLACK>();
}

template <bool Collecting>
EvalValue Evaluator::evaluate_impl() {
    if constexpr (Collecting) {
        record_base_features<WHITE>();
        record_base_features<BLACK>();
    }

    TaperedScore score;

    // Basic terms.
    score += evaluate_term<Collecting, Term::Material>();
    score += evaluate_term<Collecting, Term::PieceSquare>();
    score += evaluate_term<Collecting, Term::Pawns, WHITE>()
           - evaluate_term<Collecting, Term::Pawns, BLACK>();
    score += evaluate_term<Collecting, Term::Knights, WHITE>()
           - evaluate_term<Collecting, Term::Knights, BLACK>();
    score += evaluate_term<Collecting, Term::Bishops, WHITE>()
           - evaluate_term<Collecting, Term::Bishops, BLACK>();
    score += evaluate_term<Collecting, Term::Rooks, WHITE>()
           - evaluate_term<Collecting, Term::Rooks, BLACK>();
    score += evaluate_term<Collecting, Term::Queens, WHITE>()
           - evaluate_term<Collecting, Term::Queens, BLACK>();

    // Terms requiring data accumulated by basic terms.
    score += evaluate_term<Collecting, Term::KingSafety, WHITE>()
           - evaluate_term<Collecting, Term::KingSafety, BLACK>();
    score += evaluate_term<Collecting, Term::Mobility, WHITE>()
           - evaluate_term<Collecting, Term::Mobility, BLACK>();
    score += evaluate_term<Collecting, Term::Threats, WHITE>()
           - evaluate_term<Collecting, Term::Threats, BLACK>();

    const Color        side_to_move   = board.side_to_move();
    const Color        stronger_side  = score.eg < 0 ? BLACK : WHITE;
    const TaperedScore unscaled_score = score;
    score.eg = (score.eg * scale_factor(stronger_side)) / eval::scale_limit;

    const EvalValue tapered_value      = taper_score(score);
    const EvalValue side_to_move_value = tapered_value * (side_to_move == WHITE ? 1 : -1);
    const EvalValue final_value        = side_to_move_value + eval::tempo_bonus;

    if constexpr (Collecting)
        feature_record->complete(
            board, unscaled_score, score, tapered_value, side_to_move_value, final_value);

    return final_value;
}

EvalValue Evaluator::evaluate() {
    return evaluate_impl<false>();
}

FeatureRecord Evaluator::extract_features() {
    FeatureRecord result;
    feature_record = &result;
    evaluate_impl<true>();
    feature_record = nullptr;
    return result;
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
    zones.king[C]     = king_zone(king_sq);
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

    constexpr auto home_pawn_rank = bb::relative_rank<C>(RANK2);
    const Bitboard occupied       = bb::set(king_sq) | (pawns & home_pawn_rank);
    const Bitboard safe           = ~attacks::pawn_attacks<Opp>(opp_pawns);
    return (safe & ~occupied);
}

/// king zone: 3x3 king neighborhood for king safety evaluation
inline Bitboard Evaluator::king_zone(const Square king_sq) const {
    const File   file   = std::clamp(square::file_of(king_sq), FILE2, FILE7);
    const Rank   rank   = std::clamp(square::rank_of(king_sq), RANK2, RANK7);
    const Square center = square::make(file, rank);

    return attacks::piece_moves<KING>(center) | bb::set(center);
}

template <bool Collecting, Color C>
inline TaperedScore
Evaluator::linear_feature(feature::Id id, TaperedScore weight, int coefficient) const {
    if constexpr (Collecting) {
        constexpr int sign = C == WHITE ? 1 : -1;
        feature_record->add(id, sign * coefficient);
    }
    return weight * coefficient;
}

template <Color C>
inline void Evaluator::record_base_features() {
    constexpr int sign = C == WHITE ? 1 : -1;

    for (PieceType piece = PAWN; piece <= QUEEN; piece = PieceType(piece + 1))
        feature_record->add(feature::material(piece), sign * board.count(C, piece));

    Bitboard pieces = board.pieces(C);
    bb::scan<C>(pieces, [&](Square square) {
        const PieceType piece    = board.piece_type_on(square);
        const Square    relative = square::relative(square, C);
        feature_record->add(feature::piece_square(piece, relative), sign);
    });
}

/// dispatch a single eval term -> score
template <bool Collecting, Term term, Color C>
inline TaperedScore Evaluator::evaluate_term() {
    TaperedScore score;

    switch (term) {
    case Term::Material:    score = board.base_terms().material(); break;
    case Term::PieceSquare: score = board.base_terms().piece_square(); break;
    case Term::Pawns:       score = evaluate_pawns<Collecting, C>(); break;
    case Term::Knights:     score = evaluate_pieces<Collecting, C, KNIGHT>(); break;
    case Term::Bishops:     score = evaluate_pieces<Collecting, C, BISHOP>(); break;
    case Term::Rooks:       score = evaluate_pieces<Collecting, C, ROOK>(); break;
    case Term::Queens:      score = evaluate_pieces<Collecting, C, QUEEN>(); break;
    case Term::KingSafety:  score = evaluate_king_safety<C>(); break;
    case Term::Mobility:    score = scores.mobility[C]; break;
    case Term::Threats:     score = scores.threats[C]; break;
    default:                break;
    }

    if constexpr (Collecting)
        feature_record->record(term, C, score);

    return score;
}

/// eval pawn structure: isolated + backward + doubled + passed
template <bool Collecting, Color C>
TaperedScore Evaluator::evaluate_pawns() {
    constexpr Color Opp = ~C;

    const Bitboard pawns         = board.pieces<PAWN>(C);
    const Bitboard opp_pawns     = board.pieces<PAWN>(Opp);
    const Bitboard left_attacks  = attacks::pawn_shift<pawn_delta::left, C>(pawns);
    const Bitboard right_attacks = attacks::pawn_shift<pawn_delta::right, C>(pawns);
    const Bitboard pawn_attacks  = left_attacks | right_attacks;
    const Bitboard pawn_attacks2 = left_attacks & right_attacks;

    attacks.by2[C] |= pawn_attacks2 | (attacks.by[C][all_pieces_slot] & pawn_attacks);
    attacks.by[C][all_pieces_slot] |= pawn_attacks;
    attacks.by[C][PAWN] |= pawn_attacks;

    // isolated pawns: no friendly pawns on adjacent files
    const Bitboard pawn_files = bb::fill(pawns);
    const Bitboard isolated_pawns =
        (pawns & ~bb::shift_west(pawn_files)) & (pawns & ~bb::shift_east(pawn_files));
    TaperedScore score = linear_feature<Collecting, C>(
        feature::isolated_pawn, eval::isolated_pawn, bb::count(isolated_pawns));

    // backwards pawns: non-isolated pawns that can't advance safely
    const Bitboard stops       = attacks::pawn_shift<pawn_delta::push, C>(pawns);
    const Bitboard attack_span = bb::attack_span<C>(pawns);
    const Bitboard opp_attacks = attacks::pawn_attacks<Opp>(opp_pawns);
    const Bitboard backwards_pawns =
        attacks::pawn_shift<pawn_delta::push, Opp>(stops & opp_attacks & ~attack_span)
        & ~isolated_pawns;
    score += linear_feature<Collecting, C>(
        feature::backward_pawn, eval::backward_pawn, bb::count(backwards_pawns));

    // doubled pawns: unsupported pawn with friendly pawns behind
    Bitboard pawns_behind  = pawns & bb::span_front<C>(pawns);
    Bitboard doubled_pawns = pawns_behind & ~pawn_attacks;
    score += linear_feature<Collecting, C>(
        feature::doubled_pawn, eval::doubled_pawn, bb::count(doubled_pawns));

    // passed pawns: no opposing pawn ahead on the same or an adjacent file
    const Bitboard passed_pawns = pawns & ~bb::full_span<Opp>(opp_pawns);
    bb::scan<C>(passed_pawns, [&](const Square square) {
        const Rank rank = square::relative_rank(square, C);
        score += linear_feature<Collecting, C>(feature::passed_pawn(rank), eval::passed_pawn[rank]);
    });

    return score;
}

/// eval all pieces of type p for color c
template <bool Collecting, Color C, PieceType P>
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

        const Bitboard moves = piece_moves<C, P>(context);
        update_attacks<C, P>(moves);
        update_mobility<Collecting, C, P>(moves);
        update_threats<Collecting, C, P>(context);
        score += update_attackers<Collecting, C, P>(context, moves);

        if constexpr (P == BISHOP || P == KNIGHT) {
            score += evaluate_minor_piece<Collecting, C, P>(context, moves);
        }

        if constexpr (P == BISHOP) {
            score += evaluate_bishop<Collecting, C>(context);
        } else if constexpr (P == ROOK) {
            score += evaluate_rook<Collecting, C>(context);
        } else if constexpr (P == QUEEN) {
            score += evaluate_queen<Collecting, C>(context);
        }
    });

    if constexpr (P == BISHOP) {
        if (board.count(C, BISHOP) > 1)
            score += linear_feature<Collecting, C>(
                feature::piece_feature(feature::PieceFeature::BishopPair), eval::bishop_pair);
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

    const auto consider_shelter = [&](Square square) {
        const TaperedScore candidate = evaluate_shelter<C>(square);
        if (candidate.mg > shelter.mg)
            shelter = candidate;
    };

    if (board.has_castling_right(CASTLE_KINGSIDE, C))
        consider_shelter(kingside_sq);
    if (board.has_castling_right(CASTLE_QUEENSIDE, C))
        consider_shelter(queenside_sq);

    const TaperedScore danger = evaluate_danger<C>(king_sq);

    return shelter - danger;
}

/// merge moves into attack bitboards
template <Color C, PieceType P>
inline void Evaluator::update_attacks(const Bitboard moves) {
    attacks.by2[C] |= (attacks.by[C][all_pieces_slot] & moves);
    attacks.by[C][all_pieces_slot] |= moves;
    attacks.by[C][P] |= moves;
}

/// add mobility bonus for # of moves
template <bool Collecting, Color C, PieceType P>
inline void Evaluator::update_mobility(const Bitboard moves) {
    const int move_count = bb::count(moves & zones.mobility[C]);
    scores.mobility[C] += linear_feature<Collecting, C>(feature::mobility(P, move_count),
                                                        eval::mobility[P][move_count]);
}

/// penalize weak pieces if outnumbered or attacked by a lower-value pawn
template <bool Collecting, Color C, PieceType P>
inline void Evaluator::update_threats(const PieceContext& ctx) {
    constexpr Color Opp = ~C;

    const Bitboard defenders        = board.attacks_to(ctx.square, C);
    const Bitboard attackers        = board.attacks_to(ctx.square, Opp);
    const bool     attacked_by_pawn = (attackers & board.pieces<PAWN>(Opp)) != 0;

    if (attacked_by_pawn || bb::count(attackers) > bb::count(defenders)) {
        scores.threats[C] +=
            linear_feature<Collecting, C>(feature::weak_piece(P), eval::weak_piece[P]);
    }
}

/// update king attackers with attacks on enemy king zone
template <bool Collecting, Color C, PieceType P>
inline TaperedScore Evaluator::update_attackers(const PieceContext& ctx, const Bitboard moves) {
    constexpr Color Opp = ~C;

    if (moves & zones.king[Opp]) {
        king_attackers.count[Opp]++;
        king_attackers.danger[Opp] += eval::king_zone_attack_danger[P];
    } else if constexpr (P == BISHOP || P == ROOK) {
        const Bitboard xray_moves = attacks::piece_moves<P>(ctx.square, ctx.pawns);
        if (zones.king[Opp] & xray_moves)
            return linear_feature<Collecting, C>(
                feature::piece_feature(feature::PieceFeature::KingZoneXrayAttack),
                eval::king_zone_xray_attack);
    }

    return TaperedScore::Zero;
}

template <Color C, PieceType P>
inline Bitboard Evaluator::piece_moves(const PieceContext& ctx) const {
    Bitboard       moves  = attacks::piece_moves<P>(ctx.square, ctx.occupied);
    const Bitboard pinned = board.blockers(C) & ctx.piece_bb;
    if (pinned)
        moves &= square::collinear(board.king_sq(C), ctx.square);

    return moves;
}

/// minor piece eval: outposts + pawn shields
template <bool Collecting, Color C, PieceType P>
inline TaperedScore Evaluator::evaluate_minor_piece(const PieceContext& ctx,
                                                    const Bitboard      moves) const {
    constexpr Color Opp = ~C;
    static_assert(P == KNIGHT || P == BISHOP);

    TaperedScore score;
    if (bb::contains(zones.outposts[C], ctx.square)) {
        if constexpr (P == KNIGHT)
            score += linear_feature<Collecting, C>(
                feature::piece_feature(feature::PieceFeature::KnightOutpost), eval::knight_outpost);
        else
            score += linear_feature<Collecting, C>(
                feature::piece_feature(feature::PieceFeature::BishopOutpost), eval::bishop_outpost);
    } else if constexpr (P == KNIGHT) {
        if (moves & zones.outposts[C]) {
            score += linear_feature<Collecting, C>(
                feature::piece_feature(feature::PieceFeature::ReachableOutpost),
                eval::reachable_outpost);
        }
    }

    if (bb::contains(attacks::pawn_shift<pawn_delta::push, Opp>(ctx.pawns), ctx.square)) {
        score += linear_feature<Collecting, C>(
            feature::piece_feature(feature::PieceFeature::MinorPawnShield),
            eval::minor_pawn_shield);
    }

    return score;
}

/// bishop eval: long diagonals + pawn block penalty
template <bool Collecting, Color C>
inline TaperedScore Evaluator::evaluate_bishop(const PieceContext& ctx) const {
    TaperedScore score;

    const Bitboard xray_moves = attacks::piece_moves<BISHOP>(ctx.square, ctx.pawns);
    if (bb::is_many(eval::masks::center_squares & xray_moves))
        score += linear_feature<Collecting, C>(
            feature::piece_feature(feature::PieceFeature::BishopLongDiagonal),
            eval::bishop_long_diagonal);

    score += evaluate_bishop_blockers<Collecting, C>(ctx);

    return score;
}

/// penalize bishops blocked by pawns on the same color squares
template <bool Collecting, Color C>
inline TaperedScore Evaluator::evaluate_bishop_blockers(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const bool     dark_square = ctx.piece_bb & eval::masks::dark_squares;
    const Bitboard color_mask =
        dark_square ? eval::masks::dark_squares : eval::masks::light_squares;
    const Bitboard color_pawns = ctx.pawns & color_mask;
    const int      pawn_count  = bb::count(color_pawns);

    const Bitboard blocked_pawns =
        ctx.pawns & attacks::pawn_shift<pawn_delta::push, Opp>(ctx.occupied);
    const Bitboard pawn_chain = ctx.piece_bb & attacks::pawn_attacks<C>(ctx.pawns);
    const int blocking_factor = bb::count(blocked_pawns & eval::masks::center_files) + !pawn_chain;

    return linear_feature<Collecting, C>(
        feature::piece_feature(feature::PieceFeature::BishopBlockers),
        eval::bishop_blockers,
        pawn_count * blocking_factor);
}

/// rook eval: open/semi-open file bonus, closed+blocked penalty
template <bool Collecting, Color C>
inline TaperedScore Evaluator::evaluate_rook(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    const Bitboard file_mask  = bb::file(square::file_of(ctx.square));
    const Bitboard file_pawns = ctx.pawns & file_mask;

    const bool semi_open = !file_pawns;
    if (semi_open) {
        bool fully_open = !(ctx.opp_pawns & file_mask);
        return linear_feature<Collecting, C>(feature::rook_open(fully_open),
                                             eval::rook_open_file[fully_open]);
    }

    const bool blocked = file_pawns & attacks::pawn_shift<pawn_delta::push, Opp>(ctx.occupied);
    if (blocked) {
        return linear_feature<Collecting, C>(
            feature::piece_feature(feature::PieceFeature::RookClosedFile), eval::rook_closed_file);
    }

    return TaperedScore::Zero;
}

/// queen eval: penalize discovered attacks
template <bool Collecting, Color C>
inline TaperedScore Evaluator::evaluate_queen(const PieceContext& ctx) const {
    if (discovery_attack<C, BISHOP>(ctx) || discovery_attack<C, ROOK>(ctx)) {
        return linear_feature<Collecting, C>(
            feature::piece_feature(feature::PieceFeature::QueenDiscoveredAttack),
            eval::queen_discovered_attack);
    }
    return TaperedScore::Zero;
}

/// penalize rays with exactly one intervening piece that can move away
template <Color C, PieceType P>
inline bool Evaluator::discovery_attack(const PieceContext& ctx) const {
    constexpr Color Opp = ~C;

    Bitboard attackers = board.pieces<P>(Opp) & attacks::piece_moves<P>(ctx.square, 0);
    while (attackers) {
        const Square   attacker       = bb::lsb_pop(attackers);
        const Bitboard pieces_between = square::between(ctx.square, attacker) & ctx.occupied;

        if (pieces_between && !bb::is_many(pieces_between))
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

    const Rank rank = file_pawns ? square::relative_rank(bb::frontmost<Opp>(file_pawns), C) : RANK1;
    const Rank opp_rank =
        file_opp_pawns ? square::relative_rank(bb::frontmost<Opp>(file_opp_pawns), C) : RANK1;
    const bool blocked = file_pawns && (rank + 1 == opp_rank);

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

    const Bitboard defended        = attacks.by[C][all_pieces_slot];
    const Bitboard attacked        = attacks.by[Opp][all_pieces_slot];
    const Bitboard kq_defense      = attacks.by[C][QUEEN] | attacks.by[C][KING];
    const Bitboard weak_defense    = (~defended | kq_defense) & attacked & ~attacks.by2[C];
    const Bitboard attacker_pieces = board.pieces(Opp);
    const Bitboard safe_checks = ~attacker_pieces & (~defended | (weak_defense & attacks.by2[Opp]));

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

    danger += king_attackers.danger[C] * king_attackers.count[C];
    danger += eval::weak_king_zone_danger * bb::count(zones.king[C] & weak_defense);
    danger += eval::pinned_piece_danger * bb::count(board.blockers(C) & board.pieces(C));

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

// Integer numerator for scaling endgame evaluation toward zero in drawish pawn endings.
int Evaluator::scale_factor(Color color) const {
    const int pawn_count = board.count(color, PAWN);
    return std::min(eval::scale_limit, eval::scale_base + eval::scale_per_pawn * pawn_count);
}

// Blend middlegame and endgame scores based on game phase.
EvalValue Evaluator::taper_score(TaperedScore score) const {
    const int mg_phase = phase();
    const int eg_phase = eval::phase_limit - mg_phase;

    return ((score.mg * mg_phase) + (score.eg * eg_phase)) / eval::phase_limit;
}

// Game phase from MG piece values: zero is endgame and phase_limit is middlegame.
int Evaluator::phase() const {
    const int non_pawn_material = board.non_pawn_material(WHITE) + board.non_pawn_material(BLACK);
    const int material = std::clamp(non_pawn_material, eval::material_eg, eval::material_mg);

    return ((material - eval::material_eg) * eval::phase_limit)
         / (eval::material_mg - eval::material_eg);
}

EvalValue evaluate(const Board& board) {
    return Evaluator(board).evaluate();
}

FeatureRecord extract_features(const Board& board) {
    return Evaluator(board).extract_features();
}

} // namespace eval
