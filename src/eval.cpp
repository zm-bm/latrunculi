#include "eval.hpp"

std::string EvalVerbose::toString() const {
    int result =
        eval.board.sideToMove() == WHITE ? eval.scores.finalResult : -eval.scores.finalResult;

    std::ostringstream os;
    os << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    os << "     Term    |    White    |    Black    |    Total   \n";
    os << "             |   MG    EG  |   MG    EG  |   MG    EG \n";
    os << " ------------+-------------+-------------+------------\n";
    os << std::setw(12) << "Material" << scoreTracker[EvalTerm::Material];
    os << std::setw(12) << "Piece Sq." << scoreTracker[EvalTerm::PieceSquares];
    os << std::setw(12) << "Pawns" << scoreTracker[EvalTerm::Pawns];
    os << std::setw(12) << "Knights" << scoreTracker[EvalTerm::Knights];
    os << std::setw(12) << "Bishops" << scoreTracker[EvalTerm::Bishops];
    os << std::setw(12) << "Rooks" << scoreTracker[EvalTerm::Rooks];
    os << std::setw(12) << "Queens" << scoreTracker[EvalTerm::Queens];
    os << std::setw(12) << "Kings" << scoreTracker[EvalTerm::King];
    os << std::setw(12) << "Mobility" << scoreTracker[EvalTerm::Mobility];
    os << std::setw(12) << "Threats" << scoreTracker[EvalTerm::Threats];
    os << " ------------+-------------+-------------+------------\n";
    os << std::setw(12) << "Total" << TermData{eval.scores.finalScore};
    os << "\nEvaluation:\t" << double(result) / PAWN_VALUE_MG << '\n';
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const EvalVerbose& eval) {
    return os << eval.toString();
}
