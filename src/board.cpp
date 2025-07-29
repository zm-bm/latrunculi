#include "board.hpp"

#include "eval.hpp"
#include "fen.hpp"
#include "movegen.hpp"
#include "score.hpp"
#include "thread.hpp"

void Board::load_fen(const std::string& fen) {
    reset();
    FenParser parser(fen);

    for (const auto p : parser.pieces) {
        add_piece<true>(p.square, p.color, p.type);
        if (p.type == KING)
            king_square[p.color] = p.square;
    }

    turn                       = parser.turn;
    state.at(ply).castle       = parser.castle;
    state.at(ply).enpassant    = parser.enpassant;
    state.at(ply).halfmove_clk = parser.halfmove_clk;
    fullmove_clk               = parser.fullmove_clk;

    state.at(ply).zkey = calculate_key();
    update_check_data();
}

void Board::reset() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int p = 0; p < N_PIECES; ++p) {
            piece_bb[c][p]     = 0;
            piece_counts[c][p] = 0;
        }
    }
    for (int sq = 0; sq < N_SQUARES; ++sq)
        squares[sq] = NO_PIECE;

    material  = {0, 0};
    psq_bonus = {0, 0};
    state     = {State()};
    ply       = 0;
}

// Determine if a pseudo-legal move is legal
bool Board::is_legal_move(Move mv) const {
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

    // check if moved piece is pinned or moving in-line with check
    uint64_t pins = blockers(turn);
    return (!pins || !(pins & bb::set(from)) || bb::collinear(from, to) & bb::set(king));
}

// Determine if a move gives check for the current board
bool Board::is_checking_move(Move mv) const {
    Square from     = mv.from();
    Square to       = mv.to();
    Color  opp      = ~turn;
    Square opp_king = king_sq(opp);
    auto&  st       = state.at(ply);

    // check if piece directly attacks the king or was a blocker
    if (st.checks[type_of(piece_on(from))] & bb::set(to))
        return true;
    if ((st.blockers[opp] & bb::set(from)) && !(bb::collinear(from, to) & bb::set(opp_king)))
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

    Color     side      = side_to_move();
    PieceType piece     = piecetype_on(from);
    uint64_t  occupied  = occupancy();
    uint64_t  attackers = attacks_to(to, occupied);
    uint64_t  from_bb   = bb::set(from);

    // swap-list of best case material gain for each depth
    int gain[32] = {};
    gain[0]      = eval::piece(piecetype_on(to)).mg;

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
        for (PieceType p : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
            uint64_t opp_pieces = attackers & piece_bb[side][p];
            if (opp_pieces) {
                piece   = p;
                from_bb = opp_pieces & -opp_pieces; // get least significant bit
            }
        }
    } while (from_bb);

    // negamax the swap-list to get final static exchange eval
    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}

void Board::make(Move move) {
    const Square from           = move.from();
    const Square to             = move.to();
    const Square enpassant      = enpassant_sq();
    const auto   piecetype      = piecetype_on(from);
    const auto   movetype       = move.type();
    const Color  opp            = ~turn;
    Square       capt_sq        = to;
    PieceType    capt_piecetype = piecetype_on(to);

    if (movetype == MOVE_EP) {
        capt_piecetype   = PAWN;
        capt_sq          = to + (turn == WHITE ? SOUTH : NORTH);
        squares[capt_sq] = NO_PIECE;
    }

    // create new state and update counters
    state.push_back(State(state[ply++], move));
    auto& st = state.at(ply);
    ++fullmove_clk;
    if (thread)
        thread->ply++;

    // handle piece capture
    st.captured = capt_piecetype;
    if (capt_piecetype != NO_PIECETYPE) {
        st.halfmove_clk = 0;
        remove_piece<true>(capt_sq, opp, capt_piecetype);
        if (can_castle(opp) && capt_piecetype == ROOK)
            disable_castle(opp, capt_sq);
    }

    if (enpassant != INVALID)
        st.zkey ^= zob::hash_ep(enpassant);

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
        st.halfmove_clk = 0;
        if (std::abs(to - from) == PAWN_PUSH2) {
            st.enpassant  = to + (turn == WHITE ? SOUTH : NORTH);
            st.zkey      ^= zob::hash_ep(st.enpassant);
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

    turn     = opp;
    st.zkey ^= zob::turn;

    update_check_data();
}

void Board::unmake() {
    const Color  opp      = turn;
    const Move   move     = state[ply].move;
    const Square from     = move.from();
    const Square to       = move.to();
    const auto   movetype = move.type();

    auto piecetype      = piecetype_on(to);
    auto capt_piecetype = state[ply].captured;

    // decrement counters and pop state
    if (thread)
        thread->ply--;
    ply--;
    fullmove_clk--;
    turn = ~turn;
    state.pop_back();

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

void Board::make_null() {
    const Square enpassant = enpassant_sq();

    state.push_back(State(state[ply++], Move()));
    auto& st = state.at(ply);

    turn     = ~turn;
    st.zkey ^= zob::turn;
    if (enpassant != INVALID)
        st.zkey ^= zob::hash_ep(enpassant);

    update_check_data();
}

void Board::unmake_null() {
    --ply;
    turn = ~turn;
    state.pop_back();
}

// Calculate the Zobrist key for the current board state
uint64_t Board::calculate_key() const {
    uint64_t zkey = 0;

    for (auto sq = A1; sq != INVALID; ++sq) {
        auto piece = piece_on(sq);
        if (piece != NO_PIECE)
            zkey ^= zob::hash_piece(color_of(piece), type_of(piece), sq);
    }

    if (turn == BLACK)
        zkey ^= zob::turn;
    if (can_castle_kingside(WHITE))
        zkey ^= zob::castle[CASTLE_KINGSIDE][WHITE];
    if (can_castle_queenside(WHITE))
        zkey ^= zob::castle[CASTLE_QUEENSIDE][WHITE];
    if (can_castle_kingside(BLACK))
        zkey ^= zob::castle[CASTLE_KINGSIDE][BLACK];
    if (can_castle_queenside(BLACK))
        zkey ^= zob::castle[CASTLE_QUEENSIDE][BLACK];

    auto sq = enpassant_sq();
    if (sq != INVALID)
        zkey ^= zob::hash_ep(sq);

    return zkey;
}

// stalemate from no legal moves
bool Board::is_stalemate() const {
    auto movelist = generate<ALL_MOVES>(*this);
    for (auto& move : movelist) {
        if (is_legal_move(move))
            return false;
    }
    return true;
}

// draws from 50-move rule + 3-fold repetition
bool Board::is_draw() const {
    auto& st = state[ply];
    if (st.halfmove_clk >= 100)
        return true;

    int reps = 0;
    int stop = std::max(0, int(ply) - st.halfmove_clk);
    for (int i = std::max(0, int(ply) - 2); i >= stop; i -= 2) {
        if (state[i].zkey == st.zkey && ++reps == 2)
            return true;
    }

    return false;
}

// convert the board to a FEN string representation
std::string Board::toFEN() const {
    std::ostringstream oss;
    int                empty = 0;

    auto reset_empty = [&]() {
        if (empty > 0) {
            oss << empty;
            empty = 0;
        }
    };

    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        for (File file = FILE1; file <= FILE8; ++file) {
            Piece p = piece_on(file, rank);
            if (p != NO_PIECE) {
                reset_empty();
                oss << p;
            } else {
                ++empty;
            }
        }

        reset_empty();
        if (rank != RANK1)
            oss << '/';
    }

    oss << (turn == WHITE ? " w " : " b ");

    if (can_castle(WHITE) || can_castle(BLACK)) {
        oss << (can_castle_kingside(WHITE) ? "K" : "");
        oss << (can_castle_queenside(WHITE) ? "Q" : "");
        oss << (can_castle_kingside(BLACK) ? "k" : "");
        oss << (can_castle_queenside(BLACK) ? "q" : "");
    } else {
        oss << "-";
    }

    Square ep_sq = enpassant_sq();
    oss << " " << (ep_sq != INVALID ? to_string(ep_sq) : "-");
    oss << " " << +halfmove() << " " << +fullmove();

    return oss.str();
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
    auto movelist = generate<ALL_MOVES>(*this);
    for (auto& m : movelist) {
        bool ambiguous =
            (piecetype_on(m.from()) == piecetype) && (m.to() == to) && (m.from() != from);
        if (ambiguous && is_legal_move(m)) {
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
template <NodeType node>
uint64_t Board::perft(int depth, std::ostream& oss) {
    if (depth == 0)
        return 1;

    uint64_t nodes    = 0;
    auto     movelist = generate<ALL_MOVES>(*this);

    for (auto& move : movelist) {
        if (!is_legal_move(move))
            continue;

        make(move);
        uint64_t count  = perft<NON_PV>(depth - 1);
        nodes          += count;
        unmake();

        if constexpr (node == ROOT)
            oss << move.str() << ": " << count << '\n';
    }

    if constexpr (node == ROOT)
        oss << "NODES: " << nodes << std::endl;

    return nodes;
}

template uint64_t Board::perft<ROOT>(int, std::ostream& = std::cerr);
