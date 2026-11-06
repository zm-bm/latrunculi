#include <algorithm>
#include <cassert>
#include <cmath>

#include "core/constants.hpp"
#include "eval/parameters.hpp"
#include "search/search_frame.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {
namespace {

// Singular-extension defaults; score margins use evaluation units.
constexpr int SingularMinDepth     = 8;
constexpr int SingularTtDepthSlack = 3;

EvalValue singular_margin(int depth) noexcept {
    return std::max(1, depth * eval::pawn.mg / 50);
}

// Exclusion belongs to this ply; verification stays active throughout the call.
// Restore the entire frame when a same-ply verification returns or is interrupted.
class ScopedSingularVerification {
public:
    ScopedSingularVerification(SearchFrame& frame, bool& active, Move excluded_move) noexcept
        : frame(frame),
          saved(frame),
          active(active),
          was_active(active) {
        assert(!excluded_move.is_null());
        frame.excluded_move = excluded_move;
        active              = true;
    }

    ~ScopedSingularVerification() {
        frame  = saved;
        active = was_active;
    }

    ScopedSingularVerification(const ScopedSingularVerification&)            = delete;
    ScopedSingularVerification& operator=(const ScopedSingularVerification&) = delete;

private:
    SearchFrame&      frame;
    const SearchFrame saved;
    bool&             active;
    const bool        was_active;
};

} // namespace

EvalValue Worker::search_excluding_move(
    EvalValue alpha, EvalValue beta, int depth, int ply, Move excluded_move) {
    assert(ply >= 0 && ply < engine::max_search_ply);
    assert(depth > 0);
    const ScopedSingularVerification verification{frames[ply], verification_active, excluded_move};
    return alphabeta<NodeType::NonPv>(alpha, beta, depth, ply, nullptr);
}

// Verify alternatives before extending the TT move or taking a multicut.
template <NodeType Node>
Worker::SingularResult Worker::singular_decision(
    const TTRecord& record, int depth, int ply, EvalValue alpha, EvalValue beta) {
    if (depth < SingularMinDepth || ply == 0 || ply >= 2 * root_iteration_depth
        || ply + 1 >= engine::max_search_ply || after_null_move(ply)
        || record.depth < depth - SingularTtDepthSlack
        || (record.bound != TTBound::Exact && record.bound != TTBound::LowerBound))
        return {};

    const auto& frame = frames[ply];
    if (!frame.excluded_move.is_null())
        return {};

    const EvalValue tt_score = record.score_at_ply(ply);
    if (std::abs(tt_score) >= eval_value::mate_bound || record.move.is_null()
        || !board.is_pseudo_legal(record.move) || !board.is_legal_pseudo_move(record.move))
        return {};

    const EvalValue singular_beta = tt_score - singular_margin(depth);
    if (singular_beta <= -eval_value::mate_bound || singular_beta >= eval_value::mate_bound)
        return {};

    // Verifier descendants retain ordinary search and extensions, but cannot take multicuts.
    const bool multicut_allowed =
        Node == NodeType::NonPv && !verification_active && !board.is_check()
        && alpha > -eval_value::mate_bound && alpha < eval_value::mate_bound
        && beta > -eval_value::mate_bound && beta < eval_value::mate_bound
        && singular_beta - 1 > -eval_value::mate_bound && singular_beta >= beta;

    const EvalValue value =
        search_excluding_move(singular_beta - 1, singular_beta, (depth - 1) / 2, ply, record.move);
    if (stop_requested())
        return {};

    if (value < singular_beta)
        return {.extend = true};
    if (multicut_allowed)
        return {.cutoff = singular_beta};
    return {};
}

// The policy differs between PV and non-PV nodes; keep both instantiations here.
template Worker::SingularResult
Worker::singular_decision<NodeType::Pv>(const TTRecord&, int, int, EvalValue, EvalValue);
template Worker::SingularResult
Worker::singular_decision<NodeType::NonPv>(const TTRecord&, int, int, EvalValue, EvalValue);

} // namespace search
