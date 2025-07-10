#pragma once

#include "constants.hpp"
#include "types.hpp"

/**
 * @struct TermData
 * @brief Stores evaluation scores for both sides of a chess position.
 *
 * When displaying data, the structure formats output based on whether scores
 * for both sides are available.
 */
struct TermData {
    Score white  = ZERO_SCORE;
    Score black  = ZERO_SCORE;
    bool hasBoth = false;

    void addScore(Score score, Color color) {
        if (color == WHITE)
            white = score;
        else {
            black   = score;
            hasBoth = true;
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const TermData& term) {
        os << " | ";
        if (term.hasBoth) {
            os << term.white << " | ";
            os << term.black << " | ";
            os << term.white - term.black;
        } else {
            os << " ----  ---- |  ----  ---- | " << term.white;
        }
        os << '\n';
        return os;
    }
};

/**
 * @struct ScoreTracker
 * @brief Tracks evaluation scores for different chess evaluation terms.
 *
 * This template structure stores evaluation data conditionally based on verbosity level.
 * In Verbose mode, it maintains detailed scoring information for each evaluation term.
 * In Silent mode, it provides the same interface but performs no storage operations,
 * resulting in zero runtime overhead.
 *
 * @tparam V The verbosity level (Verbose or Silent) that determines storage behavior.
 *           When V == Verbose, stores and tracks scores.
 *           When V == Silent, provides no-op implementations with zero overhead.
 */
template <Verbosity V>
struct ScoreTracker {
    using Storage = std::conditional_t<V == Verbose, TermData, std::nullptr_t>;
    Storage terms[idx(EvalTerm::Count)];

    void addScore(EvalTerm term, Score score, Color color) {
        if constexpr (V == Verbose) {
            terms[idx(term)].addScore(score, color);
        }
    }

    template <Verbosity U = V>
    std::enable_if_t<U == Verbose, const TermData&> operator[](EvalTerm term) const {
        return terms[idx(term)];
    }
};
