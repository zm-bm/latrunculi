#include <algorithm>
#include <cmath>

#include "core/constants.hpp"
#include "eval/parameters.hpp"
#include "search/search_frame.hpp"
#include "search/search_policy.hpp"
#include "search/tt.hpp"

namespace search::detail {
namespace {

// Singular-extension defaults; score margins use evaluation units.
constexpr int SingularMinDepth     = 8;
constexpr int SingularTtDepthSlack = 3;

Score singular_margin(int depth) noexcept {
    return std::max(1, depth * eval::pawn.mg / 50);
}

} // namespace

// Verify alternatives before extending the TT move or taking a multicut.
template <NodeType Node>
std::optional<SingularVerification> singular_verification(const Board&       board,
                                                          const TTRecord&    record,
                                                          const SearchFrame& frame,
                                                          int                ply,
                                                          int                root_depth,
                                                          int                depth,
                                                          Score              alpha,
                                                          Score              beta,
                                                          bool               verification_active) {
    if (depth < SingularMinDepth || ply == 0 || ply >= 2 * root_depth
        || ply + 1 >= engine::max_search_ply || after_null_move(board, ply)
        || record.depth < depth - SingularTtDepthSlack
        || (record.bound != TTBound::Exact && record.bound != TTBound::LowerBound))
        return {};

    if (!frame.excluded_move.is_null())
        return {};

    const Score tt_score = record.score_at_ply(ply);
    if (std::abs(tt_score) >= scores::mate_bound || record.move.is_null()
        || !board.is_pseudo_legal(record.move) || !board.is_legal_pseudo_move(record.move))
        return {};

    const Score singular_beta = tt_score - singular_margin(depth);
    if (singular_beta <= -scores::mate_bound || singular_beta >= scores::mate_bound)
        return {};

    // Verifier descendants retain ordinary search and extensions, but cannot take multicuts.
    const bool multicut_allowed = Node == NodeType::NonPv && !verification_active
                               && !board.is_check() && alpha > -scores::mate_bound
                               && alpha < scores::mate_bound && beta > -scores::mate_bound
                               && beta < scores::mate_bound
                               && singular_beta - 1 > -scores::mate_bound && singular_beta >= beta;

    return SingularVerification{
        .beta = singular_beta, .depth = (depth - 1) / 2, .multicut_allowed = multicut_allowed};
}

// The policy differs between PV and non-PV nodes; keep both instantiations here.
template std::optional<SingularVerification> singular_verification<NodeType::Pv>(
    const Board&, const TTRecord&, const SearchFrame&, int, int, int, Score, Score, bool);
template std::optional<SingularVerification> singular_verification<NodeType::NonPv>(
    const Board&, const TTRecord&, const SearchFrame&, int, int, int, Score, Score, bool);

} // namespace search::detail
