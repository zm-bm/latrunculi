#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/types.hpp"

struct SearchLimits {
    static constexpr int max_depth = engine::max_search_depth;

    int  depth = max_depth;
    bool infinite{false};
    bool ponder{false};

    std::optional<Milliseconds> movetime;
    std::optional<NodeCount>    nodes;
    std::optional<Milliseconds> wtime;
    std::optional<Milliseconds> btime;
    std::optional<Milliseconds> winc;
    std::optional<Milliseconds> binc;
    std::optional<int>          movestogo;
    std::optional<int>          mate;
    std::vector<Move>           root_moves;

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
    void set_mate(int moves) { mate = std::max(moves, 1); }
    void set_root_moves(std::vector<Move> moves) {
        assert(!moves.empty());
        root_moves = std::move(moves);
    }

    [[nodiscard]] bool allows_root_move(Move move) const noexcept {
        return root_moves.empty()
            || std::find(root_moves.begin(), root_moves.end(), move) != root_moves.end();
    }

    [[nodiscard]] bool has_mate_within_limit(EvalValue value) const noexcept {
        if (!mate || *mate <= 0 || std::abs(value) <= eval_value::mate_bound)
            return false;

        const int distance_in_plies = eval_value::mate - std::abs(value);
        const int distance_in_moves = (distance_in_plies + 1) / 2;
        return distance_in_moves <= *mate;
    }

    std::optional<Milliseconds> allocated_time(Color c) const;
};
