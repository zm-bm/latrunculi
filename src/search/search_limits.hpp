#pragma once

#include <algorithm>
#include <optional>

#include "core/constants.hpp"
#include "core/types.hpp"

struct SearchLimits {
    static constexpr int max_depth = engine::max_search_depth;

    int depth = max_depth;

    std::optional<Milliseconds> movetime;
    std::optional<NodeCount>    nodes;
    std::optional<Milliseconds> wtime;
    std::optional<Milliseconds> btime;
    std::optional<Milliseconds> winc;
    std::optional<Milliseconds> binc;
    std::optional<int>          movestogo;

    SearchLimits() = default;

    void set_depth(int d) { depth = std::clamp(d, 1, max_depth); }
    void set_movetime(Milliseconds::rep mt) {
        movetime = Milliseconds{std::max(mt, Milliseconds::rep{1})};
    }
    void set_nodes(NodeCount n) { nodes = n; }
    void set_wtime(Milliseconds::rep wt) {
        wtime = Milliseconds{std::max(wt, Milliseconds::rep{0})};
    }
    void set_btime(Milliseconds::rep bt) {
        btime = Milliseconds{std::max(bt, Milliseconds::rep{0})};
    }
    void set_winc(Milliseconds::rep wi) { winc = Milliseconds{std::max(wi, Milliseconds::rep{0})}; }
    void set_binc(Milliseconds::rep bi) { binc = Milliseconds{std::max(bi, Milliseconds::rep{0})}; }
    void set_movestogo(int mtg) { movestogo = std::max(mtg, 1); }

    std::optional<Milliseconds> allocated_time(Color c) const;
};
