#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "move_set_internal.hpp"
#include "see_internal.hpp"

namespace chesswhy::tactics {
namespace {

[[nodiscard]] Side report_side(chess::Color color) {
    return color == chess::Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] bool is_move_motif(MotifTag tag) {
    switch (tag) {
    case MotifTag::Fork:
    case MotifTag::Skewer:
    case MotifTag::DiscoveredCheck:
    case MotifTag::DiscoveredAttack:
    case MotifTag::DoubleCheck:
    case MotifTag::BackRankMate:
    case MotifTag::Promotion:
        return true;
    case MotifTag::HangingPiece:
    case MotifTag::PinAbsolute:
    case MotifTag::PinRelative:
    case MotifTag::BackRankWeakness:
    case MotifTag::OverloadedDefender:
    case MotifTag::TrappedPiece:
    case MotifTag::PinTactical:
        return false;
    }
    return false;
}

[[nodiscard]] bool attacks_square(const chess::Board& board, chess::Square origin,
                                  chess::Square destination) {
    const chess::Piece piece = board.at(origin);
    switch (piece.type().internal()) {
    case chess::PieceType::PAWN:
        return chess::attacks::pawn(piece.color(), origin).check(destination.index());
    case chess::PieceType::KNIGHT:
        return chess::attacks::knight(origin).check(destination.index());
    case chess::PieceType::BISHOP:
        return chess::attacks::bishop(origin, board.occ()).check(destination.index());
    case chess::PieceType::ROOK:
        return chess::attacks::rook(origin, board.occ()).check(destination.index());
    case chess::PieceType::QUEEN:
        return chess::attacks::queen(origin, board.occ()).check(destination.index());
    case chess::PieceType::KING:
        return chess::attacks::king(origin).check(destination.index());
    default:
        return false;
    }
}

struct TrackedPiece {
    chess::Square square = chess::Square::NO_SQ;
    chess::Color color = chess::Color::NONE;
    bool present = false;
};

using TrackedPieces = std::array<TrackedPiece, 64>;

void add_tracked_piece(const chess::Board& board, std::uint8_t square_index, TrackedPieces& pieces,
                       std::size_t& count) {
    const chess::Square square(square_index);
    for (std::size_t index = 0; index < count; ++index) {
        if (pieces[index].square == square) {
            return;
        }
    }
    const chess::Piece piece = board.at(square);
    if (piece != chess::Piece::NONE && count < pieces.size()) {
        pieces[count++] = TrackedPiece{.square = square, .color = piece.color(), .present = true};
    }
}

[[nodiscard]] std::size_t tracked_piece_at(const TrackedPieces& pieces, std::size_t count,
                                           chess::Square square) {
    for (std::size_t index = 0; index < count; ++index) {
        if (pieces[index].present && pieces[index].square == square) {
            return index;
        }
    }
    return count;
}

[[nodiscard]] chess::Square captured_square(const chess::Move& move) {
    return move.typeOf() == chess::Move::ENPASSANT ? move.to().ep_square() : move.to();
}

[[nodiscard]] char piece_letter(const chess::Board& board, chess::Square square) {
    return static_cast<std::string>(board.at(square)).front();
}

[[nodiscard]] PieceOnSquare report_piece(const chess::Board& board, chess::Square square) {
    return PieceOnSquare{.square = static_cast<std::uint8_t>(square.index()),
                         .piece = piece_letter(board, square)};
}

[[nodiscard]] std::vector<chess::Board> positions_after(const chess::Board& root,
                                                        std::span<const chess::Move> line) {
    std::vector<chess::Board> positions;
    positions.reserve(line.size() + 1);
    positions.push_back(root);
    for (const chess::Move move : line) {
        chess::Board next = positions.back();
        next.makeMove(move);
        positions.push_back(std::move(next));
    }
    return positions;
}

class EditableBoard : public chess::Board {
public:
    explicit EditableBoard(const chess::Board& board) : chess::Board(board) {}

    void erase(chess::Square square) {
        const chess::Piece piece = at(square);
        if (piece == chess::Piece::NONE) {
            return;
        }
        removePiece(piece, square);

        bool metadata_changed = false;
        if (piece.type() == chess::PieceType::ROOK) {
            for (const chess::Color color : {chess::Color::WHITE, chess::Color::BLACK}) {
                const int back_rank = color == chess::Color::WHITE ? 0 : 7;
                if (static_cast<int>(square.rank()) != back_rank) {
                    continue;
                }
                for (const chess::Board::CastlingRights::Side castle_side :
                     {chess::Board::CastlingRights::Side::KING_SIDE,
                      chess::Board::CastlingRights::Side::QUEEN_SIDE}) {
                    if (cr_.has(color, castle_side) &&
                        square.file() == cr_.getRookFile(color, castle_side)) {
                        cr_.clear(color, castle_side);
                        metadata_changed = true;
                    }
                }
            }
        }
        if (ep_sq_ != chess::Square::NO_SQ) {
            const int victim_offset = sideToMove() == chess::Color::WHITE ? -8 : 8;
            if (square.index() == ep_sq_.index() + victim_offset) {
                ep_sq_ = chess::Square::NO_SQ;
                metadata_changed = true;
            }
        }
        if (metadata_changed) {
            const auto fen = getFen();
            setFen(fen);
        }
    }
};

[[nodiscard]] chess::Color color_for(Side side) {
    return side == Side::White ? chess::Color::WHITE : chess::Color::BLACK;
}

[[nodiscard]] chess::Square move_destination(chess::Move move, chess::Color side) {
    if (move.typeOf() == chess::Move::CASTLING) {
        return chess::Square::castling_king_square(move.to() > move.from(), side);
    }
    return move.to();
}

struct UciKey {
    std::array<char, 5> characters{};
    std::size_t size = 4;
};

[[nodiscard]] char promotion_character(chess::PieceType type) {
    switch (type.internal()) {
    case chess::PieceType::QUEEN:
        return 'q';
    case chess::PieceType::ROOK:
        return 'r';
    case chess::PieceType::BISHOP:
        return 'b';
    case chess::PieceType::KNIGHT:
        return 'n';
    default:
        return ' ';
    }
}

[[nodiscard]] UciKey uci_key(chess::Move move, bool chess960) {
    const chess::Square from = move.from();
    chess::Square to = move.to();
    if (!chess960 && move.typeOf() == chess::Move::CASTLING) {
        to = chess::Square(chess::File(move.to() > from ? 6 : 2), from.rank());
    }
    UciKey key;
    key.characters[0] = static_cast<char>('a' + static_cast<int>(from.file()));
    key.characters[1] = static_cast<char>('1' + static_cast<int>(from.rank()));
    key.characters[2] = static_cast<char>('a' + static_cast<int>(to.file()));
    key.characters[3] = static_cast<char>('1' + static_cast<int>(to.rank()));
    if (move.typeOf() == chess::Move::PROMOTION) {
        key.characters[4] = promotion_character(move.promotionType());
        key.size = 5;
    }
    return key;
}

[[nodiscard]] bool uci_key_less(const UciKey& left, const UciKey& right) {
    return std::lexicographical_compare(left.characters.begin(),
                                        left.characters.begin() +
                                            static_cast<std::ptrdiff_t>(left.size),
                                        right.characters.begin(),
                                        right.characters.begin() +
                                            static_cast<std::ptrdiff_t>(right.size));
}

void sort_moves_by_uci(const chess::Board& board, std::vector<chess::Move>& moves) {
    const bool chess960 = board.chess960();
    std::sort(moves.begin(), moves.end(), [chess960](chess::Move left, chess::Move right) {
        return uci_key_less(uci_key(left, chess960), uci_key(right, chess960));
    });
}

[[nodiscard]] bool square_on_segment(chess::Square first, chess::Square last,
                                     chess::Square square) {
    const int file_delta = static_cast<int>(last.file()) - static_cast<int>(first.file());
    const int rank_delta = static_cast<int>(last.rank()) - static_cast<int>(first.rank());
    const int abs_file_delta = file_delta < 0 ? -file_delta : file_delta;
    const int abs_rank_delta = rank_delta < 0 ? -rank_delta : rank_delta;
    if (file_delta != 0 && rank_delta != 0 && abs_file_delta != abs_rank_delta) {
        return false;
    }
    const int steps = std::max(abs_file_delta, abs_rank_delta);
    const int file_step = (file_delta > 0) - (file_delta < 0);
    const int rank_step = (rank_delta > 0) - (rank_delta < 0);
    for (int step = 0; step <= steps; ++step) {
        if (square.file() == static_cast<chess::File>(static_cast<int>(first.file()) +
                                                      step * file_step) &&
            square.rank() == static_cast<chess::Rank>(static_cast<int>(first.rank()) +
                                                      step * rank_step)) {
            return true;
        }
    }
    return false;
}

void append_pawn_move(chess::Square from, chess::Square to, bool promotes,
                      std::vector<chess::Move>& moves) {
    if (promotes) {
        constexpr std::array promotions{chess::PieceType::QUEEN, chess::PieceType::ROOK,
                                        chess::PieceType::BISHOP, chess::PieceType::KNIGHT};
        for (const chess::PieceType promotion : promotions) {
            moves.push_back(chess::Move::make<chess::Move::PROMOTION>(from, to, promotion));
        }
    } else {
        moves.push_back(chess::Move::make<chess::Move::NORMAL>(from, to));
    }
}

[[nodiscard]] std::vector<chess::Move>
pseudo_legal_moves_from(const chess::Board& board, chess::Square from) {
    std::vector<chess::Move> moves;
    const chess::Piece piece = board.at(from);
    if (piece == chess::Piece::NONE) {
        return moves;
    }
    const chess::Color side = piece.color();
    if (piece.type() != chess::PieceType::PAWN) {
        for (int index = 0; index < 64; ++index) {
            const chess::Square to(index);
            const chess::Piece target = board.at(to);
            if (to == from || target.color() == side ||
                target.type() == chess::PieceType::KING ||
                !attacks_square(board, from, to)) {
                continue;
            }
            moves.push_back(chess::Move::make<chess::Move::NORMAL>(from, to));
        }
        return moves;
    }

    const int file = static_cast<int>(from.file());
    const int rank = static_cast<int>(from.rank());
    const int rank_step = side == chess::Color::WHITE ? 1 : -1;
    const int promotion_rank = side == chess::Color::WHITE ? 7 : 0;
    const int next_rank = rank + rank_step;
    if (next_rank >= 0 && next_rank < 8) {
        const chess::Square forward{chess::File(file), chess::Rank(next_rank)};
        if (board.at(forward) == chess::Piece::NONE) {
            append_pawn_move(from, forward, next_rank == promotion_rank, moves);
            const int start_rank = side == chess::Color::WHITE ? 1 : 6;
            const int double_rank = rank + 2 * rank_step;
            const chess::Square double_forward{chess::File(file), chess::Rank(double_rank)};
            if (rank == start_rank && board.at(double_forward) == chess::Piece::NONE) {
                moves.push_back(
                    chess::Move::make<chess::Move::NORMAL>(from, double_forward));
            }
        }
        for (const int file_step : {-1, 1}) {
            const int target_file = file + file_step;
            if (target_file < 0 || target_file >= 8) {
                continue;
            }
            const chess::Square to{chess::File(target_file), chess::Rank(next_rank)};
            const chess::Piece target = board.at(to);
            if (target != chess::Piece::NONE && target.color() != side &&
                target.type() != chess::PieceType::KING) {
                append_pawn_move(from, to, next_rank == promotion_rank, moves);
            } else if (to == board.enpassantSq()) {
                moves.push_back(chess::Move::make<chess::Move::ENPASSANT>(from, to));
            }
        }
    }
    return moves;
}

[[nodiscard]] std::vector<chess::Move> legal_moves_from(const chess::Board& board,
                                                         chess::Square from) {
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board);
    std::vector<chess::Move> moves;
    for (const chess::Move move : legal_moves) {
        if (move.from() == from) {
            moves.push_back(move);
        }
    }
    return moves;
}

[[nodiscard]] bool pin_tactical_threat_enabled(const chess::Board& board, const Motif& motif) {
    if (!motif.threat) {
        return false;
    }
    const chess::Color side = color_for(motif.side);
    const chess::Square actor(motif.actor.square);
    const chess::Square destination(motif.threat->square);
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board);
    for (const chess::Move move : legal_moves) {
        if (move.from() != actor || move_destination(move, side) != destination) {
            continue;
        }
        switch (motif.threat->kind) {
        case ThreatKind::Checkmate: {
            chess::Board after = board;
            after.makeMove(move);
            if (after.isAttacked(after.kingSq(~side), side) &&
                !chess::movegen::anylegalmoves(after)) {
                return true;
            }
            break;
        }
        case ThreatKind::Capture:
            if (board.isCapture(move) && captured_square(move) == destination &&
                see(board, destination, side) >= 1) {
                return true;
            }
            break;
        case ThreatKind::Fork:
        case ThreatKind::Skewer:
        case ThreatKind::DiscoveredCheck:
        case ThreatKind::DiscoveredAttack:
        case ThreatKind::DoubleCheck:
        case ThreatKind::BackRankMate:
        case ThreatKind::Promotion: {
            const MotifTag expected_tag = [&] {
                switch (motif.threat->kind) {
                case ThreatKind::Fork:
                    return MotifTag::Fork;
                case ThreatKind::Skewer:
                    return MotifTag::Skewer;
                case ThreatKind::DiscoveredCheck:
                    return MotifTag::DiscoveredCheck;
                case ThreatKind::DiscoveredAttack:
                    return MotifTag::DiscoveredAttack;
                case ThreatKind::DoubleCheck:
                    return MotifTag::DoubleCheck;
                case ThreatKind::BackRankMate:
                    return MotifTag::BackRankMate;
                case ThreatKind::Promotion:
                    return MotifTag::Promotion;
                case ThreatKind::Checkmate:
                case ThreatKind::Capture:
                    return MotifTag::HangingPiece;
                }
                return MotifTag::HangingPiece;
            }();
            const std::array line{move};
            const auto realized = line_motifs(board, std::span<const chess::Move>(line),
                                              MotifOptions{.pin_tactical = false});
            if (std::ranges::any_of(realized, [expected_tag, side](const Motif& candidate) {
                    return candidate.ply == 1 && candidate.tag == expected_tag &&
                           candidate.side == report_side(side);
                })) {
                return true;
            }
            break;
        }
        }
    }
    return false;
}

[[nodiscard]] bool ignored_pin_line_attack(const chess::Board& board, const Motif& motif,
                                           const chess::Move& reply,
                                           chess::Square attacker, chess::Square king) {
    if (motif.tag != MotifTag::PinAbsolute || motif.targets.empty() ||
        attacker != chess::Square(motif.actor.square)) {
        return false;
    }
    const chess::Square held(motif.targets.front().square);
    return reply.from() == held && board.at(held) == chess::Piece::NONE &&
           held != attacker && held != king && square_on_segment(attacker, king, held);
}

[[nodiscard]] bool king_safe_after_reply(const chess::Board& board, const Motif& motif,
                                         const chess::Move& reply, chess::Color y_side) {
    const chess::Color x_side = ~y_side;
    const chess::Square king = board.kingSq(y_side);
    for (int index = 0; index < 64; ++index) {
        const chess::Square attacker(index);
        const chess::Piece piece = board.at(attacker);
        if (piece != chess::Piece::NONE && piece.color() == x_side &&
            attacks_square(board, attacker, king) &&
            !ignored_pin_line_attack(board, motif, reply, attacker, king)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool denied_move_answers(const chess::Board& board, const Motif& motif,
                                       std::uint8_t landing) {
    const auto denied = denied_moves(board, motif);
    return std::ranges::any_of(denied, [&](const chess::Move reply) {
        return answers(board, motif, reply, landing);
    });
}


[[nodiscard]] bool move_touches_motif(const TrackedPieces& pieces, std::size_t count,
                                      chess::Move move, bool castling, bool capture,
                                      chess::Square captured) {
    return tracked_piece_at(pieces, count, move.from()) < count ||
           (castling && tracked_piece_at(pieces, count, move.to()) < count) ||
           (capture && tracked_piece_at(pieces, count, captured) < count);
}

void advance_tracked_pieces(const chess::Board& before, chess::Move move, bool castling,
                            bool capture, chess::Square captured, TrackedPieces& pieces,
                            std::size_t count) {
    if (capture) {
        const std::size_t captured_index = tracked_piece_at(pieces, count, captured);
        if (captured_index < count) {
            pieces[captured_index].present = false;
        }
    }
    if (castling) {
        const bool king_side = move.to() > move.from();
        const std::size_t king_index = tracked_piece_at(pieces, count, move.from());
        const std::size_t rook_index = tracked_piece_at(pieces, count, move.to());
        if (king_index < count) {
            pieces[king_index].square =
                chess::Square::castling_king_square(king_side, before.sideToMove());
        }
        if (rook_index < count) {
            pieces[rook_index].square =
                chess::Square::castling_rook_square(king_side, before.sideToMove());
        }
    } else {
        const std::size_t moved_index = tracked_piece_at(pieces, count, move.from());
        if (moved_index < count) {
            pieces[moved_index].square = move.to();
        }
    }
}

} // namespace

namespace detail {

std::vector<chess::Move> held_piece_move_set(const chess::Board& board, chess::Square held,
                                            MoveSetLegality legality) {
    const chess::Piece piece = board.at(held);
    if (piece == chess::Piece::NONE || piece.color() != board.sideToMove()) {
        return {};
    }

    std::vector<chess::Move> moves =
        legality == MoveSetLegality::PseudoLegal ? pseudo_legal_moves_from(board, held)
                                                  : legal_moves_from(board, held);
    if (piece.type() != chess::PieceType::PAWN) {
        return moves;
    }

    const chess::Color side = piece.color();
    const int next_rank = static_cast<int>(held.rank()) +
                          (side == chess::Color::WHITE ? 1 : -1);
    if (next_rank < 0 || next_rank >= 8) {
        return moves;
    }
    const int promotion_rank = side == chess::Color::WHITE ? 7 : 0;
    std::array<chess::Move, 8> hypothetical{};
    std::size_t hypothetical_count = 0;
    for (const int file_step : {-1, 1}) {
        const int file = static_cast<int>(held.file()) + file_step;
        if (file < 0 || file >= 8) {
            continue;
        }
        const chess::Square destination{chess::File(file), chess::Rank(next_rank)};
        if (board.at<chess::PieceType>(destination) == chess::PieceType::KING ||
            std::ranges::any_of(moves, [&](const chess::Move move) {
                return move.from() == held && move.to() == destination;
            })) {
            continue;
        }

        const auto is_legal = [&](chess::Move move) {
            if (legality == MoveSetLegality::PseudoLegal) {
                return true;
            }
            EditableBoard after(board);
            after.erase(destination);
            after.makeMove(move);
            return !after.isAttacked(after.kingSq(side), ~side);
        };
        if (next_rank == promotion_rank) {
            // Promotion type cannot alter attacks on the promoting side's king.
            const chess::Move queen_move =
                chess::Move::make<chess::Move::PROMOTION>(held, destination,
                                                          chess::PieceType::QUEEN);
            if (!is_legal(queen_move)) {
                continue;
            }
            constexpr std::array promotions{chess::PieceType::QUEEN, chess::PieceType::ROOK,
                                            chess::PieceType::BISHOP, chess::PieceType::KNIGHT};
            for (const chess::PieceType promotion : promotions) {
                hypothetical[hypothetical_count++] =
                    chess::Move::make<chess::Move::PROMOTION>(held, destination, promotion);
            }
        } else {
            const chess::Move move = chess::Move::make<chess::Move::NORMAL>(held, destination);
            if (is_legal(move)) {
                hypothetical[hypothetical_count++] = move;
            }
        }
    }
    if (hypothetical_count != 0) {
        moves.reserve(moves.size() + hypothetical_count);
        for (std::size_t index = 0; index < hypothetical_count; ++index) {
            moves.push_back(hypothetical[index]);
        }
    }
    return moves;
}

} // namespace detail

void judge_contributions(const chess::Board& root, std::span<const chess::Move> line,
                         std::span<Motif> motifs) {
    const std::vector<chess::Board> positions = positions_after(root, line);
    const chess::Color tactic_side = root.sideToMove();
    const Side report_tactic_side = report_side(tactic_side);
    const chess::Color enemy = ~tactic_side;

    for (Motif& motif : motifs) {
        if (motif.ply < 0 || static_cast<std::size_t>(motif.ply) >= positions.size()) {
            continue;
        }
        if (motif.side != report_tactic_side) {
            motif.contribution = MotifContribution::None;
            continue;
        }

        const std::size_t ply = static_cast<std::size_t>(motif.ply);
        const chess::Board& motif_position = positions[ply];
        TrackedPieces pieces{};
        std::size_t piece_count = 0;
        add_tracked_piece(motif_position, motif.actor.square, pieces, piece_count);
        for (const PieceOnSquare& target : motif.targets) {
            add_tracked_piece(motif_position, target.square, pieces, piece_count);
        }

        bool direct = is_move_motif(motif.tag) && ply > 0 &&
                      positions[ply - 1].sideToMove() == tactic_side;
        bool indirect = false;
        bool motif_piece_activity = false;

        if (!direct && ply > 0) {
            const chess::Board& before = positions[ply - 1];
            const chess::Move move = line[ply - 1];
            if (before.sideToMove() == tactic_side &&
                move.typeOf() != chess::Move::CASTLING) {
                indirect = denied_move_answers(positions[ply], motif,
                                                static_cast<std::uint8_t>(move.to().index()));
            }
        }

        for (std::size_t move_index = ply; !direct && move_index < line.size(); ++move_index) {
            const chess::Board& before = positions[move_index];
            const chess::Move move = line[move_index];
            const bool castling = move.typeOf() == chess::Move::CASTLING;
            const bool capture = before.isCapture(move);
            const chess::Square captured = captured_square(move);
            const bool x_move = before.sideToMove() == tactic_side;

            if (x_move && capture) {
                const std::size_t captured_index = tracked_piece_at(pieces, piece_count, captured);
                direct = captured_index < piece_count && pieces[captured_index].color == enemy;
            }
            if (direct) {
                break;
            }

            const bool touched_piece =
                move_touches_motif(pieces, piece_count, move, castling, capture, captured);
            if (!indirect && !motif_piece_activity && !touched_piece && x_move && !castling) {
                indirect = denied_move_answers(
                    positions[move_index + 1], motif,
                    static_cast<std::uint8_t>(move.to().index()));
            }
            motif_piece_activity = motif_piece_activity || touched_piece;
            advance_tracked_pieces(before, move, castling, capture, captured, pieces, piece_count);
        }

        if (direct) {
            motif.contribution = MotifContribution::Direct;
        } else if (indirect) {
            motif.contribution = MotifContribution::Indirect;
        } else {
            motif.contribution = MotifContribution::None;
        }
    }
}

std::vector<chess::Move> denied_moves(const chess::Board& board, const Motif& motif) {
    std::vector<chess::Move> denied;
    const chess::Color x_side = color_for(motif.side);
    const chess::Color y_side = ~x_side;
    if (board.sideToMove() != y_side) {
        return denied;
    }

    switch (motif.tag) {
    case MotifTag::PinAbsolute: {
        if (motif.targets.empty()) {
            break;
        }
        const chess::Square held(motif.targets.front().square);
        if (board.at(held) == chess::Piece::NONE || board.at(held).color() != y_side) {
            break;
        }
        const chess::Square actor(motif.actor.square);
        const chess::Square king = board.kingSq(y_side);
        for (const chess::Move move :
             detail::held_piece_move_set(board, held, detail::MoveSetLegality::PseudoLegal)) {
            if (!square_on_segment(actor, king, move.to())) {
                denied.push_back(move);
            }
        }
        break;
    }
    case MotifTag::PinRelative: {
        if (motif.targets.size() < 2) {
            break;
        }
        const chess::Square actor(motif.actor.square);
        const chess::Square held(motif.targets[0].square);
        const chess::Square rear(motif.targets[1].square);
        if (board.at(held) == chess::Piece::NONE || board.at(held).color() != y_side) {
            break;
        }
        for (const chess::Move move :
             detail::held_piece_move_set(board, held, detail::MoveSetLegality::Legal)) {
            if (square_on_segment(actor, rear, move.to())) {
                continue;
            }
            chess::Board after = board;
            after.makeMove(move);
            if (see(after, rear, x_side) >= 1) {
                denied.push_back(move);
            }
        }
        break;
    }
    case MotifTag::OverloadedDefender: {
        const chess::Square held(motif.actor.square);
        if (board.at(held) == chess::Piece::NONE || board.at(held).color() != y_side) {
            break;
        }
        for (const chess::Move move :
             detail::held_piece_move_set(board, held, detail::MoveSetLegality::Legal)) {
            chess::Board after = board;
            after.makeMove(move);
            const bool exposes_target = std::ranges::any_of(
                motif.targets, [&](const PieceOnSquare& target) {
                    const chess::Square target_square(target.square);
                    const chess::Piece piece = after.at(target_square);
                    return piece != chess::Piece::NONE && piece == board.at(target_square) &&
                           piece.color() == y_side && see(after, target_square, x_side) >= 1;
                });
            if (exposes_target) {
                denied.push_back(move);
            }
        }
        break;
    }
    case MotifTag::PinTactical: {
        if (motif.targets.empty()) {
            break;
        }
        const chess::Square held(motif.targets.front().square);
        if (board.at(held) == chess::Piece::NONE || board.at(held).color() != y_side) {
            break;
        }
        for (const chess::Move move :
             detail::held_piece_move_set(board, held, detail::MoveSetLegality::Legal)) {
            chess::Board after = board;
            after.makeMove(move);
            if (pin_tactical_threat_enabled(after, motif)) {
                denied.push_back(move);
            }
        }
        break;
    }
    case MotifTag::BackRankWeakness: {
        const chess::Square king = board.kingSq(y_side);
        const int front_step = y_side == chess::Color::WHITE ? 1 : -1;
        for (const int file_step : {-1, 0, 1}) {
            const int file = static_cast<int>(king.file()) + file_step;
            const int rank = static_cast<int>(king.rank()) + front_step;
            if (file < 0 || file >= 8 || rank < 0 || rank >= 8) {
                continue;
            }
            const chess::Square destination{chess::File(file), chess::Rank(rank)};
            const chess::Piece blocker = board.at(destination);
            if (blocker == chess::Piece::NONE || blocker.color() != y_side) {
                continue;
            }
            EditableBoard without_blocker(board);
            without_blocker.erase(destination);
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, without_blocker);
            for (const chess::Move move : legal_moves) {
                if (move.from() == king && move.to() == destination &&
                    move.typeOf() != chess::Move::CASTLING) {
                    denied.push_back(move);
                    break;
                }
            }
        }
        break;
    }
    case MotifTag::HangingPiece:
    case MotifTag::TrappedPiece:
    case MotifTag::Fork:
    case MotifTag::Skewer:
    case MotifTag::DiscoveredCheck:
    case MotifTag::DiscoveredAttack:
    case MotifTag::DoubleCheck:
    case MotifTag::BackRankMate:
    case MotifTag::Promotion:
        break;
    }
    sort_moves_by_uci(board, denied);
    return denied;
}

bool answers(const chess::Board& board, const Motif& motif, const chess::Move& reply,
             std::uint8_t landing) {
    if (reply == chess::Move::NO_MOVE) {
        return false;
    }
    const chess::Color x_side = color_for(motif.side);
    const chess::Color y_side = ~x_side;
    if (board.sideToMove() != y_side) {
        return false;
    }
    if (!board.inCheck()) {
        const chess::Square destination(landing);
        if (!board.isCapture(reply) || captured_square(reply) != destination) {
            return false;
        }
        const chess::Piece captured = board.at(destination);
        return captured != chess::Piece::NONE && captured.color() == x_side;
    }

    chess::Board after = board;
    if (motif.tag == MotifTag::BackRankWeakness) {
        const chess::Square king = board.kingSq(y_side);
        const chess::Square blocker = reply.to();
        const chess::Piece piece = board.at(blocker);
        if (reply.from() != king || piece == chess::Piece::NONE || piece.color() != y_side) {
            return false;
        }
        EditableBoard without_blocker(board);
        without_blocker.erase(blocker);
        after = without_blocker;
    }
    after.makeMove(reply);
    return king_safe_after_reply(after, motif, reply, y_side);
}

std::vector<TacticLabel> tactic_labels(const chess::Board& root,
                                       std::span<const chess::Move> line) {
    const std::vector<chess::Board> positions = positions_after(root, line);
    const chess::Color tactic_side = root.sideToMove();
    const chess::Color enemy = ~tactic_side;
    std::vector<TacticLabel> labels;

    for (std::size_t move_index = 0; move_index < line.size(); ++move_index) {
        const chess::Board& before = positions[move_index];
        const chess::Move move = line[move_index];
        if (before.sideToMove() != tactic_side || !before.isCapture(move)) {
            continue;
        }

        const chess::Square removed_defender_square = captured_square(move);
        const chess::Piece defender = before.at(removed_defender_square);
        if (defender == chess::Piece::NONE || defender.color() != enemy ||
            piece_value(defender.type()) == 0) {
            continue;
        }

        for (int target_index = 0; target_index < 64; ++target_index) {
            const chess::Square target_square(target_index);
            const chess::Piece target_piece = before.at(target_square);
            if (target_square == removed_defender_square || target_piece == chess::Piece::NONE ||
                target_piece.color() != enemy || piece_value(target_piece.type()) == 0 ||
                see(before, target_square, tactic_side) != 0 ||
                detail::see_without_piece(before, target_square, tactic_side,
                                          removed_defender_square) < 1) {
                continue;
            }

            bool captured_later = false;
            for (std::size_t later = move_index + 1; later < line.size(); ++later) {
                const chess::Board& future_before = positions[later];
                const chess::Move future = line[later];
                bool target_continuity = true;
                for (std::size_t intermediate = move_index + 1; intermediate < later;
                     ++intermediate) {
                    const chess::Board& intermediate_before = positions[intermediate];
                    const chess::Move intermediate_move = line[intermediate];
                    if (intermediate_move.from() == target_square ||
                        intermediate_move.to() == target_square ||
                        (intermediate_before.isCapture(intermediate_move) &&
                         captured_square(intermediate_move) == target_square)) {
                        target_continuity = false;
                        break;
                    }
                }
                if (target_continuity && future_before.sideToMove() == tactic_side &&
                    future.to() == target_square && future.typeOf() != chess::Move::ENPASSANT &&
                    future_before.isCapture(future) &&
                    see(future_before, target_square, tactic_side) >= 1) {
                    captured_later = true;
                    break;
                }
            }
            if (!captured_later) {
                continue;
            }
            TacticLabel label;
            label.tag = TacticLabelTag::RemovalOfDefender;
            label.side = report_side(tactic_side);
            label.ply = static_cast<int>(move_index + 1);
            label.actor = report_piece(positions[move_index + 1], move.to());
            label.targets = {report_piece(before, removed_defender_square),
                             report_piece(before, target_square)};
            labels.push_back(std::move(label));
        }
    }
    return labels;
}

} // namespace chesswhy::tactics
