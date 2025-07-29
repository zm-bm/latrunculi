#include "evaluator.hpp"

void TermScore::add_score(Score score, Color color) {
    if (color == WHITE)
        white = score;
    else {
        black    = score;
        has_both = true;
    }
}

void ScoreTracker::add_score(EvalTerm term, Score score, Color color) {
    scores[term].add_score(score, color);
}

EvaluatorDebug::EvaluatorDebug(const Board& b)
    : evaluator(b, [this](EvalTerm term, Color color, Score score) {
          this->score_tracker.add_score(term, score, color);
      }) {}
