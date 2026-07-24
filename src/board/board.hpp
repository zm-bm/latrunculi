#pragma once

#include <cassert>
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
 * Mutable chess position with caller-owned per-ply state.
 *
 * Board owns the durable position representation and history. It borrows every
 * PlyState supplied at construction or transition; that storage must remain
 * alive at a stable address while it can be active or restored by unmake.
 * Preserve prior states unchanged until restoring them in LIFO order.
 */
class Board {
public:
    // Lifecycle

    static constexpr char start_fen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    explicit Board(PlyState& root_state, std::string_view fen = start_fen);
    Board()                        = delete;
    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

    // Copies source as a new root and binds this Board to root_state.
    // Preconditions: source is distinct and root_state is independent
    // destination storage.
    void copy_root_from(const Board& source, PlyState& root_state);

    // Representation

    [[nodiscard]] Bitboard pieces() const noexcept;
    [[nodiscard]] Bitboard pieces(Color color) const noexcept;
    template <PieceType... Ps>
    [[nodiscard]] Bitboard pieces() const noexcept;
    template <PieceType... Ps>
    [[nodiscard]] Bitboard pieces(Color color) const noexcept;

    [[nodiscard]] Bitboard     occupancy() const noexcept { return pieces(); }
    [[nodiscard]] std::uint8_t count(Color color, PieceType piece_type) const noexcept {
        return piece_counts[color][piece_type];
    }
    [[nodiscard]] Piece piece_on(Square square) const noexcept { return squares[square]; }
    [[nodiscard]] Piece piece_on(File file, Rank rank) const noexcept {
        return squares[square::make(file, rank)];
    }
    [[nodiscard]] PieceType piece_type_on(Square square) const noexcept {
        return type_of(squares[square]);
    }
    [[nodiscard]] Square king_sq(Color color) const noexcept { return king_square[color]; }
    [[nodiscard]] Color  side_to_move() const noexcept { return turn; }

    // Reversible state

    [[nodiscard]] int          fullmove_number() const noexcept { return (absolute_ply / 2) + 1; }
    [[nodiscard]] std::uint8_t halfmove_clock() const noexcept {
        return ply_state().halfmove_clock;
    }
    [[nodiscard]] CastlingRights castling_rights() const noexcept {
        return ply_state().castling_rights;
    }
    [[nodiscard]] Square enpassant_target() const noexcept { return ply_state().enpassant_target; }
    [[nodiscard]] Square legal_enpassant_target() const noexcept {
        return ply_state().legal_enpassant_target;
    }
    [[nodiscard]] PositionKey     key() const noexcept { return ply_state().zkey; }
    [[nodiscard]] PlyState&       ply_state() noexcept { return *active_ply_state; }
    [[nodiscard]] const PlyState& ply_state() const noexcept { return *active_ply_state; }

    // Attacks and castling

    [[nodiscard]] Bitboard checkers() const noexcept { return ply_state().checkers; }
    [[nodiscard]] Bitboard blockers(Color color) const noexcept {
        return ply_state().blockers[color];
    }
    [[nodiscard]] bool is_check() const noexcept { return checkers(); }
    [[nodiscard]] bool is_double_check() const noexcept { return bb::is_many(checkers()); }

    [[nodiscard]] Bitboard
    attacks_to(Square target, Color attacker, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard attacks_to(Square target, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard attacks_to(Square target, Color attacker) const noexcept {
        return attacks_to(target, attacker, occupancy());
    }
    [[nodiscard]] bool any_attacked(Bitboard targets, Color attacker) const noexcept;
    [[nodiscard]] bool has_castling_rights(Color color) const noexcept;
    [[nodiscard]] bool has_castling_right(CastleSide side, Color color) const noexcept;

    // Move validation

    [[nodiscard]] PieceType captured_piece_type(Move move) const noexcept;
    [[nodiscard]] bool      is_capture(Move move) const noexcept;
    // Full shape validation for an arbitrary move; does not test pins or self-check.
    [[nodiscard]] bool is_pseudo_legal(Move move) const noexcept;
    // Precondition: move is pseudo-legal. Filters pins and self-check.
    [[nodiscard]] bool is_legal_pseudo_move(Move move) const noexcept;
    // Precondition: move came from local pseudo-legal move generation.
    [[nodiscard]] bool is_legal_generated_move(Move move) const noexcept;
    // Full legality validation for an arbitrary, untrusted move.
    [[nodiscard]] bool is_legal_move(Move move) const noexcept;
    // Precondition: move is pseudo-legal in the current position.
    [[nodiscard]] bool gives_check(Move move) const noexcept;

    // Transitions

    // Preconditions: move is legal and non-null; next_state is distinct
    // writable storage.
    void make(Move move, PlyState& next_state);
    // Precondition: prior_state is the exact unchanged LIFO predecessor.
    void unmake(PlyState& prior_state);
    // Precondition: next_state is distinct writable storage.
    void make_null(PlyState& next_state);
    // Precondition: prior_state is the exact unchanged LIFO predecessor.
    void unmake_null(PlyState& prior_state);

    // Rules and evaluation

    // Checks fifty-move and repetition draws. Pass the current nonnegative search
    // ply, or zero outside search.
    [[nodiscard]] bool is_draw(int ply_from_search_root = 0) const noexcept;
    // Precondition: move is a pseudo-legal capture or promotion.
    [[nodiscard]] EvalValue    see(Move move) const noexcept;
    [[nodiscard]] TaperedScore material_score() const noexcept { return material; }
    [[nodiscard]] TaperedScore psq_bonus_score() const noexcept { return psq_bonus; }
    [[nodiscard]] EvalValue    non_pawn_material(Color color) const noexcept;

    // Diagnostics

    [[nodiscard]] std::string to_fen() const;
    // Rebuilds the current position key from representation and reversible state.
    [[nodiscard]] PositionKey recompute_key() const noexcept;

private:
    PlyState&       active_state() noexcept { return *active_ply_state; }
    const PlyState& active_state() const noexcept { return *active_ply_state; }
    void bind_ply_state(PlyState& state_slot) noexcept { active_ply_state = &state_slot; }

    void reset() noexcept;
    void load_fen(std::string_view fen);

    template <bool apply_hash>
    void add_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void remove_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void move_piece(Square from, Square to, Color color, PieceType piece_type) noexcept;

    inline void clear_castling_rights(Color color) noexcept;
    inline void clear_rook_castling_right(Color color, Square rook_square) noexcept;

    void               refresh_tactical_cache() noexcept;
    void               refresh_legal_enpassant_target() noexcept;
    [[nodiscard]] bool enpassant_preserves_king_safety(Square from, Square target) const noexcept;

    Bitboard     piece_bb[N_COLORS][N_PIECETYPES]     = {0};
    std::uint8_t piece_counts[N_COLORS][N_PIECETYPES] = {0};
    Piece        squares[N_SQUARES]                   = {NO_PIECE};
    Square       king_square[N_COLORS]                = {E1, E8};
    Color        turn                                 = WHITE;

    // Game ply represented by the FEN move number and side to move.
    int absolute_ply = 0;
    // Traversal ply relative to the current root position.
    int ply_from_root = 0;

    TaperedScore material  = {0, 0};
    TaperedScore psq_bonus = {0, 0};

    PlyState*                active_ply_state = nullptr;
    std::vector<PositionKey> key_history;
};

inline Bitboard Board::pieces() const noexcept {
    return pieces(WHITE) | pieces(BLACK);
}

inline Bitboard Board::pieces(Color color) const noexcept {
    return piece_bb[color][all_pieces_slot];
}

template <PieceType... Ps>
inline Bitboard Board::pieces() const noexcept {
    static_assert((is_piece_type(Ps) && ...));
    return ((piece_bb[WHITE][Ps] | piece_bb[BLACK][Ps]) | ...);
}

template <PieceType... Ps>
inline Bitboard Board::pieces(Color color) const noexcept {
    static_assert((is_piece_type(Ps) && ...));
    return (piece_bb[color][Ps] | ...);
}

inline PieceType Board::captured_piece_type(Move move) const noexcept {
    return move.type() == MOVE_EP ? PAWN : piece_type_on(move.to());
}

inline bool Board::is_capture(Move move) const noexcept {
    return captured_piece_type(move) != NO_PIECETYPE;
}

inline bool Board::has_castling_rights(Color color) const noexcept {
    return castling_rights() & (color ? W_CASTLE : B_CASTLE);
}

inline bool Board::has_castling_right(CastleSide side, Color color) const noexcept {
    if (side == CASTLE_KINGSIDE)
        return castling_rights() & (color ? W_KINGSIDE : B_KINGSIDE);
    return castling_rights() & (color ? W_QUEENSIDE : B_QUEENSIDE);
}

// Returns pieces of attacker color that attack target.
inline Bitboard
Board::attacks_to(Square target, Color attacker, Bitboard occupancy) const noexcept {
    return (pieces<PAWN>(attacker) & attacks::pawn_attacks(target, ~attacker)) |
           (pieces<KNIGHT>(attacker) & attacks::piece_moves<KNIGHT>(target, occupancy)) |
           (pieces<KING>(attacker) & attacks::piece_moves<KING>(target, occupancy)) |
           (pieces<BISHOP, QUEEN>(attacker) & attacks::piece_moves<BISHOP>(target, occupancy)) |
           (pieces<ROOK, QUEEN>(attacker) & attacks::piece_moves<ROOK>(target, occupancy));
}

// Returns pieces of either color that attack target.
inline Bitboard Board::attacks_to(Square target, Bitboard occupancy) const noexcept {
    return (pieces<PAWN>(WHITE) & attacks::pawn_attacks<BLACK>(target)) |
           (pieces<PAWN>(BLACK) & attacks::pawn_attacks<WHITE>(target)) |
           (pieces<KNIGHT>() & attacks::piece_moves<KNIGHT>(target, occupancy)) |
           (pieces<KING>() & attacks::piece_moves<KING>(target, occupancy)) |
           (pieces<BISHOP, QUEEN>() & attacks::piece_moves<BISHOP>(target, occupancy)) |
           (pieces<ROOK, QUEEN>() & attacks::piece_moves<ROOK>(target, occupancy));
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

template <bool apply_hash>
inline void Board::add_piece(Square square, Color color, PieceType piece_type) noexcept {
    assert(squares[square] == NO_PIECE);

    piece_counts[color][piece_type]++;
    bb::add(piece_bb[color][piece_type], square);
    bb::add(piece_bb[color][all_pieces_slot], square);
    squares[square]  = make_piece(color, piece_type);
    material        += eval::piece(piece_type, color);
    psq_bonus       += eval::piece_sq(piece_type, color, square);
    if constexpr (apply_hash)
        active_state().zkey ^= zob::hash_piece(color, piece_type, square);
}

template <bool apply_hash>
inline void Board::remove_piece(Square square, Color color, PieceType piece_type) noexcept {
    assert(squares[square] == make_piece(color, piece_type));

    piece_counts[color][piece_type]--;
    bb::remove(piece_bb[color][piece_type], square);
    bb::remove(piece_bb[color][all_pieces_slot], square);
    squares[square]  = NO_PIECE;
    material        -= eval::piece(piece_type, color);
    psq_bonus       -= eval::piece_sq(piece_type, color, square);
    if constexpr (apply_hash)
        active_state().zkey ^= zob::hash_piece(color, piece_type, square);
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
        active_state().zkey ^=
            zob::hash_piece(color, piece_type, from) ^ zob::hash_piece(color, piece_type, to);
}

inline EvalValue Board::non_pawn_material(Color color) const noexcept {
    return ((count(color, KNIGHT) * piece_value::knight_mg) +
            (count(color, BISHOP) * piece_value::bishop_mg) +
            (count(color, ROOK) * piece_value::rook_mg) +
            (count(color, QUEEN) * piece_value::queen_mg));
}
