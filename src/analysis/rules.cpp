#include "chesswhy/analysis.hpp"
#include "chesswhy/positional.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

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

bool insufficient_material(const chess::Board& board) {
    if (board.pieces(chess::PieceType::PAWN, chess::PieceType::ROOK, chess::PieceType::QUEEN)) {
        return false;
    }

    const auto knights = board.pieces(chess::PieceType::KNIGHT);
    auto bishops = board.pieces(chess::PieceType::BISHOP);
    if (knights.count() + bishops.count() <= 1) {
        return true;
    }
    if (knights.count() != 0) {
        return false;
    }

    const chess::Square first(bishops.pop());
    while (bishops) {
        if (!chess::Square::same_color(first, chess::Square(bishops.pop()))) {
            return false;
        }
    }
    return true;
}

bool game_over(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    return moves.empty() || insufficient_material(board);
}

bool continuation_move(const chess::Board& before, chess::Move move) {
    if (before.isCapture(move) || before.inCheck()) {
        return true;
    }
    chess::Board after = before;
    after.makeMove(move);
    return after.inCheck();
}

Walk walk(const chess::Board& root, std::span<const std::string> pv_uci) {
    Walk result;
    chess::Board board = root;
    const auto limit = std::min(pv_uci.size(), static_cast<std::size_t>(kLinePlies));
    bool stopped = false;
    for (std::size_t index = 0; index < limit; ++index) {
        const auto move = detail::legal_move(board, pv_uci[index]);
        if (move == chess::Move::NO_MOVE ||
            (index >= static_cast<std::size_t>(kHorizonPlies) &&
             !continuation_move(board, move))) {
            break;
        }
        board.makeMove(move);
        ++result.plies;

        if (game_over(board)) {
            stopped = true;
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, board);
            if (legal_moves.empty()) {
                if (!board.inCheck()) {
                    result.terminal_draw = TerminalDraw{.kind = DrawKind::Stalemate,
                                                        .ply = result.plies};
                }
                break;
            }
            if (insufficient_material(board)) {
                result.terminal_draw = TerminalDraw{.kind = DrawKind::InsufficientMaterial,
                                                    .ply = result.plies};
            }
            break;
        }
    }
    if (!stopped && result.plies == kLinePlies &&
        pv_uci.size() > static_cast<std::size_t>(kLinePlies)) {
        const auto next = detail::legal_move(board, pv_uci[kLinePlies]);
        result.settled = next == chess::Move::NO_MOVE || !continuation_move(board, next);
    }
    return result;
}

int settle_ply(const chess::Board& root, std::span<const chess::Move> line, int k) {
    if (k < 1 || static_cast<std::size_t>(k) > line.size()) {
        throw std::invalid_argument("settle_ply requires a ply within the walked line");
    }
    chess::Board board = root;
    int j = k;
    for (int index = 0; index < k; ++index) {
        board.makeMove(line[static_cast<std::size_t>(index)]);
    }
    while (static_cast<std::size_t>(j) < line.size() &&
           continuation_move(board, line[static_cast<std::size_t>(j)])) {
        board.makeMove(line[static_cast<std::size_t>(j)]);
        ++j;
    }
    return j;
}

Consequence consequence(const chess::Board& root, std::span<const std::string> pv_uci,
                        const engine::Line& root_line) {
    Consequence result;
    chess::Board board = root;
    const auto threatening_side = root.sideToMove();
    const Walk walked = walk(root, pv_uci);
    result.plies = walked.plies;
    result.settled = walked.settled;
    result.terminal_draw = walked.terminal_draw;

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
        if (index > 0 && plies[index - 1].threatening && ply.stops_threat.value_or(false)) {
            if (ply.threatening) {
                ply.label = TempoPlyLabel::CounterThreat;
            } else if (!ply.reply_gain || !ply.reply_gain->gains) {
                ply.label = TempoPlyLabel::UnresolvedReply;
            } else if (*ply.reply_gain->gains) {
                ply.label = TempoPlyLabel::GainingReply;
            } else if (!ply.forced_test || !ply.forced_test->forced) {
                ply.label = TempoPlyLabel::UnresolvedReply;
            } else {
                ply.label = *ply.forced_test->forced ? TempoPlyLabel::PureReply
                                                     : TempoPlyLabel::UnforcedReply;
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
        balance += board.pieces(kPieceTypes[index], side).count() * value;
        balance -= board.pieces(kPieceTypes[index], ~side).count() * value;
    }
    return balance;
}

std::vector<InitiativeRun> initiative_runs(std::span<const TempoPly> plies,
                                           const chess::Board& root,
                                           std::span<const chess::Move> line,
                                           std::span<const PositionalChange> changes,
                                           bool walk_settled) {
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
        const chess::Color holder_color =
            holder == Side::White ? chess::Color::WHITE : chess::Color::BLACK;
        std::size_t last_gain = start;
        int tempi = 1;
        while (last_gain + 2 < n && plies[last_gain + 2].tempo_gained) {
            last_gain += 2;
            ++tempi;
        }
        const std::size_t ending_index = last_gain + 2;
        const auto [ending, end] = [&]() {
            if (ending_index >= n) {
                return std::pair{InitiativeEnding::Unfinished, n - 1};
            }
            if (!plies[ending_index].threatening) {
                return std::pair{InitiativeEnding::Spent, ending_index};
            }

            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, boards[ending_index + 1]);
            const bool checkmate = boards[ending_index + 1].inCheck() && legal_moves.empty();
            if (checkmate) {
                return std::pair{InitiativeEnding::Runs, ending_index};
            }
            if (ending_index + 1 >= n) {
                return std::pair{InitiativeEnding::Unfinished, ending_index};
            }

            const TempoPly& reply = plies[ending_index + 1];
            if (!reply.stops_threat) {
                throw std::logic_error("initiative_runs requires labelled tempo plies");
            }
            if (reply.label == TempoPlyLabel::CounterThreat) {
                return std::pair{InitiativeEnding::Contested, ending_index + 1};
            }
            if (reply.label == TempoPlyLabel::GainingReply ||
                reply.label == TempoPlyLabel::UnforcedReply) {
                return std::pair{InitiativeEnding::Answered, ending_index + 1};
            }
            if (reply.label == TempoPlyLabel::UnresolvedReply) {
                return std::pair{InitiativeEnding::Unresolved, ending_index + 1};
            }
            if (reply.label == TempoPlyLabel::PureReply) {
                // label_ledger sets tempo_gained on e iff e+1 is pure_reply, so e would
                // have been included as the run's last gain and could not be its ending ply.
                throw std::logic_error("initiative_runs requires tempo_gained to match pure_reply");
            }
            if (!*reply.stops_threat) {
                return std::pair{InitiativeEnding::Runs, ending_index + 1};
            }
            throw std::logic_error("initiative_runs requires labelled tempo plies");
        }();

        InitiativeOutcome outcome = InitiativeOutcome::Unknown;
        if (ending != InitiativeEnding::Unfinished) {
            const int settle = settle_ply(root, line, static_cast<int>(end + 1));
            const bool settle_is_settled = static_cast<std::size_t>(settle) < n || walk_settled;
            bool converted = false;

            chess::Movelist final_legal_moves;
            chess::movegen::legalmoves(final_legal_moves, boards[end + 1]);
            const bool holder_mated =
                boards[end + 1].inCheck() && final_legal_moves.empty() &&
                boards[end + 1].sideToMove() != holder_color;
            converted = holder_mated;

            if (!converted && settle_is_settled) {
                converted = material_balance(boards[static_cast<std::size_t>(settle)], holder_color) -
                                material_balance(boards[start], holder_color) >=
                            1;
            }
            if (!converted) {
                for (const PositionalChange& change : changes) {
                    if (!change.net_change || !change.in_use.value_or(false) ||
                        positional::is_dynamic(change.feature.tag) ||
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
            outcome = converted ? InitiativeOutcome::Converted
                                : settle_is_settled ? InitiativeOutcome::Evaporated
                                                    : InitiativeOutcome::Unknown;
        }
        runs.push_back(InitiativeRun{
            .holder = holder,
            .start_ply = static_cast<int>(start + 1),
            .end_ply = static_cast<int>(end + 1),
            .tempi = tempi,
            .ending = ending,
            .outcome = outcome,
        });
        scan = end;
        if (scan == start) {
            ++scan;
        }
    }
    return runs;
}

} // namespace chesswhy::analysis
