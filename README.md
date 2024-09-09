# Latrunculi

UCI chess engine, written in Modern C++ tested with gtest

* Move Generation
   * Bitboard board representation
   * Board state vectors for making/unmaking moves
   * Correctness tested with gtest/perft

* Search
   * Principal variation search
   * Best collected from refutation table
   * Transposition table (hash table)
   * Pruning (Null move pruning, late move reduction)
   * Move ordering (Hash/killer moves, history heuristic, mvv-lva)

* Evaluation
   * Tapered material + piece sq values
   * Passed, doubled, tripled, isolated pawns
   * Open files, undeveloped/outpost minor pieces
   * Bishop pair, connected rooks, etc
   * Piece tropism/mobility, king safety
  
