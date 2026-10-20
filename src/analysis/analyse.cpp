#include "chesswhy/analysis.hpp"
#include "chesswhy/tactics.hpp"

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

[[nodiscard]] MoveAssessment assess_move(engine::Engine& engine, const NodeBudget& budget,
                                         std::uint64_t& nodes_searched, const chess::Board& root,
                                         const engine::Line& line, const engine::Line& best_line,
                                         std::string_view uci, bool in_multipv,
                                         int opponent_threat_rank,
                                         const std::optional<engine::Line>& opponent_line) {
    if (line.pv_uci.empty()) {
        throw engine::EngineError("Stockfish search returned an empty principal variation");
    }
    const auto move = detail::legal_move(root, uci);
    if (move == chess::Move::NO_MOVE) {
        throw engine::EngineError("Stockfish returned an illegal principal variation move");
    }

    chess::Board after_move = root;
    after_move.makeMove(move);
    const bool gives_check = after_move.inCheck();
    const bool has_reply = has_legal_moves(after_move);
    const bool mates = gives_check && !has_reply;

    std::optional<ParryInfo> parry;
    if (opponent_threat_rank >= 2 && opponent_line.has_value() && !opponent_line->pv_uci.empty()) {
        parry = search_parry(engine, budget, nodes_searched, after_move, gives_check, mates,
                             opponent_line->pv_uci.front(), *opponent_line);
    }

    std::optional<Threat> creates_threat;
    if (!gives_check && has_reply) {
        chess::Board null_after_move = after_move;
        null_after_move.makeNullMove();
        if (has_legal_moves(null_after_move)) {
            engine::SearchRequest request;
            request.fen = null_after_move.getFen();
            request.nodes = budget.e;
            request.multipv = 1;
            const auto searched = engine.search(request);
            nodes_searched += searched.nodes;
            const auto& line = first_line(searched);
            creates_threat = make_threat(null_after_move, line.pv_uci, line,
                                         report_side(null_after_move.sideToMove()));
        }
    }

    MoveAssessment result;
    result.san = chess::uci::moveToSan(root, move);
    result.uci.assign(uci);
    result.eval = eval_for(line, root.sideToMove(), root.sideToMove());
    result.loss_units = std::max(0, es_units(best_line) - es_units(line));
    result.verdict = verdict_for_loss(result.loss_units);
    result.in_multipv = in_multipv;
    result.gives_check = gives_check;
    result.line_san = san_line(root, line.pv_uci);
    const auto moves = walked_moves(root, line.pv_uci);
    result.motifs = tactics::line_motifs(root, std::span<const chess::Move>(moves));
    result.parries_threat = parry;
    result.creates_threat = std::move(creates_threat);

    if (result.verdict != Verdict::Fine) {
        const auto pv = std::span<const std::string>(line.pv_uci);
        const auto reply = pv.size() > 1 ? pv.subspan(1) : std::span<const std::string>();
        engine::Line reply_line;
        if (line.mate.has_value()) {
            reply_line.mate = -*line.mate;
        }
        result.refutation =
            make_threat(after_move, reply, reply_line, report_side(after_move.sideToMove()));
    }
    return result;
}

} // namespace

void analyse(engine::Engine& engine, const Request& request, Report& report) {
    const auto& board = request.board;
    const auto side_to_move = board.sideToMove();
    const auto opponent = ~side_to_move;
    const bool in_check = board.inCheck();

    report.engine.multipv = kMultiPv;
    report.engine.nodes = node_budget(request.budget);
    report.engine.nodes_searched = 0;
    report.position.fen = board.getFen();
    report.position.side_to_move = report_side(side_to_move);
    report.position.in_check = in_check;
    report.position.terminal.reset();
    report.position.eval_white.reset();
    report.position.motifs.clear();
    report.threats.reset();
    report.candidates.clear();
    report.fine_truncated = false;
    report.asked_move.reset();

    if (!has_legal_moves(board)) {
        report.position.terminal = in_check ? Terminal::Checkmate : Terminal::Stalemate;
        return;
    }

    report.position.motifs = tactics::position_motifs(board);

    const auto& budget = report.engine.nodes;
    engine::SearchRequest a_request;
    a_request.fen = board.getFen();
    a_request.nodes = budget.a;
    a_request.multipv = kMultiPv;
    const auto a_result = engine.search(a_request);
    report.engine.nodes_searched += a_result.nodes;
    if (a_result.lines.empty()) {
        throw engine::EngineError("Stockfish search A returned no complete principal variation");
    }
    const auto& best_line = a_result.lines.front();
    report.position.eval_white = eval_for(best_line, side_to_move, chess::Color::WHITE);

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
            if (b_result.lines.empty()) {
                throw engine::EngineError(
                    "Stockfish search B returned no complete principal variation");
            }
            opponent_line.emplace(std::move(b_result.lines.front()));
            opponent_threat = make_threat(null_board, opponent_line->pv_uci, *opponent_line,
                                          report_side(opponent));
        }
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

    report.candidates.reserve(a_result.lines.size());
    for (const auto& line : a_result.lines) {
        if (line.pv_uci.empty()) {
            throw engine::EngineError("Stockfish MultiPV line has an empty principal variation");
        }
        report.candidates.push_back(assess_move(engine, budget, report.engine.nodes_searched, board,
                                                line, best_line, line.pv_uci.front(), true,
                                                opponent_rank, opponent_line));
    }

    report.fine_truncated = a_result.lines.size() == static_cast<std::size_t>(kMultiPv) &&
                            std::ranges::all_of(report.candidates, [](const MoveAssessment& move) {
                                return move.verdict == Verdict::Fine;
                            });

    if (in_check) {
        threats.urgency =
            Urgency{.more_urgent = report_side(opponent), .rule = UrgencyRule::InCheck};
    } else {
        if (own_rank < 2 && opponent_rank < 2) {
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
    }
    report.threats = std::move(threats);

    if (request.asked.has_value()) {
        const auto asked_uci = chess::uci::moveToUci(*request.asked, board.chess960());
        const auto found =
            std::ranges::find_if(report.candidates, [&asked_uci](const MoveAssessment& move) {
                return move.uci == asked_uci;
            });
        if (found != report.candidates.end()) {
            report.asked_move = *found;
        } else {
            engine::SearchRequest c_request;
            c_request.fen = board.getFen();
            c_request.nodes = budget.c;
            c_request.multipv = 1;
            c_request.searchmoves_uci.push_back(asked_uci);
            const auto c_result = engine.search(c_request);
            report.engine.nodes_searched += c_result.nodes;
            const auto& line = first_line(c_result);
            if (line.pv_uci.empty() || line.pv_uci.front() != asked_uci) {
                throw engine::EngineError(
                    "Stockfish search C did not return the requested move first");
            }
            report.asked_move =
                assess_move(engine, budget, report.engine.nodes_searched, board, line, best_line,
                            asked_uci, false, opponent_rank, opponent_line);
        }
    }
}

} // namespace chesswhy::analysis
