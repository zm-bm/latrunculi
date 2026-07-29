#include "board/board.hpp"

#include "core/attacks.hpp"
#include "core/move_geometry.hpp"
#include "core/square.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>

namespace {

constexpr int fifty_move_rule_halfmoves = 100;

bool is_king_safe_after_enpassant(const Board& board, Square from, Square target) noexcept {
    const Color  side            = board.side_to_move();
    const Square captured_square = move_geometry::enpassant_captured_square(target, side);
    Bitboard     occupancy       = board.occupancy();
    bb::move(occupancy, from, target);
    bb::remove(occupancy, captured_square);

    // Only occupancy is simulated. The unchanged piece bitboards still contain
    // the captured pawn, so exclude it from the resulting attackers.
    Bitboard attackers = board.attacks_to(board.king_sq(side), ~side, occupancy);
    bb::remove(attackers, captured_square);
    return !attackers;
}

[[gnu::always_inline]] inline Bitboard direct_check_targets(const Board& board,
                                                            PieceType    piece_type) noexcept {
    const Color    opponent      = ~board.side_to_move();
    const Square   opponent_king = board.king_sq(opponent);
    const Bitboard occupancy     = board.occupancy();

    switch (piece_type) {
    case PAWN:   return attacks::pawn_attacks(opponent_king, opponent);
    case KNIGHT: return attacks::piece_moves<KNIGHT>(opponent_king);
    case BISHOP: return attacks::piece_moves<BISHOP>(opponent_king, occupancy);
    case ROOK:   return attacks::piece_moves<ROOK>(opponent_king, occupancy);
    case QUEEN:  return attacks::piece_moves<QUEEN>(opponent_king, occupancy);
    default:     return 0;
    }
}

} // namespace

// Tactical cache maintenance

void Board::refresh_tactical_cache() noexcept {
    auto&          state     = ply_state();
    const Color    side      = side_to_move();
    const Color    opponent  = ~side;
    const Bitboard occupancy = this->occupancy();

    state.checkers        = attacks_to(king_sq(side), opponent, occupancy);
    state.blockers[WHITE] = attacks::slider_blockers(
        king_sq(WHITE), pieces<BISHOP, QUEEN>(BLACK), pieces<ROOK, QUEEN>(BLACK), occupancy);
    state.blockers[BLACK] = attacks::slider_blockers(
        king_sq(BLACK), pieces<BISHOP, QUEEN>(WHITE), pieces<ROOK, QUEEN>(WHITE), occupancy);
}

void Board::refresh_legal_enpassant_target() noexcept {
    auto&        state           = ply_state();
    const Square target          = state.enpassant_target;
    state.legal_enpassant_target = INVALID;

    if (target == INVALID || piece_on(target) != NO_PIECE)
        return;

    const Color  side            = side_to_move();
    const Square captured_square = move_geometry::enpassant_captured_square(target, side);
    if (piece_on(captured_square) != make_piece(~side, PAWN))
        return;

    Bitboard candidate_capturers = pieces<PAWN>(side) & attacks::pawn_attacks(target, ~side);
    while (candidate_capturers) {
        if (is_king_safe_after_enpassant(*this, bb::lsb_pop(candidate_capturers), target)) {
            state.legal_enpassant_target = target;
            return;
        }
    }
}

// Move validation and check detection

bool Board::is_pseudo_legal(Move move) const noexcept {
    if (move.is_null())
        return false;

    const Square from = move.from();
    const Square to   = move.to();
    if (from == to)
        return false;

    const Piece source_piece = piece_on(from);
    if (source_piece == NO_PIECE || color_of(source_piece) != turn)
        return false;

    const Piece destination_piece = piece_on(to);
    if (destination_piece != NO_PIECE && color_of(destination_piece) == turn)
        return false;

    const PieceType piece_type = type_of(source_piece);
    const Bitboard  occupancy  = this->occupancy();

    switch (move.type()) {
    case BASIC_MOVE: {
        if (piece_type == PAWN) {
            const int push_delta = move_geometry::pawn_push(turn);
            const int move_delta = int(to) - int(from);

            if (square::relative_rank(to, turn) == RANK8)
                return false;

            if (move_delta == push_delta)
                return destination_piece == NO_PIECE;

            if (move_delta == 2 * push_delta) {
                const Square mid = from + push_delta;
                return square::relative_rank(from, turn) == RANK2 && destination_piece == NO_PIECE
                    && piece_on(mid) == NO_PIECE;
            }

            return bb::contains(attacks::pawn_attacks(from, turn), to)
                && destination_piece != NO_PIECE;
        }

        if (piece_type == KING)
            return bb::contains(attacks::piece_moves<KING>(from), to);

        return bb::contains(attacks::piece_moves(from, piece_type, occupancy), to);
    }

    case MOVE_PROM: {
        if (piece_type != PAWN || !valid_promotion_piece(move.prom_piece()))
            return false;
        if (square::relative_rank(from, turn) != RANK7 || square::relative_rank(to, turn) != RANK8)
            return false;

        const int push_delta = move_geometry::pawn_push(turn);
        if (int(to) - int(from) == push_delta)
            return destination_piece == NO_PIECE;

        return bb::contains(attacks::pawn_attacks(from, turn), to) && destination_piece != NO_PIECE;
    }

    case MOVE_EP: {
        if (piece_type != PAWN || to != enpassant_target() || destination_piece != NO_PIECE)
            return false;
        if (!bb::contains(attacks::pawn_attacks(from, turn), to))
            return false;

        const Square captured_square = move_geometry::enpassant_captured_square(to, turn);
        return piece_on(captured_square) == make_piece(~turn, PAWN);
    }

    case MOVE_CASTLE: {
        if (piece_type != KING)
            return false;

        const CastleSide side     = move_geometry::castle_side(from, to);
        const auto&      castling = move_geometry::castling(side, turn);
        if (from != castling.king_from || to != castling.king_to)
            return false;

        if (!has_castling_right(side, turn))
            return false;

        if (piece_on(castling.rook_from) != make_piece(turn, ROOK))
            return false;
        if (occupancy & castling.empty_path)
            return false;

        return !any_attacked(castling.king_path, ~turn);
    }
    }

    return false;
}

bool Board::is_legal_move(Move move) const noexcept {
    return is_pseudo_legal(move) && is_legal_pseudo_move(move);
}

// Full validation is needed only while in check, or for king, en-passant, and pinned-piece moves.
bool Board::is_legal_generated_move(Move move) const noexcept {
    assert(is_pseudo_legal(move));

    const Square from = move.from();

    if (checkers() || from == king_sq(turn) || move.type() == MOVE_EP
        || bb::contains(blockers(turn), from))
        return is_legal_pseudo_move(move);

    return true;
}

bool Board::is_legal_pseudo_move(Move move) const noexcept {
    assert(is_pseudo_legal(move));

    const Square from = move.from();
    const Square to   = move.to();
    const Square king = king_sq(turn);

    // Castling king safety was fully validated as pseudo-legal.
    if (from == king) {
        if (move.type() == MOVE_CASTLE)
            return true;

        Bitboard occupancy = this->occupancy();
        bb::move(occupancy, from, to);
        return !attacks_to(to, ~turn, occupancy);
    }

    if (move.type() == MOVE_EP)
        return is_king_safe_after_enpassant(*this, from, to);

    const Bitboard checkers_bb = checkers();
    if (checkers_bb) {
        if (bb::is_many(checkers_bb))
            return false;

        // A non-king move must capture or interpose against the sole checker.
        const Square checker = bb::lsb(checkers_bb);
        if (to != checker && !bb::contains(square::between(king, checker), to))
            return false;
    }

    // A pinned piece may move only along the king ray.
    return !bb::contains(blockers(turn), from) || bb::contains(square::collinear(from, to), king);
}

bool Board::gives_check(Move move) const noexcept {
    const Square from          = move.from();
    const Square to            = move.to();
    const Color  opponent      = ~turn;
    const Square opponent_king = king_sq(opponent);

    // Handle direct and discovered checks.
    const PieceType piece_type = type_of(piece_on(from));
    if (bb::contains(direct_check_targets(*this, piece_type), to))
        return true;
    if (bb::contains(blockers(opponent), from)
        && !bb::contains(square::collinear(from, to), opponent_king))
        return true;

    switch (move.type()) {
    case MOVE_PROM: {
        Bitboard occupancy = this->occupancy();
        bb::remove(occupancy, from);
        return bb::contains(attacks::piece_moves(to, move.prom_piece(), occupancy), opponent_king);
    }

    case MOVE_EP: {
        // En passant can uncover a slider by removing a pawn off the destination.
        const Square captured_square = move_geometry::enpassant_captured_square(to, turn);
        Bitboard     occupancy       = this->occupancy();
        bb::move(occupancy, from, to);
        bb::remove(occupancy, captured_square);
        return (pieces<BISHOP, QUEEN>(turn)
                & attacks::piece_moves<BISHOP>(opponent_king, occupancy))
            || (pieces<ROOK, QUEEN>(turn) & attacks::piece_moves<ROOK>(opponent_king, occupancy));
    }

    case MOVE_CASTLE: {
        // Castling can give check through the relocated rook.
        const auto& castling  = move_geometry::castling(move_geometry::castle_side(from, to), turn);
        Bitboard    occupancy = this->occupancy();
        bb::move(occupancy, from, to);
        bb::move(occupancy, castling.rook_from, castling.rook_to);
        return bb::contains(attacks::piece_moves<ROOK>(castling.rook_to, occupancy), opponent_king);
    }

    case BASIC_MOVE: return false;
    default:         return false;
    }
}

// Draw detection

bool Board::is_draw(int ply_from_search_root) const noexcept {
    assert(ply_from_search_root >= 0);

    if (halfmove_clock() >= fifty_move_rule_halfmoves)
        return true;

    const PositionKey current_key   = key();
    const std::size_t current_index = ply_states.size() - 1;
    const std::size_t search_ply    = static_cast<std::size_t>(ply_from_search_root);
    const std::size_t reversible_history_plies =
        std::min<std::size_t>(halfmove_clock(), current_index);
    int prior_occurrences = 0;

    for (std::size_t plies_back = 2; plies_back <= reversible_history_plies; plies_back += 2) {
        const std::size_t index = current_index - plies_back;

        if (ply_states[index].zkey != current_key)
            continue;

        // One prior occurrence strictly after the search root is a cycle draw.
        // At or before the root, two are required for threefold repetition.
        if (search_ply > 0 && plies_back < search_ply) {
            return true;
        }
        if (++prior_occurrences == 2)
            return true;
    }

    return false;
}
