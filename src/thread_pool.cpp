#include "thread_pool.hpp"

#include <algorithm>
#include <iterator>

#include "search_stats.hpp"
#include "thread.hpp"

ThreadPool::ThreadPool(size_t thread_count, uci::Protocol& protocol) : protocol(protocol) {
    for (size_t i = 0; i < thread_count; ++i) {
        threads.push_back(std::make_unique<Thread>(i, protocol, *this));
    }
}

bool ThreadPool::start_all(SearchOptions& options) {
    if (is_busy())
        return false;

    tt.age_table();

    for (auto& thread : threads) {
        thread->start(options);
    }

    return true;
}

void ThreadPool::shutdown_all() {
    for (auto& thread : threads) {
        thread->shutdown();
    }
}

void ThreadPool::halt_all() {
    for (auto& thread : threads) {
        thread->halt();
    }
}

void ThreadPool::wait_all() {
    for (auto& thread : threads) {
        thread->wait();
    }
}

void ThreadPool::halt_helpers() {
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->halt();
    }
}

void ThreadPool::wait_helpers() {
    for (size_t i = 1; i < threads.size(); ++i) {
        threads[i]->wait();
    }
}

bool ThreadPool::resize(size_t thread_count) {
    if (is_busy())
        return false;

    if (thread_count == threads.size())
        return true;

    if (thread_count < threads.size()) {
        for (size_t i = thread_count; i < threads.size(); ++i) {
            threads[i]->shutdown();
        }
        threads.resize(thread_count);
    } else {
        for (size_t i = threads.size(); i < thread_count; ++i) {
            threads.push_back(std::make_unique<Thread>(i, protocol, *this));
        }
    }

    return true;
}

bool ThreadPool::is_busy() const {
    return std::any_of(
        threads.begin(), threads.end(), [](const auto& thread) { return thread->is_busy(); });
}

int ThreadPool::size() const {
    return threads.size();
}

Move ThreadPool::best_voted_move(const Board& board, Move fallback) const {
    struct Vote {
        Move     move{NULL_MOVE};
        int64_t  weight{0};
        int      depth{0};
        int      value{-INF_VALUE};
        uint64_t nodes{0};
    };

    auto              root_moves = movegen::generate_pseudo_legal(board);
    std::vector<Move> legal_root_moves;
    legal_root_moves.reserve(root_moves.size());
    for (const auto& move : root_moves) {
        if (board.is_legal_generated_move(move))
            legal_root_moves.push_back(move);
    }

    const auto is_legal_root_move = [&](Move move) {
        return std::find(legal_root_moves.begin(), legal_root_moves.end(), move) !=
               legal_root_moves.end();
    };

    std::vector<RootSearchResult> results;
    results.reserve(threads.size());

    for (const auto& thread : threads) {
        auto result = thread->root_result_snapshot();
        if (!result.completed || result.depth <= 0 || result.move.is_null() ||
            !is_legal_root_move(result.move))
            continue;
        results.push_back(result);
    }

    if (results.empty())
        return fallback;

    const int weakest =
        std::min_element(results.begin(), results.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.value < rhs.value;
        })->value;

    std::vector<Vote> votes;
    votes.reserve(results.size());

    for (const auto& result : results) {
        auto it = std::find_if(
            votes.begin(), votes.end(), [&](const Vote& vote) { return vote.move == result.move; });

        if (it == votes.end()) {
            votes.push_back({result.move});
            it = std::prev(votes.end());
        }

        it->weight += int64_t(result.depth) * 1000 + std::max(0, result.value - weakest);
        it->depth   = std::max(it->depth, result.depth);
        it->value   = std::max(it->value, result.value);
        it->nodes  += result.nodes;
    }

    const auto better = [](const Vote& lhs, const Vote& rhs) {
        if (lhs.weight != rhs.weight)
            return lhs.weight < rhs.weight;
        if (lhs.depth != rhs.depth)
            return lhs.depth < rhs.depth;
        if (lhs.value != rhs.value)
            return lhs.value < rhs.value;
        if (lhs.nodes != rhs.nodes)
            return lhs.nodes < rhs.nodes;
        return lhs.move.bits > rhs.move.bits;
    };

    return std::max_element(votes.begin(), votes.end(), better)->move;
}

template <typename T>
T ThreadPool::accumulate(T Thread::*member) const {
    T total = T{};
    for (const auto& thread : threads) {
        total += (*thread).*member;
    }
    return total;
}

template uint64_t      ThreadPool::accumulate<uint64_t>(uint64_t Thread::*member) const;
template SearchStats<> ThreadPool::accumulate<SearchStats<>>(SearchStats<> Thread::*member) const;
