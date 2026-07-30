#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "board/castling_rights.hpp"
#include "board/ply_state.hpp"
#include "board/zobrist.hpp"
#include "core/attacks.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/move_geometry.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/eval.hpp"
#include "eval/tapered_score.hpp"

/**
 * Mutable chess position supporting reversible move traversal.
 *
 * Board owns the reversible state history since its loaded position. Moves and
 * null moves append state and are unmade in LIFO order.
 */
class Board {
public:
    static constexpr char start_fen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // Representation lifecycle and key diagnostics (board_representation.cpp)

    explicit Board(std::string_view fen = start_fen);
    Board(const Board& source);
    Board& operator=(const Board& source);
    Board(Board&&)            = delete;
    Board& operator=(Board&&) = delete;

    void        clear_position() noexcept;
    PositionKey recompute_key() const noexcept;

    // Position and state queries

    Bitboard pieces(Color color) const noexcept { return piece_bb[color][all_pieces_slot]; }

    template <PieceType... P> requires((is_piece_type(P) && ...))
    Bitboard pieces(Color c) const noexcept {
        return (piece_bb[c][P] | ...);
    }

    template <PieceType... P> requires((is_piece_type(P) && ...))
    Bitboard pieces() const noexcept {
        return ((piece_bb[WHITE][P] | piece_bb[BLACK][P]) | ...);
    }

    Bitboard     occupancy() const noexcept { return pieces(WHITE) | pieces(BLACK); }
    std::uint8_t count(Color c, PieceType pt) const noexcept { return piece_counts[c][pt]; }
    Piece        piece_on(Square square) const noexcept { return squares[square]; }
    Piece        piece_on(File f, Rank r) const noexcept { return piece_on(square::make(f, r)); }
    PieceType    piece_type_on(Square square) const noexcept { return type_of(squares[square]); }
    Square       king_sq(Color color) const noexcept { return king_square[color]; }

    Color          side_to_move() const noexcept { return turn; }
    PositionKey    key() const noexcept { return ply_state().zkey; }
    CastlingRights castling_rights() const noexcept { return ply_state().castling_rights; }

    Square enpassant_target() const noexcept { return ply_state().enpassant_target; }
    Square legal_enpassant_target() const noexcept { return ply_state().legal_enpassant_target; }

    std::uint8_t halfmove_clock() const noexcept { return ply_state().halfmove_clock; }
    int          fullmove_number() const noexcept { return (absolute_ply / 2) + 1; }

    Move previous_move() const noexcept { return ply_state().previous_move; }
    bool can_unmake() const noexcept { return ply_states.size() > 1; }

    Bitboard checkers() const noexcept { return ply_state().checkers; }
    Bitboard blockers(Color king_color) const noexcept { return ply_state().blockers[king_color]; }
    bool     is_check() const noexcept { return checkers(); }
    bool     is_double_check() const noexcept { return bb::is_many(checkers()); }

    TaperedScore material_score() const noexcept { return material; }
    TaperedScore psq_bonus_score() const noexcept { return psq_bonus; }
    EvalValue    non_pawn_material(Color color) const noexcept;

    // Attacks, castling, and move classification

    Bitboard attacks_to(Square target, Color attacker) const noexcept;
    Bitboard attacks_to(Square target, Color attacker, Bitboard occupancy) const noexcept;
    Bitboard all_attackers_to(Square target, Bitboard occupancy) const noexcept;

    bool any_attacked(Bitboard targets, Color attacker) const noexcept;
    bool has_castling_rights(Color color) const noexcept;
    bool has_castling_right(CastleSide side, Color color) const noexcept;

    PieceType captured_piece_type(Move move) const noexcept;
    bool is_capture(Move move) const noexcept { return captured_piece_type(move) != NO_PIECETYPE; }

    // FEN loading and serialization (board_fen.cpp)

    void        load_fen(std::string_view fen);
    std::string to_fen() const;

    // Move rules and draw detection (board_rules.cpp)

    void refresh_tactical_cache() noexcept;
    void refresh_legal_enpassant_target() noexcept;

    bool is_pseudo_legal(Move move) const noexcept;
    bool is_legal_pseudo_move(Move move) const noexcept;
    bool is_legal_move(Move move) const noexcept;
    bool gives_check(Move move) const noexcept;
    bool is_draw(int ply_from_search_root = 0) const noexcept;

    // Move application and reversal (board_move.cpp)

    void make(Move move);
    void unmake();
    void make_null();
    void unmake_null();

    // Static exchange evaluation (board_see.cpp)

    EvalValue see(Move move) const noexcept;

private:
    // Ply-state storage

    PlyState&       ply_state() noexcept;
    const PlyState& ply_state() const noexcept;
    PlyState&       push_ply_state(Move move);
    void            pop_ply_state() noexcept;

    // Incremental representation mutation

    template <bool apply_hash>
    void add_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void remove_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void move_piece(Square from, Square to, Color color, PieceType piece_type) noexcept;

    // Position storage

    // Redundant piece placement for fast queries; piece_bb[color][all_pieces_slot]
    // stores aggregate occupancy.
    Bitboard     piece_bb[N_COLORS][N_PIECETYPES]     = {};
    std::uint8_t piece_counts[N_COLORS][N_PIECETYPES] = {};
    Piece        squares[N_SQUARES]                   = {NO_PIECE};
    Square       king_square[N_COLORS]                = {E1, E8};
    Color        turn                                 = WHITE;

    // Game ply used for FEN fullmove numbering; null moves do not change it.
    int absolute_ply = 0;

    // Incremental tapered scores, positive for White and negative for Black.
    TaperedScore material  = {0, 0};
    TaperedScore psq_bonus = {0, 0};

    // Reversible history from the loaded root; active_state caches ply_states.back().
    std::vector<PlyState> ply_states;
    PlyState*             active_state = nullptr;
};

// Inline ply-state storage definitions

inline PlyState& Board::ply_state() noexcept {
    assert(!ply_states.empty() && active_state == &ply_states.back());
    return *active_state;
}

inline const PlyState& Board::ply_state() const noexcept {
    assert(!ply_states.empty() && active_state == &ply_states.back());
    return *active_state;
}

inline PlyState& Board::push_ply_state(Move move) {
    assert(!ply_states.empty());

    const std::size_t previous_index = ply_states.size() - 1;
    PlyState&         state          = ply_states.emplace_back();
    active_state                     = &state;

    const PlyState& previous = ply_states[previous_index];
    state.zkey               = previous.zkey;
    state.castling_rights    = previous.castling_rights;
    state.halfmove_clock     = previous.halfmove_clock + 1;
    state.previous_move      = move;
    return state;
}

inline void Board::pop_ply_state() noexcept {
    assert(ply_states.size() > 1);
    ply_states.pop_back();
    active_state = &ply_states.back();
}

// Inline query definitions

inline EvalValue Board::non_pawn_material(Color color) const noexcept {
    return ((count(color, KNIGHT) * piece_value::knight_mg)
            + (count(color, BISHOP) * piece_value::bishop_mg)
            + (count(color, ROOK) * piece_value::rook_mg)
            + (count(color, QUEEN) * piece_value::queen_mg));
}

// Returns geometric attackers of target, including pinned pieces.
inline Bitboard Board::attacks_to(Square target, Color attacker) const noexcept {
    return attacks_to(target, attacker, occupancy());
}

// Supplied occupancy changes slider rays, not membership in the current position.
inline Bitboard
Board::attacks_to(Square target, Color attacker, Bitboard occupancy) const noexcept {
    return (pieces<PAWN>(attacker) & attacks::pawn_attacks(target, ~attacker))
         | (pieces<KNIGHT>(attacker) & attacks::piece_moves<KNIGHT>(target, occupancy))
         | (pieces<KING>(attacker) & attacks::piece_moves<KING>(target, occupancy))
         | (pieces<BISHOP, QUEEN>(attacker) & attacks::piece_moves<BISHOP>(target, occupancy))
         | (pieces<ROOK, QUEEN>(attacker) & attacks::piece_moves<ROOK>(target, occupancy));
}

inline Bitboard Board::all_attackers_to(Square target, Bitboard occupancy) const noexcept {
    return (pieces<PAWN>(WHITE) & attacks::pawn_attacks<BLACK>(target))
         | (pieces<PAWN>(BLACK) & attacks::pawn_attacks<WHITE>(target))
         | (pieces<KNIGHT>() & attacks::piece_moves<KNIGHT>(target, occupancy))
         | (pieces<KING>() & attacks::piece_moves<KING>(target, occupancy))
         | (pieces<BISHOP, QUEEN>() & attacks::piece_moves<BISHOP>(target, occupancy))
         | (pieces<ROOK, QUEEN>() & attacks::piece_moves<ROOK>(target, occupancy));
}

inline bool Board::any_attacked(Bitboard targets, Color attacker) const noexcept {
    const Bitboard occupancy = this->occupancy();
    while (targets) {
        const Square target = bb::lsb_pop(targets);
        if (attacks_to(target, attacker, occupancy))
            return true;
    }
    return false;
}

inline bool Board::has_castling_rights(Color color) const noexcept {
    return castling_rights() & (color == WHITE ? W_CASTLE : B_CASTLE);
}

inline bool Board::has_castling_right(CastleSide side, Color color) const noexcept {
    if (side == CASTLE_KINGSIDE)
        return castling_rights() & (color == WHITE ? W_KINGSIDE : B_KINGSIDE);
    return castling_rights() & (color == WHITE ? W_QUEENSIDE : B_QUEENSIDE);
}

// Classifies move in the current position without validating it.
inline PieceType Board::captured_piece_type(Move move) const noexcept {
    return move.type() == MOVE_EP ? PAWN : piece_type_on(move.to());
}

// Inline representation mutation

// Keep bitboards, counts, mailbox, material, and PSQT synchronized. Callers
// maintain king_square and, when apply_hash is false, the position key.
template <bool apply_hash>
inline void Board::add_piece(Square square, Color color, PieceType piece_type) noexcept {
    assert(squares[square] == NO_PIECE);

    piece_counts[color][piece_type]++;
    bb::add(piece_bb[color][piece_type], square);
    bb::add(piece_bb[color][all_pieces_slot], square);
    squares[square] = make_piece(color, piece_type);
    material += eval::piece(piece_type, color);
    psq_bonus += eval::piece_sq(piece_type, color, square);
    if constexpr (apply_hash)
        ply_state().zkey ^= zob::hash_piece(color, piece_type, square);
}

template <bool apply_hash>
inline void Board::remove_piece(Square square, Color color, PieceType piece_type) noexcept {
    assert(squares[square] == make_piece(color, piece_type));

    piece_counts[color][piece_type]--;
    bb::remove(piece_bb[color][piece_type], square);
    bb::remove(piece_bb[color][all_pieces_slot], square);
    squares[square] = NO_PIECE;
    material -= eval::piece(piece_type, color);
    psq_bonus -= eval::piece_sq(piece_type, color, square);
    if constexpr (apply_hash)
        ply_state().zkey ^= zob::hash_piece(color, piece_type, square);
}

template <bool apply_hash>
inline void Board::move_piece(Square from, Square to, Color color, PieceType piece_type) noexcept {
    assert(squares[from] == make_piece(color, piece_type));
    assert(squares[to] == NO_PIECE);

    bb::move(piece_bb[color][piece_type], from, to);
    bb::move(piece_bb[color][all_pieces_slot], from, to);
    squares[from] = NO_PIECE;
    squares[to]   = make_piece(color, piece_type);
    psq_bonus += eval::piece_sq(piece_type, color, to) - eval::piece_sq(piece_type, color, from);
    if constexpr (apply_hash)
        ply_state().zkey ^=
            zob::hash_piece(color, piece_type, from) ^ zob::hash_piece(color, piece_type, to);
}
