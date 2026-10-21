#include "chesswhy/analysis.hpp"

#include <cstddef>
#include <string>

#include <chesswhy/tactics.hpp>

#include "move_utils.hpp"

namespace chesswhy::analysis {

NodeBudget node_budget(Budget budget) {
    switch (budget) {
    case Budget::Quick:
        return {.a = 250'000, .b = 50'000, .c = 50'000, .d = 25'000, .e = 25'000, .r = 25'000};
    case Budget::Standard:
        return {
            .a = 1'000'000, .b = 200'000, .c = 200'000, .d = 100'000, .e = 100'000, .r = 100'000};
    case Budget::Deep:
        return {
            .a = 4'000'000, .b = 800'000, .c = 800'000, .d = 400'000, .e = 400'000, .r = 400'000};
    }
    return {};
}

int es_units(const engine::Line& line) {
    return (2 * line.win) + line.draw;
}
bool is_tactic_margin(int margin_units) {
    return margin_units >= kTacticMarginUnits;
}

Verdict verdict_for_loss(int loss_units) {
    if (loss_units <= kFineUnits) {
        return Verdict::Fine;
    }
    if (loss_units < kMistakeUnits) {
        return Verdict::Inaccuracy;
    }
    if (loss_units < kBlunderUnits) {
        return Verdict::Mistake;
    }
    return Verdict::Blunder;
}

bool null_move_prone(const chess::Board& board) {
    const auto side = board.sideToMove();
    const auto non_pawn_non_king =
        board.us(side) ^
        (board.pieces(chess::PieceType::PAWN, chess::PieceType::KING) & board.us(side));
    return non_pawn_non_king.count() <= 1;
}

int walked_plies(const chess::Board& root, std::span<const std::string> pv_uci) {
    chess::Board board = root;
    constexpr auto max_plies = static_cast<std::size_t>(kLinePlies);
    constexpr auto initial_plies = static_cast<std::size_t>(kHorizonPlies);
    std::size_t plies = 0;

    for (std::size_t index = 0; index < pv_uci.size() && index < max_plies; ++index) {
        const auto move = detail::legal_move(board, pv_uci[index]);
        if (move == chess::Move::NO_MOVE || (index >= initial_plies && !board.isCapture(move))) {
            break;
        }
        board.makeMove(move);
        ++plies;
    }
    return static_cast<int>(plies);
}

Consequence consequence(const chess::Board& root, std::span<const std::string> pv_uci,
                        const engine::Line& root_line) {
    Consequence result;
    chess::Board board = root;
    const auto threatening_side = root.sideToMove();
    result.plies = walked_plies(root, pv_uci);

    for (std::size_t index = 0; index < static_cast<std::size_t>(result.plies); ++index) {
        const auto move = detail::legal_move(board, pv_uci[index]);
        if (move == chess::Move::NO_MOVE) {
            break;
        }

        const int mover_sign = board.sideToMove() == threatening_side ? 1 : -1;
        if (board.isCapture(move)) {
            result.material +=
                mover_sign * tactics::piece_value(board.getCapturing<chess::PieceType>(move));
        }
        if (move.typeOf() == chess::Move::PROMOTION) {
            const int promotion_delta = tactics::piece_value(move.promotionType()) -
                                        tactics::piece_value(chess::PieceType::PAWN);
            result.material += mover_sign * promotion_delta;
            if (board.sideToMove() == threatening_side) {
                result.promotes = true;
            }
        }

        board.makeMove(move);
    }

    if (root_line.mate.has_value() && *root_line.mate > 0) {
        result.cls = ConsequenceClass::Mate;
        result.mate_in = root_line.mate;
    } else if (result.material >= 1) {
        result.cls = ConsequenceClass::Material;
    } else if (result.promotes) {
        result.cls = ConsequenceClass::Promotion;
    } else {
        result.cls = ConsequenceClass::Positional;
    }
    return result;
}

int consequence_rank(const Consequence& consequence) {
    switch (consequence.cls) {
    case ConsequenceClass::Mate:
        return 4;
    case ConsequenceClass::Material:
        if (consequence.material >= 3) {
            return 3;
        }
        if (consequence.material >= 1) {
            return 2;
        }
        return 0;
    case ConsequenceClass::Promotion:
        return 1;
    case ConsequenceClass::Positional:
        return 0;
    }
    return 0;
}

} // namespace chesswhy::analysis
