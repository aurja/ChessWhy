#include "chesswhy/analysis.hpp"
#include "chesswhy/positional.hpp"
#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "move_utils.hpp"

namespace chesswhy::analysis {
namespace {

// M1 §4.5: the threat still works iff ES_O(D) >= ES_O(B) - 0.03.
constexpr int kParryMarginUnits = 60;

[[nodiscard]] Side report_side(chess::Color color) noexcept {
    return color == chess::Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] TempoValue make_tempo_value(Side side, int moves_units, int passes_units) {
    const int value_units = moves_units - passes_units;
    return TempoValue{.side = side,
                      .moves_units = moves_units,
                      .passes_units = passes_units,
                      .value_units = value_units,
                      .label = tempo_label(value_units)};
}

[[nodiscard]] Eval eval_for(const engine::Line& line, chess::Color searched_side,
                            chess::Color requested_side) {
    const int sign = searched_side == requested_side ? 1 : -1;
    Eval result;
    const int side_units = es_units(line);
    result.es_units = sign > 0 ? side_units : 2000 - side_units;
    if (line.cp.has_value()) {
        result.cp = sign * *line.cp;
    }
    if (line.mate.has_value()) {
        result.mate = sign * *line.mate;
    }
    return result;
}

[[nodiscard]] std::vector<std::string> san_line(const chess::Board& root,
                                                std::span<const std::string> pv_uci) {
    std::vector<std::string> result;
    result.reserve(std::min(pv_uci.size(), static_cast<std::size_t>(kLinePlies)));
    chess::Board board = root;
    const auto count = std::min(pv_uci.size(), static_cast<std::size_t>(kLinePlies));
    for (std::size_t index = 0; index < count; ++index) {
        const auto move = detail::legal_move(board, pv_uci[index]);
        if (move == chess::Move::NO_MOVE) {
            break;
        }
        result.push_back(chess::uci::moveToSan(board, move));
        board.makeMove(move);
    }
    return result;
}
[[nodiscard]] std::vector<chess::Move> walked_moves(const chess::Board& root,
                                                    std::span<const std::string> pv_uci) {
    const auto plies = static_cast<std::size_t>(walked_plies(root, pv_uci));
    std::vector<chess::Move> moves;
    moves.reserve(plies);
    chess::Board board = root;
    for (std::size_t index = 0; index < plies; ++index) {
        const auto move = detail::legal_move(board, pv_uci[index]);
        if (move == chess::Move::NO_MOVE) {
            break;
        }
        moves.push_back(move);
        board.makeMove(move);
    }
    return moves;
}

[[nodiscard]] Threat make_threat(const chess::Board& root, std::span<const std::string> pv_uci,
                                 const engine::Line& line, Side side) {
    Threat result;
    result.side = side;
    result.root_fen = root.getFen();
    result.line_san = san_line(root, pv_uci);
    result.consequence = consequence(root, pv_uci, line);
    const auto moves = walked_moves(root, pv_uci);
    result.motifs = tactics::line_motifs(root, std::span<const chess::Move>(moves));
    return result;
}

[[nodiscard]] const engine::Line& first_line(const engine::SearchResult& result) {
    if (result.lines.empty()) {
        throw engine::EngineError("Stockfish search returned no complete principal variation");
    }
    return result.lines.front();
}

[[nodiscard]] bool has_legal_moves(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    return !moves.empty();
}

[[nodiscard]] std::size_t legal_move_count(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    return moves.size();
}

void annotate_threat_line(const chess::Board& root, std::span<const chess::Move> moves,
                          Threat& threat) {
    tactics::judge_contributions(root, moves, threat.motifs);
    threat.tactic_labels = tactics::tactic_labels(root, moves);
}

void judge_refutation(engine::Engine& engine, const NodeBudget& budget,
                      std::uint64_t& nodes_searched, const chess::Board& root,
                      std::span<const std::string> pv_uci, const Eval& refuted_move_eval,
                      Threat& refutation) {
    const auto line_moves = walked_moves(root, pv_uci);
    if (line_moves.empty()) {
        return;
    }

    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, root);
    if (legal_moves.empty()) {
        throw engine::EngineError("Stockfish refutation has a move after a terminal position");
    }

    OpportunityJudgement judgement;
    if (legal_moves.size() == 1) {
        judgement.forced = true;
        refutation.opportunity = judgement;
        if (refutation.is_tactic.value_or(false)) {
            annotate_threat_line(root, line_moves, refutation);
        }
        return;
    }

    const auto refutation_first = line_moves.front();
    engine::SearchRequest request;
    request.fen = root.getFen();
    request.nodes = budget.r;
    request.multipv = 1;
    request.searchmoves_uci.reserve(legal_moves.size() - 1);
    for (const auto move : legal_moves) {
        if (move != refutation_first) {
            request.searchmoves_uci.emplace_back(chess::uci::moveToUci(move, root.chess960()));
        }
    }

    const auto searched = engine.search(request);
    nodes_searched += searched.nodes;
    const auto& alternative_line = first_line(searched);
    if (alternative_line.pv_uci.empty()) {
        throw engine::EngineError("Stockfish search R returned an empty principal variation");
    }
    const auto alternative_move = detail::legal_move(root, alternative_line.pv_uci.front());
    if (alternative_move == chess::Move::NO_MOVE || alternative_move == refutation_first) {
        throw engine::EngineError("Stockfish search R returned an invalid alternative move");
    }

    judgement.alternative = BestAlternative{
        .san = chess::uci::moveToSan(root, alternative_move),
        .uci = chess::uci::moveToUci(alternative_move, root.chess960()),
        .eval = eval_for(alternative_line, root.sideToMove(), root.sideToMove()),
    };
    const int margin_units =
        2000 - refuted_move_eval.es_units - judgement.alternative->eval.es_units;
    judgement.margin_units = margin_units;
    judgement.is_opportunity = is_opportunity_margin(margin_units);
    refutation.opportunity = judgement;
    if (refutation.is_tactic.value_or(false) || judgement.is_opportunity) {
        annotate_threat_line(root, line_moves, refutation);
    }
}

[[nodiscard]] bool pseudo_legal_uci(const chess::Board& board, std::string_view uci) {
    if ((uci.size() != 4 && uci.size() != 5) || uci[0] < 'a' || uci[0] > 'h' || uci[1] < '1' ||
        uci[1] > '8' || uci[2] < 'a' || uci[2] > 'h' || uci[3] < '1' || uci[3] > '8') {
        return false;
    }
    const chess::Square from(uci.substr(0, 2));
    const chess::Square to(uci.substr(2, 2));
    const auto moving = board.at(from);
    if (moving == chess::Piece::NONE || moving.color() != board.sideToMove()) {
        return false;
    }
    const auto target = board.at(to);
    if (target != chess::Piece::NONE && target.color() == board.sideToMove()) {
        return false;
    }

    const auto type = moving.type();
    const bool promotion = uci.size() == 5;
    if (promotion && (uci[4] != 'q' && uci[4] != 'r' && uci[4] != 'b' && uci[4] != 'n')) {
        return false;
    }
    if (type != chess::PieceType::PAWN && promotion) {
        return false;
    }

    const int file_delta = static_cast<int>(to.file()) - static_cast<int>(from.file());
    const int rank_delta = static_cast<int>(to.rank()) - static_cast<int>(from.rank());
    const auto side = board.sideToMove();
    if (type == chess::PieceType::PAWN) {
        const int direction = side == chess::Color::WHITE ? 1 : -1;
        const int promotion_rank = side == chess::Color::WHITE ? 7 : 0;
        const bool reaches_last_rank = static_cast<int>(to.rank()) == promotion_rank;
        if (reaches_last_rank != promotion) {
            return false;
        }
        if (file_delta == 0) {
            if (target != chess::Piece::NONE) {
                return false;
            }
            if (rank_delta == direction) {
                return true;
            }
            const int start_rank = side == chess::Color::WHITE ? 1 : 6;
            if (static_cast<int>(from.rank()) != start_rank || rank_delta != 2 * direction) {
                return false;
            }
            const chess::Square between(from.file(),
                                        chess::Rank(static_cast<int>(from.rank()) + direction));
            return board.at(between) == chess::Piece::NONE;
        }
        if ((file_delta != 1 && file_delta != -1) || rank_delta != direction) {
            return false;
        }
        if (target != chess::Piece::NONE) {
            return true;
        }
        return board.enpassantSq() == to;
    }

    if (promotion) {
        return false;
    }
    if (type == chess::PieceType::KNIGHT) {
        return chess::attacks::knight(from).check(to.index());
    }
    if (type == chess::PieceType::BISHOP) {
        return chess::attacks::bishop(from, board.occ()).check(to.index());
    }
    if (type == chess::PieceType::ROOK) {
        return chess::attacks::rook(from, board.occ()).check(to.index());
    }
    if (type == chess::PieceType::QUEEN) {
        return chess::attacks::queen(from, board.occ()).check(to.index());
    }
    if (type == chess::PieceType::KING) {
        if (chess::attacks::king(from).check(to.index())) {
            return true;
        }
        if (rank_delta != 0 || (file_delta != 2 && file_delta != -2) || board.chess960() ||
            board.inCheck()) {
            return false;
        }
        const auto castle_side = file_delta > 0 ? chess::Board::CastlingRights::Side::KING_SIDE
                                                : chess::Board::CastlingRights::Side::QUEEN_SIDE;
        const auto& rights = board.castlingRights();
        if (!rights.has(side, castle_side)) {
            return false;
        }
        const auto rook_file = rights.getRookFile(side, castle_side);
        const chess::Square rook(rook_file, from.rank());
        if (board.at(rook) != chess::Piece(chess::PieceType::ROOK, side)) {
            return false;
        }
        const int step = file_delta > 0 ? 1 : -1;
        for (int file = static_cast<int>(from.file()) + step; file != static_cast<int>(rook_file);
             file += step) {
            if (board.at(chess::Square(chess::File(file), from.rank())) != chess::Piece::NONE) {
                return false;
            }
        }
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<ParryInfo>
search_parry(engine::Engine& engine, const NodeBudget& budget, std::uint64_t& nodes_searched,
             const chess::Board& after_move, bool gives_check, bool mates,
             std::string_view threat_move, const engine::Line& baseline) {
    const auto move = detail::legal_move(after_move, threat_move);
    if (move == chess::Move::NO_MOVE) {
        if (mates || (gives_check && pseudo_legal_uci(after_move, threat_move))) {
            return ParryInfo{.parries = false, .test = ParryTest::DeferredByCheck};
        }
        return ParryInfo{.parries = true, .test = ParryTest::ThreatMoveIllegal};
    }

    engine::SearchRequest request;
    request.fen = after_move.getFen();
    request.nodes = budget.d;
    request.multipv = 1;
    request.searchmoves_uci.emplace_back(threat_move);
    const auto searched = engine.search(request);
    nodes_searched += searched.nodes;
    const auto& line = first_line(searched);
    const bool threat_works = es_units(line) >= es_units(baseline) - kParryMarginUnits;
    return ParryInfo{.parries = !threat_works, .test = ParryTest::Searched};
}

[[nodiscard]] MoveAssessment assess_move(const chess::Board& root, const engine::Line& line,
                                         const engine::Line& best_line, std::string_view uci,
                                         bool in_multipv, bool same_score_as_best,
                                         std::optional<OpportunityJudgement> opportunity,
                                         const std::vector<Motif>& root_motifs,
                                         std::vector<chess::Move>& walked_line) {
    if (line.pv_uci.empty()) {
        throw engine::EngineError("Stockfish search returned an empty principal variation");
    }
    const auto move = detail::legal_move(root, uci);
    if (move == chess::Move::NO_MOVE) {
        throw engine::EngineError("Stockfish returned an illegal principal variation move");
    }

    chess::Board after_move = root;
    after_move.makeMove(move);
    const auto moves = walked_moves(root, line.pv_uci);

    MoveAssessment result;
    result.san = chess::uci::moveToSan(root, move);
    result.uci.assign(uci);
    result.eval = eval_for(line, root.sideToMove(), root.sideToMove());
    result.depth = line.depth;
    result.loss_units = std::max(0, es_units(best_line) - es_units(line));
    result.consequence = consequence(root, line.pv_uci, line);
    result.is_tactic = is_tactic(result.consequence);
    result.allows_tactic = allows_tactic(result.consequence, result.eval);
    result.verdict = verdict(result.loss_units, same_score_as_best, result.allows_tactic);
    result.in_multipv = in_multipv;
    result.gives_check = after_move.inCheck();
    result.line_san = san_line(root, line.pv_uci);
    result.walked_plies = static_cast<int>(moves.size());
    result.motifs = tactics::line_motifs(root, moves);
    result.positional = positional::line_changes(root, moves);
    result.detailed = false;
    result.opportunity = std::move(opportunity);

    const bool tactic_line =
        result.is_tactic || (result.opportunity && result.opportunity->is_opportunity);
    if (tactic_line) {
        tactics::judge_contributions(root, moves, result.motifs);
        result.tactic_labels = tactics::tactic_labels(root, moves);
        result.root_motifs = root_motifs;
        tactics::judge_contributions(root, moves, result.root_motifs);
    }

    if (result.verdict != Verdict::Best && result.verdict != Verdict::Okay) {
        const auto pv = std::span<const std::string>(line.pv_uci);
        const auto reply = pv.size() > 1 ? pv.subspan(1) : std::span<const std::string>();
        engine::Line reply_line;
        if (line.mate.has_value()) {
            reply_line.mate = -*line.mate;
        }
        result.refutation =
            make_threat(after_move, reply, reply_line, report_side(after_move.sideToMove()));
        result.refutation->is_tactic = result.allows_tactic;
        if (result.allows_tactic) {
            const auto refutation_moves = walked_moves(after_move, reply);
            annotate_threat_line(after_move, refutation_moves, *result.refutation);
        }
    }
    walked_line = std::move(moves);
    return result;
}

void complete_move_details(engine::Engine& engine, const NodeBudget& budget,
                           std::uint64_t& nodes_searched, const chess::Board& root,
                           const engine::Line& line, MoveAssessment& result,
                           int opponent_threat_rank,
                           const std::optional<engine::Line>& opponent_line) {
    result.detailed = true;
    const auto move = detail::legal_move(root, result.uci);
    if (move == chess::Move::NO_MOVE) {
        throw engine::EngineError("Stockfish returned an illegal principal variation move");
    }
    chess::Board after_move = root;
    after_move.makeMove(move);
    const bool has_reply = has_legal_moves(after_move);
    const bool mates = result.gives_check && !has_reply;

    if (!root.inCheck() && opponent_threat_rank >= 2 && opponent_line.has_value() &&
        !opponent_line->pv_uci.empty()) {
        result.parries_threat =
            search_parry(engine, budget, nodes_searched, after_move, result.gives_check, mates,
                         opponent_line->pv_uci.front(), *opponent_line);
    }

    if (result.gives_check) {
        result.creates_threat_skipped = CreatesThreatSkip::GivesCheck;
    } else if (!has_reply) {
        result.creates_threat_skipped = CreatesThreatSkip::GameOver;
    } else {
        chess::Board null_after_move = after_move;
        null_after_move.makeNullMove();
        if (!has_legal_moves(null_after_move)) {
            result.creates_threat_skipped = CreatesThreatSkip::NoLegalMove;
        } else {
            engine::SearchRequest request;
            request.fen = null_after_move.getFen();
            request.nodes = budget.e;
            request.multipv = 1;
            const auto searched = engine.search(request);
            nodes_searched += searched.nodes;
            const auto& created_line = first_line(searched);
            result.creates_threat = make_threat(null_after_move, created_line.pv_uci, created_line,
                                                report_side(null_after_move.sideToMove()));
            result.tempo_value =
                make_tempo_value(report_side(after_move.sideToMove()), 2000 - result.eval.es_units,
                                 2000 - es_units(created_line));
        }
    }

    if (result.refutation.has_value()) {
        const auto pv = std::span<const std::string>(line.pv_uci);
        const auto reply = pv.size() > 1 ? pv.subspan(1) : std::span<const std::string>();
        judge_refutation(engine, budget, nodes_searched, after_move, reply, result.eval,
                         *result.refutation);
    }
}

void complete_tempo_ledger(engine::Engine& engine, const NodeBudget& budget,
                           std::uint64_t& nodes_searched, const chess::Board& root,
                           std::span<const chess::Move> walked_line, MoveAssessment& result,
                           std::optional<bool> null_move_reliable) {
    result.tempo_ledger.reset();
    if (root.inCheck()) {
        result.tempo_ledger_skipped = TempoLedgerSkip::InCheck;
        return;
    }
    if (null_move_reliable && !*null_move_reliable) {
        result.tempo_ledger_skipped = TempoLedgerSkip::NullMoveUnreliable;
        return;
    }
    if (!result.detailed) {
        result.tempo_ledger_skipped = TempoLedgerSkip::NotDetailed;
        return;
    }

    TempoLedger ledger;
    ledger.plies.reserve(walked_line.size());
    chess::Board board = root;
    for (std::size_t index = 0; index < walked_line.size(); ++index) {
        board.makeMove(walked_line[index]);
        const bool game_over = !has_legal_moves(board);
        TempoPly ply;
        ply.ply = static_cast<int>(index + 1);
        ply.san = result.line_san[index];
        ply.side = report_side(~board.sideToMove());

        if (board.inCheck()) {
            ply.threatening = true;
            ply.by_check = true;
        } else if (index == 0) {
            if (result.creates_threat) {
                const int rank = consequence_rank(result.creates_threat->consequence);
                ply.threat_rank = rank;
                ply.threatening = rank >= 2;
            }
        } else if (!game_over) {
            chess::Board null_board = board;
            null_board.makeNullMove();
            if (has_legal_moves(null_board)) {
                engine::SearchRequest request;
                request.fen = null_board.getFen();
                request.nodes = budget.l;
                request.multipv = 1;
                const auto searched = engine.search(request);
                nodes_searched += searched.nodes;
                const auto& threat_line = first_line(searched);
                const int rank =
                    consequence_rank(consequence(null_board, threat_line.pv_uci, threat_line));
                ply.threat_rank = rank;
                ply.threatening = rank >= 2;
            }
        }

        if (!ledger.plies.empty() && ledger.plies.back().threatening) {
            if (game_over) {
                ply.stops_threat = true;
            } else {
                engine::SearchRequest request;
                request.fen = board.getFen();
                request.nodes = budget.l;
                request.multipv = 1;
                const auto searched = engine.search(request);
                nodes_searched += searched.nodes;
                const auto& reply_line = first_line(searched);
                const int rank =
                    consequence_rank(consequence(board, reply_line.pv_uci, reply_line));
                ply.stops_threat = rank < 2;
            }
        }
        ledger.plies.push_back(std::move(ply));
        if (game_over) {
            break;
        }
    }
    label_ledger(ledger.plies);
    for (const TempoPly& ply : ledger.plies) {
        if (!ply.tempo_gained) {
            continue;
        }
        if (ply.side == Side::White) {
            ++ledger.tempi.white;
        } else {
            ++ledger.tempi.black;
        }
    }
    ledger.runs = initiative_runs(ledger.plies, root, walked_line.first(ledger.plies.size()),
                                  result.positional);
    result.tempo_ledger = std::move(ledger);
    result.tempo_ledger_skipped.reset();
}
[[nodiscard]] bool selected_by_selection(const Selection& selection, const MoveAssessment& move,
                                         std::size_t line_index) {
    bool selected =
        selection.top.has_value() && line_index < static_cast<std::size_t>(*selection.top);
    for (const auto keyword : selection.keywords) {
        switch (keyword) {
        case Selection::Keyword::Best:
            selected = selected || line_index == 0;
            break;
        case Selection::Keyword::Playable:
            selected = selected || move.verdict == Verdict::Best || move.verdict == Verdict::Okay;
            break;
        case Selection::Keyword::Inaccuracies:
            selected = selected || move.verdict == Verdict::Inaccuracy;
            break;
        case Selection::Keyword::Mistakes:
            selected = selected || move.verdict == Verdict::Mistake;
            break;
        case Selection::Keyword::Blunders:
            selected = selected || move.verdict == Verdict::Blunder;
            break;
        case Selection::Keyword::Unplayable:
            selected = selected || move.verdict == Verdict::Inaccuracy ||
                       move.verdict == Verdict::Mistake || move.verdict == Verdict::Blunder;
            break;
        case Selection::Keyword::All:
            selected = true;
            break;
        }
    }
    return selected || std::ranges::find(selection.moves, move.uci) != selection.moves.end();
}

} // namespace

void analyse(engine::Engine& engine, const Request& request, Report& report) {
    const auto& board = request.board;
    const auto side_to_move = board.sideToMove();
    const auto opponent = ~side_to_move;
    const bool in_check = board.inCheck();

    report.input.selection = request.selection;
    report.engine.nodes = node_budget(request.budget);
    report.engine.nodes_searched = 0;
    report.position.fen = board.getFen();
    report.position.side_to_move = report_side(side_to_move);
    report.position.in_check = in_check;
    report.position.terminal.reset();
    report.position.tempo_value.reset();
    report.position.eval_white.reset();
    report.position.motifs.clear();
    report.position.positional = positional::features(board);
    report.threats.reset();
    report.candidates.clear();
    report.asked_moves.clear();
    report.playable_truncated = false;

    const auto root_legal_moves = legal_move_count(board);
    report.position.legal_moves = static_cast<int>(root_legal_moves);
    const int lines = multipv_lines(request.selection, report.position.legal_moves);
    report.engine.multipv = lines;
    report.engine.nodes.a = search_a_nodes(request.budget, lines);
    if (root_legal_moves == 0) {
        report.position.terminal = in_check ? Terminal::Checkmate : Terminal::Stalemate;
        return;
    }

    report.position.motifs = tactics::position_motifs(board);

    const auto& budget = report.engine.nodes;
    engine::SearchRequest a_request;
    a_request.fen = board.getFen();
    a_request.nodes = budget.a;
    a_request.multipv = lines;
    const auto a_result = engine.search(a_request);
    report.engine.nodes_searched += a_result.nodes;
    if (a_result.lines.empty()) {
        throw engine::EngineError("Stockfish search A returned no complete principal variation");
    }
    if (root_legal_moves >= 2 && a_result.lines.size() < 2) {
        throw engine::EngineError(
            "Stockfish search A returned fewer than two principal variations");
    }
    const auto& best_line = a_result.lines.front();
    const Eval best_eval = eval_for(best_line, side_to_move, side_to_move);
    report.position.eval_white = eval_for(best_line, side_to_move, chess::Color::WHITE);

    std::optional<OpportunityJudgement> root_opportunity;
    OpportunityJudgement root_judgement;
    if (root_legal_moves == 1) {
        root_judgement.forced = true;
    } else {
        const auto& alternative_line = a_result.lines[1];
        if (alternative_line.pv_uci.empty()) {
            throw engine::EngineError("Stockfish MultiPV line has an empty principal variation");
        }
        const auto alternative_move = detail::legal_move(board, alternative_line.pv_uci.front());
        if (alternative_move == chess::Move::NO_MOVE) {
            throw engine::EngineError("Stockfish returned an illegal principal variation move");
        }
        root_judgement.alternative = BestAlternative{
            .san = chess::uci::moveToSan(board, alternative_move),
            .uci = chess::uci::moveToUci(alternative_move, board.chess960()),
            .eval = eval_for(alternative_line, side_to_move, side_to_move),
        };
        const int margin_units = best_eval.es_units - root_judgement.alternative->eval.es_units;
        root_judgement.margin_units = margin_units;
        root_judgement.is_opportunity = is_opportunity_margin(margin_units);
    }
    root_opportunity = std::move(root_judgement);

    std::optional<engine::Line> opponent_line;
    std::optional<Threat> opponent_threat;
    if (!in_check) {
        chess::Board null_board = board;
        null_board.makeNullMove();
        if (has_legal_moves(null_board)) {
            engine::SearchRequest b_request;
            b_request.fen = null_board.getFen();
            b_request.nodes = budget.b;
            b_request.multipv = 1;
            auto b_result = engine.search(b_request);
            report.engine.nodes_searched += b_result.nodes;
            if (!b_result.lines.empty()) {
                opponent_line.emplace(std::move(b_result.lines.front()));
                opponent_threat = make_threat(null_board, opponent_line->pv_uci, *opponent_line,
                                              report_side(opponent));
            }
        }
    }
    if (opponent_line) {
        report.position.tempo_value = make_tempo_value(
            report_side(side_to_move), es_units(best_line), 2000 - es_units(*opponent_line));
    }
    Threat own_threat = make_threat(board, best_line.pv_uci, best_line, report_side(side_to_move));
    const int own_rank = consequence_rank(own_threat.consequence);
    const int opponent_rank =
        opponent_threat.has_value() ? consequence_rank(opponent_threat->consequence) : 0;
    Threats threats;
    if (side_to_move == chess::Color::WHITE) {
        threats.white = std::move(own_threat);
        threats.black = std::move(opponent_threat);
    } else {
        threats.black = std::move(own_threat);
        threats.white = std::move(opponent_threat);
    }
    threats.null_move_reliable =
        in_check ? std::optional<bool>() : std::optional<bool>(!null_move_prone(board));
    const bool root_tempo_dependent = tempo_dependent(report.position.tempo_value);

    report.candidates.reserve(a_result.lines.size());
    for (std::size_t index = 0; index < a_result.lines.size(); ++index) {
        const auto& line = a_result.lines[index];
        if (line.pv_uci.empty()) {
            throw engine::EngineError("Stockfish MultiPV line has an empty principal variation");
        }
        const Eval line_eval = eval_for(line, side_to_move, side_to_move);
        const bool same_score_as_best = index == 0 || same_score(line_eval, best_eval);
        std::vector<chess::Move> walked_line;
        auto candidate = assess_move(
            board, line, best_line, line.pv_uci.front(), true, same_score_as_best,
            index == 0 ? root_opportunity : std::nullopt, report.position.motifs, walked_line);
        const bool selected = selected_by_selection(request.selection, candidate, index);
        if (is_detailed_line(static_cast<int>(index + 1), selected)) {
            complete_move_details(engine, budget, report.engine.nodes_searched, board, line,
                                  candidate, opponent_rank, opponent_line);
        }
        positional::mark_use(candidate.positional, board, walked_line, root_tempo_dependent);
        complete_tempo_ledger(engine, budget, report.engine.nodes_searched, board, walked_line,
                              candidate, threats.null_move_reliable);
        report.candidates.push_back(std::move(candidate));
    }

    report.playable_truncated =
        a_result.lines.size() == static_cast<std::size_t>(lines) &&
        lines < report.position.legal_moves &&
        std::ranges::all_of(
            report.candidates,
            [](const MoveAssessment& move) {
                return move.verdict == Verdict::Best || move.verdict == Verdict::Okay;
            });

    if (in_check) {
        threats.urgency =
            Urgency{.more_urgent = report_side(opponent), .rule = UrgencyRule::InCheck};
    } else if (own_rank < 2 && opponent_rank < 2) {
        threats.urgency = Urgency{.more_urgent = std::nullopt, .rule = UrgencyRule::Quiet};
    } else if (own_rank != opponent_rank) {
        const auto urgent =
            own_rank > opponent_rank ? report_side(side_to_move) : report_side(opponent);
        threats.urgency = Urgency{.more_urgent = urgent, .rule = UrgencyRule::ConsequenceClass};
    } else {
        const auto& best_move = report.candidates.front();
        if (!best_move.parries_threat.has_value()) {
            throw engine::EngineError("Threat urgency requires the best move's parry search");
        }
        const bool parries = best_move.parries_threat->parries;
        threats.urgency =
            Urgency{.more_urgent = parries ? report_side(opponent) : report_side(side_to_move),
                    .rule = parries ? UrgencyRule::Parry : UrgencyRule::Counterattack};
    }
    report.threats = std::move(threats);

    for (const auto asked_move : request.asked) {
        const auto asked_uci = chess::uci::moveToUci(asked_move, board.chess960());
        const auto found =
            std::ranges::find_if(report.candidates, [&asked_uci](const MoveAssessment& candidate) {
                return candidate.uci == asked_uci;
            });
        if (found != report.candidates.end()) {
            continue;
        }
        const bool already_assessed =
            std::ranges::any_of(report.asked_moves, [&asked_uci](const MoveAssessment& candidate) {
                return candidate.uci == asked_uci;
            });
        if (already_assessed) {
            continue;
        }

        engine::SearchRequest c_request;
        c_request.fen = board.getFen();
        c_request.nodes = budget.c;
        c_request.multipv = 1;
        c_request.searchmoves_uci.push_back(asked_uci);
        const auto c_result = engine.search(c_request);
        report.engine.nodes_searched += c_result.nodes;
        const auto& line = first_line(c_result);
        if (line.pv_uci.empty() || line.pv_uci.front() != asked_uci) {
            throw engine::EngineError("Stockfish search C did not return the requested move first");
        }
        std::vector<chess::Move> walked_line;
        auto assessment = assess_move(board, line, best_line, asked_uci, false, false, std::nullopt,
                                      report.position.motifs, walked_line);
        complete_move_details(engine, budget, report.engine.nodes_searched, board, line, assessment,
                              opponent_rank, opponent_line);
        positional::mark_use(assessment.positional, board, walked_line, root_tempo_dependent);
        complete_tempo_ledger(engine, budget, report.engine.nodes_searched, board, walked_line,
                              assessment, threats.null_move_reliable);
        report.asked_moves.push_back(std::move(assessment));
    }
}

} // namespace chesswhy::analysis
