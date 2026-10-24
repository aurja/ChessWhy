#include "chesswhy/analysis.hpp"
#include "chesswhy/positional.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

#include <chesswhy/tactics.hpp>

#include "move_utils.hpp"

namespace chesswhy::analysis {

NodeBudget node_budget(Budget budget) {
    switch (budget) {
    case Budget::Quick:
        return {.a = 250'000,
                .b = 50'000,
                .c = 50'000,
                .d = 25'000,
                .e = 25'000,
                .l = 10'000,
                .r = 25'000};
    case Budget::Standard:
        return {.a = 1'000'000,
                .b = 200'000,
                .c = 200'000,
                .d = 100'000,
                .e = 100'000,
                .l = 30'000,
                .r = 100'000};
    case Budget::Deep:
        return {.a = 4'000'000,
                .b = 800'000,
                .c = 800'000,
                .d = 400'000,
                .e = 400'000,
                .l = 160'000,
                .r = 400'000};
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

std::optional<TempoLabel> tempo_label(int value_units) {
    if (value_units > kTempoDependentUnits) {
        return TempoLabel::TempoDependent;
    }
    if (value_units < 0) {
        return TempoLabel::Zugzwang;
    }
    if (value_units < kNonTempoUnits) {
        return TempoLabel::NonTempo;
    }
    return std::nullopt;
}

bool tempo_dependent(const std::optional<TempoValue>& root) {
    return root && root->label == TempoLabel::TempoDependent;
}

void label_ledger(std::vector<TempoPly>& plies) {
    for (std::size_t index = 0; index < plies.size(); ++index) {
        TempoPly& ply = plies[index];
        if (index > 0 && plies[index - 1].threatening) {
            if (ply.stops_threat.value_or(false)) {
                ply.label =
                    ply.threatening ? TempoPlyLabel::CounterThreat : TempoPlyLabel::PureReply;
            } else {
                ply.label = ply.threatening ? TempoPlyLabel::Threatening : TempoPlyLabel::Neither;
            }
        } else {
            ply.label = ply.threatening ? TempoPlyLabel::Threatening : TempoPlyLabel::Neither;
        }
        ply.tempo_gained = false;
    }
    for (std::size_t index = 0; index + 1 < plies.size(); ++index) {
        plies[index].tempo_gained = plies[index + 1].label == TempoPlyLabel::PureReply;
    }
}

int material_balance(const chess::Board& board, chess::Color side) {
    constexpr std::array kPieceValues{1, 3, 3, 5, 9};
    constexpr std::array kPieceTypes{chess::PieceType::PAWN, chess::PieceType::KNIGHT,
                                     chess::PieceType::BISHOP, chess::PieceType::ROOK,
                                     chess::PieceType::QUEEN};
    int balance = 0;
    for (std::size_t index = 0; index < kPieceTypes.size(); ++index) {
        const int value = kPieceValues[index];
        balance += static_cast<int>(board.pieces(kPieceTypes[index], side).count()) * value;
        balance -= static_cast<int>(board.pieces(kPieceTypes[index], ~side).count()) * value;
    }
    return balance;
}

std::vector<InitiativeRun> initiative_runs(std::span<const TempoPly> plies,
                                           const chess::Board& root,
                                           std::span<const chess::Move> line,
                                           std::span<const PositionalChange> changes) {
    if (line.size() != plies.size()) {
        throw std::logic_error("initiative_runs requires one line move per ledger ply");
    }
    const std::size_t n = plies.size();
    std::vector<InitiativeRun> runs;
    if (n == 0 || std::find_if(plies.begin(), plies.begin() + static_cast<std::ptrdiff_t>(n),
                               [](const TempoPly& ply) { return ply.tempo_gained; }) ==
                      plies.begin() + static_cast<std::ptrdiff_t>(n)) {
        return runs;
    }

    std::vector<chess::Board> boards;
    boards.reserve(n + 1);
    chess::Board board = root;
    boards.push_back(board);
    for (std::size_t index = 0; index < n; ++index) {
        board.makeMove(line[index]);
        boards.push_back(board);
    }

    runs.reserve(n);
    std::size_t scan = 0;
    while (scan < n) {
        if (!plies[scan].tempo_gained) {
            ++scan;
            continue;
        }

        const std::size_t start = scan;
        const Side holder = plies[start].side;
        std::size_t last_gain = start;
        int tempi = 1;
        while (last_gain + 2 < n && plies[last_gain + 2].tempo_gained) {
            last_gain += 2;
            ++tempi;
        }
        const std::size_t ending_index = last_gain + 2;
        InitiativeEnding ending = InitiativeEnding::Unfinished;
        std::size_t end = n - 1;
        if (ending_index >= n) {
            end = n - 1;
        } else if (!plies[ending_index].threatening) {
            ending = InitiativeEnding::Spent;
            end = ending_index;
        } else {
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, boards[ending_index + 1]);
            const bool checkmate = boards[ending_index + 1].inCheck() && legal_moves.empty();
            if (checkmate) {
                ending = InitiativeEnding::Runs;
                end = ending_index;
            } else if (ending_index + 1 >= n) {
                ending = InitiativeEnding::Unfinished;
                end = ending_index;
            } else {
                const TempoPly& reply = plies[ending_index + 1];
                if (!reply.stops_threat) {
                    throw std::logic_error("initiative_runs requires labelled tempo plies");
                }
                if (reply.label == TempoPlyLabel::CounterThreat) {
                    ending = InitiativeEnding::Contested;
                    end = ending_index + 1;
                } else if (!*reply.stops_threat) {
                    ending = InitiativeEnding::Runs;
                    end = ending_index + 1;
                } else {
                    throw std::logic_error("initiative_runs requires labelled tempo plies");
                }
            }
        }

        bool converted =
            material_balance(boards[n],
                             holder == Side::White ? chess::Color::WHITE : chess::Color::BLACK) -
                material_balance(boards[start], holder == Side::White ? chess::Color::WHITE
                                                                      : chess::Color::BLACK) >=
            1;
        if (!converted) {
            for (const PositionalChange& change : changes) {
                if (!change.net_change || !change.in_use.value_or(false) ||
                    change.ply < static_cast<int>(start + 1) ||
                    change.ply > static_cast<int>(end + 1)) {
                    continue;
                }
                const auto favoured = positional::favours(change);
                if (favoured && *favoured == holder) {
                    converted = true;
                    break;
                }
            }
        }
        runs.push_back(InitiativeRun{
            .holder = holder,
            .start_ply = static_cast<int>(start + 1),
            .end_ply = static_cast<int>(end + 1),
            .tempi = tempi,
            .ending = ending,
            .outcome = converted ? InitiativeOutcome::Converted : InitiativeOutcome::Evaporated,
        });
        scan = end;
        if (scan == start) {
            ++scan;
        }
    }
    return runs;
}

} // namespace chesswhy::analysis
