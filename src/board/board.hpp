#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include "board/castling.hpp"
#include "board/ply_state.hpp"
#include "board/position_key_history.hpp"
#include "board/zobrist.hpp"
#include "core/attacks.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/eval.hpp"
#include "eval/tapered_score.hpp"

class Board {
private:
    Bitboard     piece_bb[N_COLORS][N_PIECETYPES]     = {0};
    std::uint8_t piece_counts[N_COLORS][N_PIECETYPES] = {0};
    Piece        squares[N_SQUARES]                   = {NO_PIECE};
    Square       king_square[N_COLORS]                = {E1, E8};
    Color        turn                                 = WHITE;

    int game_ply     = 0;
    int fullmove_clk = 0;

    TaperedScore material  = {0, 0};
    TaperedScore psq_bonus = {0, 0};

    PlyState*          active_ply_state = nullptr;
    PositionKeyHistory position_key_history;

    PlyState&       active_state() { return *active_ply_state; }
    const PlyState& active_state() const { return *active_ply_state; }

public:
    explicit Board(PlyState& root_state, const std::string& fen = startfen);
    Board()                        = delete;
    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

    void load_board(const Board*);
    void load_fen(const std::string&);
    void reset();
    void bind_ply_state(PlyState& state_slot) { active_ply_state = &state_slot; }

    // accessors

    Bitboard pieces() const;
    Bitboard pieces(Color c) const;
    template <PieceType... Ps>
    Bitboard pieces() const;
    template <PieceType... Ps>
    Bitboard pieces(Color c) const;

    Bitboard     occupancy() const { return pieces(); }
    std::uint8_t count(Color c, PieceType p) const { return piece_counts[c][p]; }
    Piece        piece_on(Square sq) const { return squares[sq]; }
    Piece        piece_on(File f, Rank r) const { return squares[square::make(f, r)]; };
    PieceType    piecetype_on(Square sq) const { return type_of(squares[sq]); }
    Square       king_sq(Color c) const { return king_square[c]; }
    Color        side_to_move() const { return turn; }
    TaperedScore material_score() const { return material; }
    TaperedScore psq_bonus_score() const { return psq_bonus; }
    int          fullmove() const { return (fullmove_clk / 2) + 1; }

    CastleRights castle_rights() const { return ply_state().castle; }
    Bitboard     checkers() const { return ply_state().tactical.checkers; }
    Bitboard     blockers(Color c) const { return ply_state().tactical.blockers[c]; }
    Bitboard     pinners(Color c) const { return ply_state().tactical.pinners[c]; }
    Square       enpassant_sq() const { return ply_state().enpassant; }
    std::uint8_t halfmove() const { return ply_state().halfmove_clk; }
    Square       legal_enpassant_sq() const;

    PlyState&       ply_state() { return *active_ply_state; }
    const PlyState& ply_state() const { return *active_ply_state; }

    // castling

    bool can_castle(Color c) const;
    bool can_castle_kingside(Color c) const;
    bool can_castle_queenside(Color c) const;
    void disable_castle(Color c);
    void disable_castle(Color c, Square sq);

    // attack bitboards

    Bitboard attacks_to(Square sq, Color c, Bitboard occupancy) const;
    Bitboard attacks_to(Square sq, Bitboard occupancy) const;
    Bitboard attacks_to(Square sq) const { return attacks_to(sq, occupancy()); }
    Bitboard attacks_to(Square sq, Color c) const { return attacks_to(sq, c, occupancy()); }
    bool     attacks_to(Bitboard bitboard, Color c) const;

    // piece modifiers

    template <bool>
    void add_piece(Square, Color, PieceType);
    template <bool>
    void remove_piece(Square, Color, PieceType);
    template <bool>
    void move_piece(Square, Square, Color, PieceType);

    // check data update methods

    void update_check_data();

    // move properties

    PieceType captured_piece_type(Move move) const;

    bool is_capture(Move move) const;
    // Fast shape check for arbitrary moves; does not test pins or self-check.
    bool is_pseudo_legal(Move move) const;
    // Requires a pseudo-legal/generated move; filters pins and self-check.
    bool is_legal_pseudo_move(Move move) const;
    // Requires a move from local movegen/picker output; fast-paths ordinary legal moves.
    bool is_legal_generated_move(Move move) const;
    // Full legality check for arbitrary/untrusted moves.
    bool      is_legal_move(Move move) const;
    bool      is_checking_move(Move move) const;
    bool      seeAtLeast(Move move, EvalValue threshold) const;
    EvalValue seeMove(Move move) const;

    // make moves

    void make(Move, PlyState& next_state);
    void unmake(PlyState& prior_state);
    void make_null(PlyState& next_state);
    void unmake_null(PlyState& prior_state);

    // zobrist keys

    PositionKey key() const { return ply_state().zkey; }
    PositionKey calculate_key() const;

    // checks and draws

    bool is_check() const { return checkers(); }
    bool is_double_check() const { return bb::is_many(checkers()); };
    bool is_draw(int search_ply = 0) const;

    // string conversions

    std::string toFEN() const;

    // eval helpers

    EvalValue nonPawnMaterial(Color c) const;

    static constexpr char startfen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
};

inline Board::Board(PlyState& root_state, const std::string& fen) {
    bind_ply_state(root_state);
    load_fen(fen);
}

inline Bitboard Board::pieces() const {
    return pieces(WHITE) | pieces(BLACK);
}

inline Bitboard Board::pieces(Color c) const {
    return piece_bb[c][all_pieces_slot];
}

template <PieceType... Ps>
inline Bitboard Board::pieces() const {
    static_assert((is_piece_type(Ps) && ...));
    return ((piece_bb[WHITE][Ps] | piece_bb[BLACK][Ps]) | ...);
}

template <PieceType... Ps>
inline Bitboard Board::pieces(Color c) const {
    static_assert((is_piece_type(Ps) && ...));
    return (piece_bb[c][Ps] | ...);
};

inline PieceType Board::captured_piece_type(Move move) const {
    return move.type() == MOVE_EP ? PAWN : piecetype_on(move.to());
}

inline bool Board::is_capture(Move move) const {
    return captured_piece_type(move) != NO_PIECETYPE;
}

inline bool Board::can_castle(Color c) const {
    return castle_rights() & (c ? W_CASTLE : B_CASTLE);
};

inline bool Board::can_castle_kingside(Color c) const {
    return castle_rights() & (c ? W_KINGSIDE : B_KINGSIDE);
};

inline bool Board::can_castle_queenside(Color c) const {
    return castle_rights() & (c ? W_QUEENSIDE : B_QUEENSIDE);
};

inline void Board::disable_castle(Color c) {
    auto& state = this->active_state();

    if (can_castle_kingside(c))
        state.zkey ^= zob::castle[CASTLE_KINGSIDE][c];
    if (can_castle_queenside(c))
        state.zkey ^= zob::castle[CASTLE_QUEENSIDE][c];

    state.castle &= (c == WHITE ? B_CASTLE : W_CASTLE);
}

inline void Board::disable_castle(Color c, Square sq) {
    auto& state = this->active_state();

    if (sq == castle::rook_from[CASTLE_KINGSIDE][c] && can_castle_kingside(c)) {
        state.zkey   ^= zob::castle[CASTLE_KINGSIDE][c];
        state.castle &= ~(c == WHITE ? W_KINGSIDE : B_KINGSIDE);
    }

    else if (sq == castle::rook_from[CASTLE_QUEENSIDE][c] && can_castle_queenside(c)) {
        state.zkey   ^= zob::castle[CASTLE_QUEENSIDE][c];
        state.castle &= ~(c == WHITE ? W_QUEENSIDE : B_QUEENSIDE);
    }
}

// Returns a bitboard of pieces of color c which attack a square
inline Bitboard Board::attacks_to(Square sq, Color c, Bitboard occupied) const {
    return (pieces<PAWN>(c) & attacks::pawn_attacks(sq, ~c)) |
           (pieces<KNIGHT>(c) & attacks::piece_moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>(c) & attacks::piece_moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>(c) & attacks::piece_moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>(c) & attacks::piece_moves<ROOK>(sq, occupied));
}

// Returns a bitboard of pieces of any color which attack a square
inline Bitboard Board::attacks_to(Square sq, Bitboard occupied) const {
    return (pieces<PAWN>(WHITE) & attacks::pawn_attacks<BLACK>(sq)) |
           (pieces<PAWN>(BLACK) & attacks::pawn_attacks<WHITE>(sq)) |
           (pieces<KNIGHT>() & attacks::piece_moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>() & attacks::piece_moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>() & attacks::piece_moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>() & attacks::piece_moves<ROOK>(sq, occupied));
}

// Determine if any bitboard is attacked by color c
inline bool Board::attacks_to(Bitboard bitboard, Color c) const {
    Bitboard occ = occupancy();
    while (bitboard) {
        Square sq = bb::lsb_pop(bitboard);
        if (attacks_to(sq, c, occ))
            return true;
    }
    return false;
}

template <bool apply_hash>
inline void Board::add_piece(Square sq, Color c, PieceType pt) {
    assert(squares[sq] == NO_PIECE);

    piece_counts[c][pt]++;
    bb::add(piece_bb[c][pt], sq);
    bb::add(piece_bb[c][all_pieces_slot], sq);
    squares[sq]  = make_piece(c, pt);
    material    += eval::piece(pt, c);
    psq_bonus   += eval::piece_sq(pt, c, sq);
    if constexpr (apply_hash)
        active_state().zkey ^= zob::hash_piece(c, pt, sq);
}

template <bool apply_hash>
inline void Board::remove_piece(Square sq, Color c, PieceType pt) {
    assert(squares[sq] == make_piece(c, pt));

    piece_counts[c][pt]--;
    bb::remove(piece_bb[c][pt], sq);
    bb::remove(piece_bb[c][all_pieces_slot], sq);
    squares[sq]  = NO_PIECE;
    material    -= eval::piece(pt, c);
    psq_bonus   -= eval::piece_sq(pt, c, sq);
    if constexpr (apply_hash)
        active_state().zkey ^= zob::hash_piece(c, pt, sq);
}

template <bool apply_hash>
inline void Board::move_piece(Square from, Square to, Color c, PieceType pt) {
    assert(squares[from] == make_piece(c, pt));
    assert(squares[to] == NO_PIECE);

    bb::move(piece_bb[c][pt], from, to);
    bb::move(piece_bb[c][all_pieces_slot], from, to);
    squares[from]  = NO_PIECE;
    squares[to]    = make_piece(c, pt);
    psq_bonus     += eval::piece_sq(pt, c, to) - eval::piece_sq(pt, c, from);
    if constexpr (apply_hash)
        active_state().zkey ^= zob::hash_piece(c, pt, from) ^ zob::hash_piece(c, pt, to);
}

inline EvalValue Board::nonPawnMaterial(Color c) const {
    return ((count(c, KNIGHT) * piece_value::knight_mg) +
            (count(c, BISHOP) * piece_value::bishop_mg) + (count(c, ROOK) * piece_value::rook_mg) +
            (count(c, QUEEN) * piece_value::queen_mg));
}
