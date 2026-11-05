#pragma once

#include "search/worker.hpp"

class SearchTestAccess {
public:
    static void reset(search::Worker& worker) { worker.reset_search_state(); }

    static Board& board(search::Worker& worker) { return worker.board; }

    static int& search_ply(search::Worker& worker) { return worker.search_ply; }

    static int& root_iteration_depth(search::Worker& worker) { return worker.root_iteration_depth; }
    static void set_nodes(search::Worker& worker, NodeCount n) { worker.nodes.store(n); }

    static search::ordering::State& ordering_state(search::Worker& worker) {
        return worker.ordering_state;
    }

    static auto& frames(search::Worker& worker) { return worker.frames; }

    static const search::Limits& limits(const search::Worker& worker) { return worker.limits; }

    static search::RootLine& root_result(search::Worker& worker) { return worker.root_result; }

    static std::vector<search::RootLine>& root_lines(search::Worker& worker) {
        return worker.root_lines;
    }

    static search::Instrumentation<>& instrumentation(search::Worker& worker) {
        return worker.stats;
    }

    template <search::NodeType Node>
    static EvalValue alphabeta(search::Worker&             worker,
                               EvalValue                   alpha,
                               EvalValue                   beta,
                               int                         depth,
                               search::PrincipalVariation* pv = nullptr) {
        return worker.alphabeta<Node>(alpha, beta, depth, pv);
    }

    static bool after_null_move(const search::Worker& worker) { return worker.after_null_move(); }

    static EvalValue search_excluding_move(
        search::Worker& worker, EvalValue alpha, EvalValue beta, int depth, Move excluded_move) {
        return worker.search_excluding_move(alpha, beta, depth, excluded_move);
    }
    template <search::NodeType Node = search::NodeType::NonPv>
    static auto singular_result(search::Worker&         worker,
                                const search::TTRecord& record,
                                int                     depth,
                                EvalValue               alpha = -eval_value::inf,
                                EvalValue               beta  = eval_value::inf) {
        return worker.singular_extension<Node>(record, depth, alpha, beta);
    }

    template <search::NodeType Node>
    static EvalValue quiescence(search::Worker&             worker,
                                EvalValue                   alpha,
                                EvalValue                   beta,
                                search::PrincipalVariation* pv = nullptr) {
        return worker.quiescence<Node>(alpha, beta, pv);
    }

    static void build_root_lines(search::Worker& worker) { worker.build_root_lines(); }

    static void search_root(search::Worker& worker) { worker.search_root(); }

    static bool should_search_root_depth(const search::Worker& worker, int depth) noexcept {
        return worker.should_search_root_depth(depth);
    }

    static bool search_root_depth(search::Worker& worker, int depth, EvalValue previous_value) {
        return worker.search_root_depth(depth, previous_value);
    }

    static bool search_root_window(search::Worker& worker,
                                   int             search_depth,
                                   int             nominal_depth,
                                   EvalValue       alpha,
                                   EvalValue       beta) {
        return worker.search_root_window(search_depth, nominal_depth, alpha, beta);
    }

    static void report_root_progress(search::Worker& worker, const search::RootLine& line) {
        worker.report_root_progress(line);
    }
};
