#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "board/castling.hpp"
#include "board/ply_state.hpp"
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

    PlyState*                active_ply_state = nullptr;
    std::vector<PositionKey> key_history;

    PlyState&       active_state() noexcept { return *active_ply_state; }
    const PlyState& active_state() const noexcept { return *active_ply_state; }

    void reset() noexcept;
    void bind_ply_state(PlyState& state_slot) noexcept { active_ply_state = &state_slot; }

    void disable_castle(Color c) noexcept;
    void disable_castle(Color c, Square sq) noexcept;

    template <bool>
    void add_piece(Square, Color, PieceType) noexcept;
    template <bool>
    void remove_piece(Square, Color, PieceType) noexcept;
    template <bool>
    void move_piece(Square, Square, Color, PieceType) noexcept;

    void update_check_data() noexcept;

public:
    explicit Board(PlyState& root_state, const std::string& fen = startfen);
    Board()                        = delete;
    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

    void load_fen(const std::string&);
    // Binds independent caller-owned storage and copies source as a search root.
    // Precondition: source is distinct and root_state is destination-owned storage.
    void copy_root_from(const Board& source, PlyState& root_state);

    // accessors

    [[nodiscard]] Bitboard pieces() const noexcept;
    [[nodiscard]] Bitboard pieces(Color c) const noexcept;
    template <PieceType... Ps>
    [[nodiscard]] Bitboard pieces() const noexcept;
    template <PieceType... Ps>
    [[nodiscard]] Bitboard pieces(Color c) const noexcept;

    [[nodiscard]] Bitboard     occupancy() const noexcept { return pieces(); }
    [[nodiscard]] std::uint8_t count(Color c, PieceType p) const noexcept {
        return piece_counts[c][p];
    }
    [[nodiscard]] Piece piece_on(Square sq) const noexcept { return squares[sq]; }
    [[nodiscard]] Piece piece_on(File f, Rank r) const noexcept {
        return squares[square::make(f, r)];
    };
    [[nodiscard]] PieceType piecetype_on(Square sq) const noexcept { return type_of(squares[sq]); }
    [[nodiscard]] Square    king_sq(Color c) const noexcept { return king_square[c]; }
    [[nodiscard]] Color     side_to_move() const noexcept { return turn; }
    [[nodiscard]] TaperedScore material_score() const noexcept { return material; }
    [[nodiscard]] TaperedScore psq_bonus_score() const noexcept { return psq_bonus; }
    [[nodiscard]] int          fullmove() const noexcept { return (fullmove_clk / 2) + 1; }

    [[nodiscard]] CastleRights castle_rights() const noexcept { return ply_state().castle; }
    [[nodiscard]] Bitboard     checkers() const noexcept { return ply_state().tactical.checkers; }
    [[nodiscard]] Bitboard     blockers(Color c) const noexcept {
        return ply_state().tactical.blockers[c];
    }
    [[nodiscard]] Bitboard pinners(Color c) const noexcept {
        return ply_state().tactical.pinners[c];
    }
    [[nodiscard]] Square       enpassant_sq() const noexcept { return ply_state().enpassant; }
    [[nodiscard]] std::uint8_t halfmove() const noexcept { return ply_state().halfmove_clk; }
    [[nodiscard]] Square       legal_enpassant_sq() const noexcept;

    [[nodiscard]] PlyState&       ply_state() noexcept { return *active_ply_state; }
    [[nodiscard]] const PlyState& ply_state() const noexcept { return *active_ply_state; }

    // castling

    [[nodiscard]] bool can_castle(Color c) const noexcept;
    [[nodiscard]] bool can_castle_kingside(Color c) const noexcept;
    [[nodiscard]] bool can_castle_queenside(Color c) const noexcept;

    // attack bitboards

    [[nodiscard]] Bitboard attacks_to(Square sq, Color c, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard attacks_to(Square sq, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard attacks_to(Square sq) const noexcept {
        return attacks_to(sq, occupancy());
    }
    [[nodiscard]] Bitboard attacks_to(Square sq, Color c) const noexcept {
        return attacks_to(sq, c, occupancy());
    }
    [[nodiscard]] bool attacks_to(Bitboard bitboard, Color c) const noexcept;

    // move properties

    [[nodiscard]] PieceType captured_piece_type(Move move) const noexcept;

    [[nodiscard]] bool is_capture(Move move) const noexcept;
    // Full shape validation for an arbitrary move; does not test pins or self-check.
    [[nodiscard]] bool is_pseudo_legal(Move move) const noexcept;
    // Precondition: move is pseudo-legal. Filters pins and self-check.
    [[nodiscard]] bool is_legal_pseudo_move(Move move) const noexcept;
    // Precondition: move came from local pseudo-legal move generation.
    [[nodiscard]] bool is_legal_generated_move(Move move) const noexcept;
    // Full legality validation for an arbitrary, untrusted move.
    [[nodiscard]] bool      is_legal_move(Move move) const noexcept;
    [[nodiscard]] bool      is_checking_move(Move move) const noexcept;
    [[nodiscard]] bool      see_at_least(Move move, EvalValue threshold) const noexcept;
    [[nodiscard]] EvalValue see(Move move) const noexcept;

    // make moves

    void make(Move, PlyState& next_state);
    void unmake(PlyState& prior_state);
    void make_null(PlyState& next_state);
    void unmake_null(PlyState& prior_state);

    // zobrist keys

    [[nodiscard]] PositionKey key() const noexcept { return ply_state().zkey; }
    [[nodiscard]] PositionKey calculate_key() const noexcept;

    // checks and draws

    [[nodiscard]] bool is_check() const noexcept { return checkers(); }
    [[nodiscard]] bool is_double_check() const noexcept { return bb::is_many(checkers()); };
    [[nodiscard]] bool is_draw(int search_ply = 0) const noexcept;

    // string conversions

    [[nodiscard]] std::string to_fen() const;

    // eval helpers

    [[nodiscard]] EvalValue non_pawn_material(Color c) const noexcept;

    static constexpr char startfen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
};

inline Board::Board(PlyState& root_state, const std::string& fen) {
    key_history.reserve(engine::max_search_ply + 1);
    bind_ply_state(root_state);
    load_fen(fen);
}

inline Bitboard Board::pieces() const noexcept {
    return pieces(WHITE) | pieces(BLACK);
}

inline Bitboard Board::pieces(Color c) const noexcept {
    return piece_bb[c][all_pieces_slot];
}

template <PieceType... Ps>
inline Bitboard Board::pieces() const noexcept {
    static_assert((is_piece_type(Ps) && ...));
    return ((piece_bb[WHITE][Ps] | piece_bb[BLACK][Ps]) | ...);
}

template <PieceType... Ps>
inline Bitboard Board::pieces(Color c) const noexcept {
    static_assert((is_piece_type(Ps) && ...));
    return (piece_bb[c][Ps] | ...);
};

inline PieceType Board::captured_piece_type(Move move) const noexcept {
    return move.type() == MOVE_EP ? PAWN : piecetype_on(move.to());
}

inline bool Board::is_capture(Move move) const noexcept {
    return captured_piece_type(move) != NO_PIECETYPE;
}

inline bool Board::can_castle(Color c) const noexcept {
    return castle_rights() & (c ? W_CASTLE : B_CASTLE);
};

inline bool Board::can_castle_kingside(Color c) const noexcept {
    return castle_rights() & (c ? W_KINGSIDE : B_KINGSIDE);
};

inline bool Board::can_castle_queenside(Color c) const noexcept {
    return castle_rights() & (c ? W_QUEENSIDE : B_QUEENSIDE);
};

inline void Board::disable_castle(Color c) noexcept {
    auto& state = this->active_state();

    if (can_castle_kingside(c))
        state.zkey ^= zob::castle[CASTLE_KINGSIDE][c];
    if (can_castle_queenside(c))
        state.zkey ^= zob::castle[CASTLE_QUEENSIDE][c];

    state.castle &= (c == WHITE ? B_CASTLE : W_CASTLE);
}

inline void Board::disable_castle(Color c, Square sq) noexcept {
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
inline Bitboard Board::attacks_to(Square sq, Color c, Bitboard occupied) const noexcept {
    return (pieces<PAWN>(c) & attacks::pawn_attacks(sq, ~c)) |
           (pieces<KNIGHT>(c) & attacks::piece_moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>(c) & attacks::piece_moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>(c) & attacks::piece_moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>(c) & attacks::piece_moves<ROOK>(sq, occupied));
}

// Returns a bitboard of pieces of any color which attack a square
inline Bitboard Board::attacks_to(Square sq, Bitboard occupied) const noexcept {
    return (pieces<PAWN>(WHITE) & attacks::pawn_attacks<BLACK>(sq)) |
           (pieces<PAWN>(BLACK) & attacks::pawn_attacks<WHITE>(sq)) |
           (pieces<KNIGHT>() & attacks::piece_moves<KNIGHT>(sq, occupied)) |
           (pieces<KING>() & attacks::piece_moves<KING>(sq, occupied)) |
           (pieces<BISHOP, QUEEN>() & attacks::piece_moves<BISHOP>(sq, occupied)) |
           (pieces<ROOK, QUEEN>() & attacks::piece_moves<ROOK>(sq, occupied));
}

// Determine if any bitboard is attacked by color c
inline bool Board::attacks_to(Bitboard bitboard, Color c) const noexcept {
    Bitboard occ = occupancy();
    while (bitboard) {
        Square sq = bb::lsb_pop(bitboard);
        if (attacks_to(sq, c, occ))
            return true;
    }
    return false;
}

template <bool apply_hash>
inline void Board::add_piece(Square sq, Color c, PieceType pt) noexcept {
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
inline void Board::remove_piece(Square sq, Color c, PieceType pt) noexcept {
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
inline void Board::move_piece(Square from, Square to, Color c, PieceType pt) noexcept {
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

inline EvalValue Board::non_pawn_material(Color c) const noexcept {
    return ((count(c, KNIGHT) * piece_value::knight_mg) +
            (count(c, BISHOP) * piece_value::bishop_mg) + (count(c, ROOK) * piece_value::rook_mg) +
            (count(c, QUEEN) * piece_value::queen_mg));
}
