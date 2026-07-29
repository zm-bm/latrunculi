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

    // Construction, copying, initialization, and key diagnostics (board_representation.cpp)

    explicit Board(std::string_view fen = start_fen);
    Board(const Board& source);
    Board& operator=(const Board& source);
    Board(Board&&)            = delete;
    Board& operator=(Board&&) = delete;

    // Clears all representation and history. The Board must be repopulated
    // before position queries or move operations are used.
    void clear_position() noexcept;

    // Rebuilds the current position key from representation and reversible state.
    [[nodiscard]] PositionKey recompute_key() const noexcept;

    // Position and state queries

    Bitboard pieces(Color color) const noexcept { return piece_bb[color][all_pieces_slot]; }
    template <PieceType... Types>
    Bitboard pieces(Color color) const noexcept;
    template <PieceType... Types>
    Bitboard pieces() const noexcept;

    Bitboard     occupancy() const noexcept;
    std::uint8_t count(Color color, PieceType piece_type) const noexcept;
    Piece        piece_on(Square square) const noexcept { return squares[square]; }
    Piece        piece_on(File file, Rank rank) const noexcept;
    PieceType    piece_type_on(Square square) const noexcept { return type_of(squares[square]); }
    Square       king_sq(Color color) const noexcept { return king_square[color]; }
    Color        side_to_move() const noexcept { return turn; }

    int            fullmove_number() const noexcept { return (absolute_ply / 2) + 1; }
    std::uint8_t   halfmove_clock() const noexcept { return ply_state().halfmove_clock; }
    CastlingRights castling_rights() const noexcept { return ply_state().castling_rights; }
    Square         enpassant_target() const noexcept { return ply_state().enpassant_target; }
    Square legal_enpassant_target() const noexcept { return ply_state().legal_enpassant_target; }
    PositionKey key() const noexcept { return ply_state().zkey; }
    Move        previous_move() const noexcept { return ply_state().previous_move; }
    bool        can_unmake() const noexcept { return ply_states.size() > 1; }

    Bitboard checkers() const noexcept { return ply_state().checkers; }
    Bitboard blockers(Color king_color) const noexcept { return ply_state().blockers[king_color]; }
    [[nodiscard]] bool is_check() const noexcept { return checkers(); }
    [[nodiscard]] bool is_double_check() const noexcept { return bb::is_many(checkers()); }

    TaperedScore material_score() const noexcept { return material; }
    TaperedScore psq_bonus_score() const noexcept { return psq_bonus; }
    EvalValue    non_pawn_material(Color color) const noexcept;

    // Attacks, castling, and move classification

    // Returns geometric attackers of target, including pinned pieces.
    Bitboard attacks_to(Square target, Color attacker) const noexcept;
    // Supplied occupancy changes slider rays, not membership in the current position.
    Bitboard attacks_to(Square target, Color attacker, Bitboard occupancy) const noexcept;
    Bitboard all_attackers_to(Square target, Bitboard occupancy) const noexcept;

    [[nodiscard]] bool any_attacked(Bitboard targets, Color attacker) const noexcept;
    [[nodiscard]] bool has_castling_rights(Color color) const noexcept;
    [[nodiscard]] bool has_castling_right(CastleSide side, Color color) const noexcept;

    // Classifies move in the current position without validating it.
    [[nodiscard]] PieceType captured_piece_type(Move move) const noexcept;
    [[nodiscard]] bool      is_capture(Move move) const noexcept;

    // FEN loading and serialization (board_fen.cpp)

    // Replaces the current position and clears move and repetition history.
    // If parsing fails, the board is unchanged.
    void                      load_fen(std::string_view fen);
    [[nodiscard]] std::string to_fen() const;

    // Move rules and draw detection (board_rules.cpp)

    // Refresh derived active-state caches after representation changes. These
    // functions do not update the position key.
    void refresh_tactical_cache() noexcept;
    void refresh_legal_enpassant_target() noexcept;

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
    // Checks fifty-move and repetition draws. Pass the current nonnegative search
    // ply, or zero outside search.
    [[nodiscard]] bool is_draw(int ply_from_search_root = 0) const noexcept;

    // Move application and reversal (board_move.cpp)

    // Preconditions: move is legal and non-null.
    void make(Move move);
    // Precondition: can_unmake().
    void unmake();
    void make_null();
    // Precondition: can_unmake().
    void unmake_null();

    // Static exchange evaluation (board_see.cpp)

    // Precondition: move is a pseudo-legal capture or promotion.
    [[nodiscard]] EvalValue see(Move move) const noexcept;

private:
    // Owned ply-state stack; active_state always points to the current entry.

    PlyState&       ply_state() noexcept;
    const PlyState& ply_state() const noexcept;
    PlyState&       push_ply_state(Move move);
    void            pop_ply_state() noexcept;

    // Incremental piece-representation updates

    // Keep bitboards, counts, mailbox, material, and PSQT synchronized. Callers
    // maintain king_square and, when apply_hash is false, the position key.
    template <bool apply_hash>
    void add_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void remove_piece(Square square, Color color, PieceType piece_type) noexcept;
    template <bool apply_hash>
    void move_piece(Square from, Square to, Color color, PieceType piece_type) noexcept;

    Bitboard     piece_bb[N_COLORS][N_PIECETYPES]     = {};
    std::uint8_t piece_counts[N_COLORS][N_PIECETYPES] = {};
    Piece        squares[N_SQUARES]                   = {NO_PIECE};
    Square       king_square[N_COLORS]                = {E1, E8};
    Color        turn                                 = WHITE;

    // Game ply represented by the FEN move number and side to move.
    int          absolute_ply = 0;
    TaperedScore material     = {0, 0};
    TaperedScore psq_bonus    = {0, 0};

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

template <PieceType... Types>
inline Bitboard Board::pieces(Color color) const noexcept {
    static_assert((is_piece_type(Types) && ...));
    return (piece_bb[color][Types] | ...);
}

template <PieceType... Types>
inline Bitboard Board::pieces() const noexcept {
    static_assert((is_piece_type(Types) && ...));
    return ((piece_bb[WHITE][Types] | piece_bb[BLACK][Types]) | ...);
}

inline Bitboard Board::occupancy() const noexcept {
    return piece_bb[WHITE][all_pieces_slot] | piece_bb[BLACK][all_pieces_slot];
}

inline std::uint8_t Board::count(Color color, PieceType piece_type) const noexcept {
    return piece_counts[color][piece_type];
}

inline Piece Board::piece_on(File file, Rank rank) const noexcept {
    return squares[square::make(file, rank)];
}

inline EvalValue Board::non_pawn_material(Color color) const noexcept {
    return ((count(color, KNIGHT) * piece_value::knight_mg)
            + (count(color, BISHOP) * piece_value::bishop_mg)
            + (count(color, ROOK) * piece_value::rook_mg)
            + (count(color, QUEEN) * piece_value::queen_mg));
}

inline Bitboard Board::attacks_to(Square target, Color attacker) const noexcept {
    return attacks_to(target, attacker, occupancy());
}

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

inline PieceType Board::captured_piece_type(Move move) const noexcept {
    return move.type() == MOVE_EP ? PAWN : piece_type_on(move.to());
}

inline bool Board::is_capture(Move move) const noexcept {
    return captured_piece_type(move) != NO_PIECETYPE;
}

// Inline representation mutation

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
