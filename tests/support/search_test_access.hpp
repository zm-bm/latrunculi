#pragma once

#include "search/search_policy.hpp"
#include "search/worker.hpp"

class SearchTestAccess {
public:
    static void reset(search::Worker& worker) { worker.reset_search_state(); }

    static Board& board(search::Worker& worker) { return worker.board; }

    static int& search_ply(search::Worker& worker) { return worker.search_ply; }

    static bool& verification_active(search::Worker& worker) { return worker.verification_active; }

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

    static search::RootLine& root_result(search::Worker& worker) { return worker.root_result; }

    static std::vector<search::RootLine>& root_lines(search::Worker& worker) {
        return worker.root_lines;
    }

    static search::Instrumentation<>& instrumentation(search::Worker& worker) {
        return worker.stats;
    }

    template <search::NodeType Node>
    static Score alphabeta(search::Worker&             worker,
                           Score                       alpha,
                           Score                       beta,
                           int                         depth,
                           search::PrincipalVariation* pv = nullptr) {
        return worker.alphabeta<Node>(alpha, beta, depth, pv);
    }

    static Score search_excluding_move(
        search::Worker& worker, Score alpha, Score beta, int depth, Move excluded_move) {
        const search::detail::ScopedSingularVerification scope{
            worker.frames[worker.search_ply], worker.verification_active, excluded_move};
        return worker.alphabeta<search::NodeType::NonPv>(alpha, beta, depth, nullptr);
    }
    template <search::NodeType Node = search::NodeType::NonPv>
    static auto singular_result(search::Worker&         worker,
                                const search::TTRecord& record,
                                int                     depth,
                                Score                   alpha = -scores::inf,
                                Score                   beta  = scores::inf) {
        struct Result {
            bool                 extend{false};
            std::optional<Score> cutoff;
        };
        const auto request =
            search::detail::singular_verification<Node>(worker.board,
                                                        record,
                                                        worker.frames[worker.search_ply],
                                                        worker.search_ply,
                                                        worker.root_iteration_depth,
                                                        depth,
                                                        alpha,
                                                        beta,
                                                        worker.verification_active);
        if (!request)
            return Result{};
        const auto value = search_excluding_move(
            worker, request->beta - 1, request->beta, request->depth, record.move);
        if (worker.stop_requested())
            return Result{};
        if (value < request->beta)
            return Result{.extend = true};
        if (request->multicut_allowed)
            return Result{.cutoff = request->beta};
        return Result{};
    }

    template <search::NodeType Node>
    static Score quiescence(search::Worker&             worker,
                            Score                       alpha,
                            Score                       beta,
                            search::PrincipalVariation* pv = nullptr) {
        return worker.quiescence<Node>(alpha, beta, pv);
    }

    static void build_root_lines(search::Worker& worker) { worker.build_root_lines(); }

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
