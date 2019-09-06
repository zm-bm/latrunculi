# Antonius

[Antonius](img/antonius.jpg)

UCI chess engine, written in C++

* Board representation
    * Piece-centric, 14 total bitboards
        * 1 for all pieces of a color
        * 6 for each piece type of a color
    * Square-centric, 8x8 mailbox
        * Redundant, but allows faster move generation
    * Vector of past/current board states
        * Move
        * Castling rights
        * En passant square
        * Captured piece
        * Half move clock
    * Side to move
    * Full move clock
    * Additional eval info, incrementally updated

* Move Generation
    * Uses Pradu Kannan's fancy magic bitboard implementation
    * Pseudo legal
    * Staged by captures, quiet moves, and evasions
        * Allows for quiescense search
    * Correctness tested via perft

* Search
    * Principal variation search
    * Quiescense search
    * PV collection via refutation table
    * Transposition table
    * Reductions
        * Null move pruning
        * Late move reduction
    * Move ordering
        * Hash move
        * Killer moves
        * History heuristic
        * MVV-LVA

* Evaluation
    * Material
    * Piece value squares
    * Tapered between opening and endgame
    * Passed, doubled, tripled, isolated pawns
    * Open and half open files
    * Undeveloped minor pieces
    * Minor piece outposts
    * Bishop pair
    * Connected rooks, rooks on 7th rank
    * Piece tropism (proximity to own/enemy king)
    * King safety
    * Piece mobility
    * Tempo

# Remaining

* 3-fold repetition
* Insufficient material
* UCI Controller
    * Time controls
    * Best move
* TT table
    * aging
* Search
    * EPD tactical tests
    * Static exchange evaluation
    * Aspiration windows
* Support other compilers/architectures
