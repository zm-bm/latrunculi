#ifndef LATRUNCULI_SEARCH_H
#define LATRUNCULI_SEARCH_H

#include <cstring>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <chrono>
#include "globals.hpp"
#include "types.hpp"
#include "eval.hpp"
#include "move.hpp"

class Board;

struct Killer {
    Move move1;
    Move move2;
};

class Search
{
    public:

        Search() = default;
        Search(Board * board)
        : bestMove(Move()), _board(board), searchPly(0), nSearched(0)
        { }

        /*
         *  search.cpp
         */
        
        void think(int);
        
        template<bool>
        int negamax(int, int, int, bool=true, bool=true);
        int quiesce(int, int);

        template<bool, bool = true>
        U64 perft(int);

        void reset();
        void sortMoves(std::vector<Move>&, Move = Move());

        Move bestMove;
        int bestScore = 0;
        const static int MAX_DEPTH = 64;

    private:
        // Board to search
        Board * _board;

        // Main search variables
        std::vector<Move> pv[MAX_DEPTH];
        I32 searchPly;
        U32 history[2][6][64];
        Killer killers[MAX_DEPTH];

        // Search statistics variables
        U32 nSearched;
        std::chrono::high_resolution_clock::time_point start, stop;

        // Helper methods
        void addToHistory(Move move, int ply);
        void savePV(Move move);
        void printPV(int, int, unsigned int);
};



#endif