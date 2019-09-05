#include <iostream>
#include "tt.hpp"
#include "move.hpp"

namespace TT {

    void Table::init(U32 nbytes)
    {
        // Clear existing table
        delete [] _table;

        // Determine size of transposition table
        _size = nbytes / sizeof(Entry) / 2;

        try
        {
            _table = new Entry [_size * 2];
        }
        catch(std::bad_alloc& e)
        {
            std::cerr << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    void Table::save(U64 zkey, U8 depth, int score, NodeType flags, Move best)
    {
        int alwaysReplaceIx = zkey % _size;
        int depthPreferredIx = alwaysReplaceIx + 1;

        Entry* entry = &_table[depthPreferredIx];
        if (depth >= entry->depth)
        {
            // Replace entry if depth is higher than previous entry
            *entry = Entry(zkey, score, depth, flags, best);
        }
        else
        {
            // Otherwise, always replace
            _table[alwaysReplaceIx] = Entry(zkey, score, depth, flags, best);
        }
    }

    Entry * Table::probe(U64 zkey) const
    {
        int alwaysReplaceIx = zkey % _size;
        int depthPreferredIx = alwaysReplaceIx + 1;

        // Check the always replace entry first
        Entry* entry = &_table[alwaysReplaceIx];

        if ((entry->zkey == zkey)
            && (entry->flag != TT_NONE))
        {
            // TODO: Check legality of move
            return entry;
        }

        // Check the depth preferred
        entry = &_table[depthPreferredIx];
        if ((entry->zkey == zkey)
            && (entry->flag != TT_NONE))
        {
            // TODO: Check legality of move
            return entry;
        }

        return nullptr;
    }

    std::ostream& operator<<(std::ostream& os, const Entry& e)
    {
        os << "Key\t" << e.zkey << std::endl;
        os << "Score\t" << e.score << std::endl;
        os << "Depth\t" << (int)e.depth << std::endl;
        os << "Flag\t" << e.flag << std::endl;
        os << "Best\t" << e.best << std::endl;
        return os;
    }

    Table table = Table();

}
    