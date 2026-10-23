#include "chesswhy/analysis.hpp"

#include <algorithm>
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

bool judges_every_move(const Selection& selection) {
    return std::ranges::any_of(selection.keywords, [](Selection::Keyword keyword) {
        return keyword == Selection::Keyword::Playable ||
               keyword == Selection::Keyword::Inaccuracies ||
               keyword == Selection::Keyword::Mistakes || keyword == Selection::Keyword::Blunders ||
               keyword == Selection::Keyword::Unplayable || keyword == Selection::Keyword::All;
    });
}

int multipv_lines(const Selection& selection, int legal_moves) {
    if (legal_moves <= 0) {
        return 0;
    }
    if (judges_every_move(selection)) {
        return legal_moves;
    }
    const int requested =
        selection.top.has_value() ? std::max(kMinMultiPv, *selection.top) : kMinMultiPv;
    return std::min(legal_moves, requested);
}
bool is_detailed_line(int line_number, bool selected) {
    return line_number >= 1 && (line_number <= kDetailedLines || selected);
}

std::uint64_t search_a_nodes(Budget budget, int lines) {
    if (lines <= 0) {
        return 0;
    }
    return (node_budget(budget).a * static_cast<std::uint64_t>(lines)) / kMinMultiPv;
}

Verdict verdict(int loss_units, bool same_score_as_best, bool move_allows_tactic) {
    if (loss_units <= kPlayableUnits) {
        return same_score_as_best ? Verdict::Best : Verdict::Okay;
    }
    if (loss_units <= kInaccuracyUnits) {
        return Verdict::Inaccuracy;
    }
    return move_allows_tactic ? Verdict::Blunder : Verdict::Mistake;
}

bool same_score(const Eval& a, const Eval& b) {
    if (a.mate.has_value() || b.mate.has_value()) {
        return a.mate.has_value() && b.mate.has_value() && a.mate == b.mate;
    }
    return a.cp.has_value() && b.cp.has_value() && a.cp == b.cp;
}

bool is_tactic(const Consequence& consequence) {
    return consequence.cls == ConsequenceClass::Mate ||
           consequence.cls == ConsequenceClass::Material;
}

bool allows_tactic(const Consequence& line_consequence, const Eval& eval) {
    return line_consequence.material <= -1 || (eval.mate.has_value() && *eval.mate < 0);
}

bool is_opportunity_margin(int margin_units) {
    return margin_units >= kOpportunityMarginUnits;
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
