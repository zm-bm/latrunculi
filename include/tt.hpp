#ifndef LATRUNCULI_TT_H
#define LATRUNCULI_TT_H

#include <iostream>
#include "types.hpp"
#include "move.hpp"

namespace TT {

    struct Entry
    {

        U64         zkey;
        I32         score;
        Move        best;
        NodeType    flag;
        U8          depth;
        U8          age;

        Entry() = default;

        Entry(U64 _zkey, int _score, U8 _depth, NodeType _flag, Move _best)
        : zkey(_zkey), score(_score), best(_best), flag(_flag), depth(_depth), age(0)
        { }

        friend std::ostream& operator<<(std::ostream&, const Entry&);

        // void update(U64, int, U8, NodeType, Move);

    };

    class Table
    {
    private:

        Entry * _table;
        U32 _size = 0;

    public:

        void init(U32);
        void save(U64, U8, int, NodeType, Move);
        Entry * probe(U64) const;

        Table()
        {
            init(16777216);
        }

        ~Table()
        {
            delete [] _table;
        }

    };

    extern Table table;


    // inline void Entry::update(U64 _zkey, int _score, U8 _depth, NodeType _flag, Move _best)        
    // {
    //     zkey = _zkey;
    //     score = _score;
    //     depth = _depth;
    //     flag = _flag;
    //     best = _best;
    // }

}

#endif