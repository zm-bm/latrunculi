#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

#include "core/constants.hpp"
#include "core/types.hpp"

struct SearchLimits {
    static constexpr int max_depth = engine::max_search_depth;

    int depth = max_depth;

    std::optional<Milliseconds> movetime;
    std::optional<uint64_t>     nodes;
    std::optional<Milliseconds> wtime;
    std::optional<Milliseconds> btime;
    std::optional<Milliseconds> winc;
    std::optional<Milliseconds> binc;
    std::optional<int>          movestogo;

    SearchLimits() = default;

    void set_depth(int d) { depth = std::clamp(d, 1, max_depth); }
    void set_movetime(int mt) { movetime = Milliseconds{std::max(mt, 1)}; }
    void set_nodes(int n) { nodes = static_cast<uint64_t>(std::max(n, 0)); }
    void set_wtime(int wt) { wtime = Milliseconds{std::max(wt, 0)}; }
    void set_btime(int bt) { btime = Milliseconds{std::max(bt, 0)}; }
    void set_winc(int wi) { winc = Milliseconds{std::max(wi, 0)}; }
    void set_binc(int bi) { binc = Milliseconds{std::max(bi, 0)}; }
    void set_movestogo(int mtg) { movestogo = std::max(mtg, 1); }

    std::optional<Milliseconds> allocated_time(Color c) const;
};
