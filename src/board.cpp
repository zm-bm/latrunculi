#include "board.hpp"

#include "eval.hpp"
#include "movegen.hpp"
#include "position_state.hpp"
#include "score.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {
bool valid_promotion_piece(PieceType piece) {
    return piece >= KNIGHT && piece <= QUEEN;
}

constexpr int direct_check_idx(PieceType piece) {
    return int(piece) * int(piece != KING);
}

template <bool Root>
uint64_t perft_impl(Board&              board,
                    int                 depth,
                    std::ostream&       oss,
                    PositionStateStack& states,
                    int                 ply,
                    PositionState&      current_state) {
    if (depth == 0)
        return 1;

    uint64_t nodes    = 0;
    auto     movelist = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move, states.child(ply));
        uint64_t count =
            perft_impl<false>(board, depth - 1, oss, states, ply + 1, states.child(ply));
        nodes += count;
        board.unmake(current_state);

        if constexpr (Root)
            oss << move.str() << ": " << count << '\n';
    }

    if constexpr (Root)
        oss << "NODES: " << nodes << std::endl;

    return nodes;
}
} // namespace

bool Board::is_pseudo_legal(Move mv) const {
    if (mv.is_null())
        return false;

    const Square from = mv.from();
    const Square to   = mv.to();
    if (from == to)
        return false;

    const Piece piece = piece_on(from);
    if (piece == NO_PIECE || color_of(piece) != turn)
        return false;

    const Piece target = piece_on(to);
    if (target != NO_PIECE && color_of(target) == turn)
        return false;

    const PieceType piecetype = type_of(piece);
    const uint64_t  to_bb     = bb::set(to);
    const uint64_t  occupied  = occupancy();

    switch (mv.type()) {
    case BASIC_MOVE: {
        if (piecetype == PAWN) {
            const int push_delta = (turn == WHITE) ? NORTH : SOUTH;
            const int move_delta = int(to) - int(from);

            if (relative_rank(to, turn) == RANK8)
                return false;

            if (move_delta == push_delta)
                return target == NO_PIECE;

            if (move_delta == 2 * push_delta) {
                const Square mid = from + push_delta;
                return relative_rank(from, turn) == RANK2 && target == NO_PIECE &&
                       piece_on(mid) == NO_PIECE;
            }

            return (bb::pawn_attacks(bb::set(from), turn) & to_bb) && target != NO_PIECE &&
                   color_of(target) == ~turn;
        }

        if (piecetype == KING)
            return bb::moves<KING>(from) & to_bb;

        return piecetype != NO_PIECETYPE && piecetype != ALL_PIECES && piecetype != KING &&
               (bb::moves(from, piecetype, occupied) & to_bb);
    }

    case MOVE_PROM: {
        if (piecetype != PAWN || !valid_promotion_piece(mv.prom_piece()))
            return false;
        if (relative_rank(from, turn) != RANK7 || relative_rank(to, turn) != RANK8)
            return false;

        const int push_delta = (turn == WHITE) ? NORTH : SOUTH;
        if (int(to) - int(from) == push_delta)
            return target == NO_PIECE;

        return (bb::pawn_attacks(bb::set(from), turn) & to_bb) && target != NO_PIECE &&
               color_of(target) == ~turn;
    }

    case MOVE_EP: {
        if (piecetype != PAWN || to != enpassant_sq() || target != NO_PIECE)
            return false;
        if (!(bb::pawn_attacks(bb::set(from), turn) & to_bb))
            return false;

        const Square captured = to + (turn == WHITE ? SOUTH : NORTH);
        return piece_on(captured) == make_piece(~turn, PAWN);
    }

    case MOVE_CASTLE: {
        if (piecetype != KING)
            return false;

        const Square expected_from = (turn == WHITE) ? E1 : E8;
        if (from != expected_from)
            return false;

        const CastleSide side = (to > from) ? CASTLE_KINGSIDE : CASTLE_QUEENSIDE;
        const Square     expected_to =
            (side == CASTLE_KINGSIDE) ? ((turn == WHITE) ? G1 : G8) : ((turn == WHITE) ? C1 : C8);
        if (to != expected_to)
            return false;

        if (side == CASTLE_KINGSIDE) {
            if (!can_castle_kingside(turn))
                return false;
        } else if (!can_castle_queenside(turn)) {
            return false;
        }

        if (piece_on(castle::rook_from[side][turn]) != make_piece(turn, ROOK))
            return false;
        if (occupied & castle::path[side][turn])
            return false;

        return !attacks_to(castle::kingpath[side][turn], ~turn);
    }
    }

    return false;
}

bool Board::is_legal_move(Move mv) const {
    return is_pseudo_legal(mv) && is_legal_pseudo_move(mv);
}

// Requires a generated pseudo-legal move. Only strict cases can expose or leave king in check.
bool Board::is_legal_generated_move(Move mv) const {
    const Square from = mv.from();

    if (checkers() || from == king_sq(turn) || mv.type() == MOVE_EP ||
        (blockers(turn) & bb::set(from)))
        return is_legal_pseudo_move(mv);

    return true;
}

// Requires a pseudo-legal move. Filters moves that expose or leave the king in check.
bool Board::is_legal_pseudo_move(Move mv) const {
    Square from = mv.from();
    Square to   = mv.to();
    Square king = king_sq(turn);

    // king move: check if king is attacked(castle legality handled in movegen)
    if (from == king) {
        if (mv.type() != MOVE_CASTLE)
            return !attacks_to(to, ~turn, occupancy() ^ bb::set(from, to));
        return true;
    }

    // enpassant: check if captured pawn is blocking check
    else if (mv.type() == MOVE_EP) {
        Square   pawn = to + (turn == WHITE ? SOUTH : NORTH);
        uint64_t occ  = (occupancy() ^ bb::set(from, pawn)) | bb::set(to);
        return !(pieces<BISHOP, QUEEN>(~turn) & bb::moves<BISHOP>(king, occ)) &&
               !(pieces<ROOK, QUEEN>(~turn) & bb::moves<ROOK>(king, occ));
    }

    // non-king moves must resolve any current check
    uint64_t checkers_bb = checkers();
    if (checkers_bb) {
        if (bb::is_many(checkers_bb))
            return false;

        Square checker = bb::lsb(checkers_bb);
        if (to != checker && !(bb::between(king, checker) & bb::set(to)))
            return false;
    }

    // check if moved piece is pinned or moving in-line with check
    uint64_t pins = blockers(turn);
    return (!pins || !(pins & bb::set(from)) || (bb::collinear(from, to) & bb::set(king)));
}

// Determine if a move gives check for the current board
bool Board::is_checking_move(Move mv) const {
    Square      from     = mv.from();
    Square      to       = mv.to();
    Color       opp      = ~turn;
    Square      opp_king = king_sq(opp);
    const auto& state    = position_state();

    // check if piece directly attacks the king or was a blocker
    const PieceType piecetype = type_of(piece_on(from));
    if (state.checks[direct_check_idx(piecetype)] & bb::set(to))
        return true;
    if ((state.blockers[opp] & bb::set(from)) && !(bb::collinear(from, to) & bb::set(opp_king)))
        return true;

    switch (mv.type()) {
    case MOVE_PROM: {
        // check if a promotion attacks the enemy king
        uint64_t occupied = occupancy() ^ bb::set(from);
        return bb::moves(to, mv.prom_piece(), occupied) & bb::set(opp_king);
    }

    case MOVE_EP: {
        // check if captured pawn was blocking enemy king from attack
        Square   pawn     = to + (turn == WHITE ? SOUTH : NORTH);
        uint64_t occupied = (occupancy() ^ bb::set(from, pawn)) | bb::set(to);
        return ((pieces<BISHOP, QUEEN>(turn) & bb::moves<BISHOP>(opp_king, occupied)) ||
                (pieces<ROOK, QUEEN>(turn) & bb::moves<ROOK>(opp_king, occupied)));
    }

    case MOVE_CASTLE: {
        // check if rook attacks enemy king
        Square   rook_from = castle::rook_from[to < from][turn];
        Square   rook_to   = castle::rook_to[to < from][turn];
        uint64_t occupied  = occupancy() ^ bb::set(from, rook_from, to, rook_to);
        return bb::moves<ROOK>(rook_to, occupied) & bb::set(opp_king);
    }

    case BASIC_MOVE: return false;
    default:         return false;
    }
}

// static exchange eval: likely eval change after a series of exchanges
int Board::seeMove(Move move) const {
    const Square from = move.from();
    const Square to   = move.to();

    Color     side           = side_to_move();
    PieceType piece          = piecetype_on(from);
    PieceType captured_piece = captured_piece_type(move);
    uint64_t  occupied       = occupancy();
    uint64_t  from_bb        = bb::set(from);

    if (move.type() == MOVE_EP) {
        const Square captured = to + (side == WHITE ? SOUTH : NORTH);
        occupied              = (occupied ^ bb::set(captured)) | bb::set(to);
    }

    uint64_t attackers = attacks_to(to, occupied);

    // swap-list of best case material gain for each depth
    int gain[32] = {};
    gain[0]      = eval::piece(captured_piece).mg;

    int depth = 0;
    do {
        // next depth + side
        depth++;
        side = ~side;

        // calculate gain, prune if negative
        gain[depth] = eval::piece(piece).mg - gain[depth - 1];
        if (std::max(-gain[depth - 1], gain[depth]) < 0)
            break;

        // update bitboards
        occupied  ^= from_bb;
        attackers  = attacks_to(to, occupied) & occupied;

        // find next least valuable attacker
        from_bb = 0;
        for (PieceType p : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            uint64_t opp_pieces = attackers & piece_bb[side][p];
            if (opp_pieces) {
                piece   = p;
                from_bb = opp_pieces & -opp_pieces; // get least significant bit
                break;
            }
        }
    } while (from_bb);

    // negamax the swap-list to get final static exchange eval
    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}

void Board::make(Move move, PositionState& next_state) {
    assert(&next_state != active_position_state);
    position_key_history.push(key());

    const Square from           = move.from();
    const Square to             = move.to();
    const Square enpassant      = legal_enpassant_sq();
    const auto   piecetype      = piecetype_on(from);
    const auto   movetype       = move.type();
    const Color  opp            = ~turn;
    Square       capt_sq        = to;
    PieceType    capt_piecetype = captured_piece_type(move);

    if (movetype == MOVE_EP) {
        capt_sq          = to + (turn == WHITE ? SOUTH : NORTH);
        squares[capt_sq] = NO_PIECE;
    }

    // create new state and update counters
    next_state            = PositionState(active_state(), move);
    active_position_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();
    ++fullmove_clk;

    // handle piece capture
    state.captured = capt_piecetype;
    if (capt_piecetype != NO_PIECETYPE) {
        state.halfmove_clk = 0;
        remove_piece<true>(capt_sq, opp, capt_piecetype);
        if (can_castle(opp) && capt_piecetype == ROOK)
            disable_castle(opp, capt_sq);
    }

    if (enpassant != INVALID)
        state.zkey ^= zob::hash_ep(enpassant);

    // move the piece / castle
    move_piece<true>(from, to, turn, piecetype);
    if (movetype == MOVE_CASTLE) {
        auto   castle_side = CastleSide(to < from);
        Square rook_from   = castle::rook_from[castle_side][turn];
        Square rook_to     = castle::rook_to[castle_side][turn];
        move_piece<true>(rook_from, rook_to, turn, ROOK);
    }

    switch (piecetype) {
    case PAWN:
        // set enpassant square,
        state.halfmove_clk = 0;
        if (std::abs(to - from) == PAWN_PUSH2) {
            state.enpassant = to + (turn == WHITE ? SOUTH : NORTH);
        } else if (movetype == MOVE_PROM) {
            remove_piece<true>(to, turn, PAWN);
            add_piece<true>(to, turn, move.prom_piece());
        }
        break;

    case KING:
        king_square[turn] = to;
        if (can_castle(turn))
            disable_castle(turn);
        break;

    case ROOK:
        if (can_castle(turn))
            disable_castle(turn, from);
        break;

    default: break;
    }

    turn        = opp;
    state.zkey ^= zob::turn;

    if (legal_enpassant_sq() != INVALID)
        state.zkey ^= zob::hash_ep(state.enpassant);

    update_check_data();
}

void Board::unmake(PositionState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_position_state);

    const Color  opp      = turn;
    const Move   move     = active_state().previous_move;
    const Square from     = move.from();
    const Square to       = move.to();
    const auto   movetype = move.type();

    auto piecetype      = piecetype_on(to);
    auto capt_piecetype = active_state().captured;

    // decrement counters and pop state
    position_key_history.pop(prior_state.zkey);
    active_position_state = &prior_state;
    --game_ply;
    fullmove_clk--;
    turn = ~turn;

    // replace promoted piece with pawn
    if (movetype == MOVE_PROM) {
        remove_piece<false>(to, turn, piecetype);
        add_piece<false>(to, turn, PAWN);
        piecetype = PAWN;
    }

    // move the piece back, restore captured piece
    move_piece<false>(to, from, turn, piecetype);
    if (movetype == MOVE_CASTLE) {
        auto   castle_side = CastleSide(to < from);
        Square rook_from   = castle::rook_from[castle_side][turn];
        Square rook_to     = castle::rook_to[castle_side][turn];
        move_piece<false>(rook_to, rook_from, turn, ROOK);
    } else if (capt_piecetype != NO_PIECETYPE) {
        Square capt_sq = (movetype != MOVE_EP) ? to : to + (turn == WHITE ? SOUTH : NORTH);
        add_piece<false>(capt_sq, opp, capt_piecetype);
    }

    if (piecetype == KING)
        king_square[turn] = from;
}

void Board::make_null(PositionState& next_state) {
    assert(&next_state != active_position_state);
    position_key_history.push(key());

    const Square hashed_enpassant = legal_enpassant_sq();

    next_state            = PositionState(active_state(), Move());
    active_position_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();

    turn        = ~turn;
    state.zkey ^= zob::turn;
    if (hashed_enpassant != INVALID)
        state.zkey ^= zob::hash_ep(hashed_enpassant);

    update_check_data();
}

void Board::unmake_null(PositionState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_position_state);

    position_key_history.pop(prior_state.zkey);
    active_position_state = &prior_state;
    --game_ply;
    turn = ~turn;
}

// stalemate from no legal moves
bool Board::is_stalemate() const {
    auto movelist = movegen::generate_pseudo_legal(*this);
    for (auto& move : movelist) {
        if (is_legal_generated_move(move))
            return false;
    }
    return true;
}

// draws from 50-move rule + 3-fold repetition
bool Board::is_draw(int search_ply) const {
    if (halfmove() >= 100)
        return true;

    const int rewind      = std::min<int>(halfmove(), position_key_history.count());
    int       repetitions = 0;

    for (int distance = 2; distance <= rewind; distance += 2) {
        const int index = position_key_history.count() - distance;
        if (index < 0)
            break;

        if (position_key_history[index] != key())
            continue;

        if (distance < search_ply)
            return true;
        if (++repetitions == 2)
            return true;
    }

    return false;
}

// convert a move to standard algebraic notation
std::string Board::toSAN(Move move) const {
    std::string result    = "";
    Square      from      = move.from();
    Square      to        = move.to();
    PieceType   piecetype = piecetype_on(from);

    if (move.type() == MOVE_CASTLE) {
        if (move.from() < move.to())
            return "O-O";
        else
            return "O-O-O";
    }

    if (piecetype != PAWN)
        result += std::toupper(to_char(piecetype));

    // handle move disambiguation
    auto movelist = movegen::generate_pseudo_legal(*this);
    for (auto& m : movelist) {
        bool ambiguous =
            (piecetype_on(m.from()) == piecetype) && (m.to() == to) && (m.from() != from);
        if (ambiguous && is_legal_generated_move(m)) {
            if (file_of(m.from()) != file_of(from))
                result += to_char(file_of(from));
            if (rank_of(m.from()) != rank_of(from))
                result += to_char(rank_of(from));
            break;
        }
    }

    if (is_capture(move)) {
        if (piecetype == PAWN)
            result += to_char(file_of(from));
        result += 'x';
    }

    result += to_string(to);
    if (move.type() == MOVE_PROM)
        result += '=' + to_char(move.prom_piece());
    if (is_checking_move(move))
        result += '+';

    return result;
}

// perform a perft search to find # of legal moves at a given depth
template <bool Root>
uint64_t Board::perft(int depth, std::ostream& oss) {
    if (depth < 0 || depth > MAX_SEARCH_PLY)
        throw std::invalid_argument("perft depth out of range");

    PositionStateStack states;
    return perft_impl<Root>(*this, depth, oss, states, 0, active_state());
}

template uint64_t Board::perft<true>(int, std::ostream&);
template uint64_t Board::perft<false>(int, std::ostream&);
