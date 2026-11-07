#pragma once

#include "search/worker.hpp"

class SearchTestAccess {
public:
    static void reset(search::Worker& worker) { worker.reset_search_state(); }

    static Board& board(search::Worker& worker) { return worker.board; }

    static int& root_iteration_depth(search::Worker& worker) { return worker.root_iteration_depth; }
    static void set_nodes(search::Worker& worker, NodeCount n) { worker.nodes.store(n); }

    static search::History& history(search::Worker& worker) { return worker.history; }

    // Direct slots are for fixture seeding and inspection, not production updates.
    static auto& quiet_history_entry(search::History& history, Color side, Square from, Square to) {
        return history.quiet_scores[side][from][to];
    }

    static auto& capture_history_entry(
        search::History& history, Color side, PieceType moving_piece, Square to, PieceType victim) {
        return history.capture_scores[side][piece_slot(moving_piece)][to][piece_slot(victim)];
    }

    static auto& continuation_history_entry(search::History& history,
                                            Color            previous_side,
                                            PieceType        previous_piece,
                                            Square           previous_to,
                                            PieceType        moving_piece,
                                            Square           to) {
        return (*history.continuation_scores)[search::History::continuation_index(
            search::OrderingContext::make_previous_key(previous_side, previous_piece, previous_to),
            moving_piece,
            to)];
    }

    static auto& continuation_history_entry(search::History&               history,
                                            const search::OrderingContext& context,
                                            PieceType                      moving_piece,
                                            Square                         to) {
        return history.continuation_entry(context, moving_piece, to);
    }

    static Move& counter_move_entry(search::History& history,
                                    Color            previous_side,
                                    PieceType        previous_piece,
                                    Square           previous_to) {
        return history.counter_moves[search::OrderingContext::make_previous_key(
            previous_side, previous_piece, previous_to)];
    }

    static Move& counter_move_entry(search::History&               history,
                                    const search::OrderingContext& context) {
        return history.counter_moves[context.previous_key];
    }

    static auto& frames(search::Worker& worker) { return worker.frames; }

    static const search::Limits& limits(const search::Worker& worker) { return worker.limits; }

    static void publish_root_snapshot(search::Worker& worker) { worker.publish_root_snapshot(); }

    static search::RootLine& accepted_result(search::Worker& worker) {
        return worker.accepted_result;
    }

    static std::vector<search::RootLine>& root_candidates(search::Worker& worker) {
        return worker.root_candidates;
    }

    static search::Instrumentation<>& instrumentation(search::Worker& worker) {
        return worker.stats;
    }

    template <search::NodeType Node>
    static Score alphabeta(search::Worker& worker,
                           Score           alpha,
                           Score           beta,
                           int             depth,
                           search::PV*     pv  = nullptr,
                           int             ply = 0) {
        auto* frame       = &worker.frames[ply];
        frame->pv_enabled = pv != nullptr;
        const Score value = worker.alphabeta<Node>(alpha, beta, depth, frame);
        if (pv)
            *pv = frame->pv;
        return value;
    }

    template <search::NodeType Node>
    static Score quiescence(
        search::Worker& worker, Score alpha, Score beta, search::PV* pv = nullptr, int ply = 0) {
        auto* frame       = &worker.frames[ply];
        frame->pv_enabled = pv != nullptr;
        const Score value = worker.quiescence<Node>(alpha, beta, frame);
        if (pv)
            *pv = frame->pv;
        return value;
    }

    static void build_root_candidates(search::Worker& worker) { worker.build_root_candidates(); }

    static void search_root(search::Worker& worker) { worker.search_root(); }

    static bool should_search_root_depth(const search::Worker& worker, int depth) noexcept {
        return worker.should_search_root_depth(depth);
    }

    static bool search_root_depth(search::Worker& worker, int depth, Score previous_value) {
        return worker.search_root_depth(depth, previous_value);
    }

    static bool search_root_window(
        search::Worker& worker, int search_depth, int nominal_depth, Score alpha, Score beta) {
        return worker.search_root_window(search_depth, nominal_depth, alpha, beta);
    }

    static void report_root_progress(search::Worker& worker, const search::RootLine& line) {
        worker.report_root_progress(line);
    }
};
