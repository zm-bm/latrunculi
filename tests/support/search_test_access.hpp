#pragma once

#include "search/search_worker.hpp"

class SearchTestAccess {
public:
    static void reset(SearchWorker& worker) { worker.reset_search_state(); }

    static Board& board(SearchWorker& worker) { return worker.board; }

    static int& search_ply(SearchWorker& worker) { return worker.search_ply; }

    static MoveOrdering& ordering(SearchWorker& worker) { return worker.ordering; }

    static RootLine& root_result(SearchWorker& worker) { return worker.root_result; }

    static std::vector<RootLine>& root_lines(SearchWorker& worker) { return worker.root_lines; }

    static SearchInstrumentation<>& instrumentation(SearchWorker& worker) { return worker.stats; }

    template <NodeType Node>
    static EvalValue alphabeta(SearchWorker&       worker,
                               EvalValue           alpha,
                               EvalValue           beta,
                               int                 depth,
                               PrincipalVariation* pv       = nullptr,
                               bool                can_null = true) {
        return worker.alphabeta<Node>(alpha, beta, depth, pv, can_null);
    }

    template <NodeType Node>
    static EvalValue quiescence(SearchWorker&       worker,
                                EvalValue           alpha,
                                EvalValue           beta,
                                PrincipalVariation* pv = nullptr) {
        return worker.quiescence<Node>(alpha, beta, pv);
    }

    static void build_root_lines(SearchWorker& worker) { worker.build_root_lines(); }

    static EvalValue search_root(SearchWorker& worker) { return worker.search_root(); }

    static bool search_root_depth(SearchWorker& worker, int depth, EvalValue previous_value) {
        return worker.search_root_depth(depth, previous_value);
    }

    static bool
    search_root_window(SearchWorker& worker, int depth, EvalValue alpha, EvalValue beta) {
        return worker.search_root_window(depth, alpha, beta);
    }

    static void report_root_progress(SearchWorker& worker, const RootLine& line) {
        worker.report_root_progress(line);
    }
};
