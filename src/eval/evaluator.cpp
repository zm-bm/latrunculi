#include "eval/evaluator.hpp"

namespace eval {

void TermScore::add_score(TaperedScore score, Color color) {
    if (color == WHITE)
        white = score;
    else {
        black    = score;
        has_both = true;
    }
}

void ScoreTracker::add_score(Term term, TaperedScore score, Color color) {
    scores[std::to_underlying(term)].add_score(score, color);
}

EvaluatorDebug::EvaluatorDebug(const Board& b)
    : evaluator(b, [this](Term term, Color color, TaperedScore score) {
          this->score_tracker.add_score(term, score, color);
      }) {}

} // namespace eval
