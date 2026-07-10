#include "eval/evaluator.hpp"

void TermScore::add_score(TaperedScore score, Color color) {
    if (color == WHITE)
        white = score;
    else {
        black    = score;
        has_both = true;
    }
}

void ScoreTracker::add_score(EvalTerm term, TaperedScore score, Color color) {
    scores[term].add_score(score, color);
}

EvaluatorDebug::EvaluatorDebug(const Board& b)
    : evaluator(b, [this](EvalTerm term, Color color, TaperedScore score) {
          this->score_tracker.add_score(term, score, color);
      }) {}
