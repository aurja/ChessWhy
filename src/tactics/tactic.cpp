#include "chesswhy/tactics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

void add_tracked_piece(const chess::Board& board, std::uint8_t square_index,
                       TrackedPieces& pieces, std::size_t& count) {
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

[[nodiscard]] bool motif_attacks(const chess::Board& board, const TrackedPieces& pieces,
                                 std::size_t count, chess::Color side,
                                 chess::Square destination) {
    for (std::size_t index = 0; index < count; ++index) {
        const TrackedPiece& piece = pieces[index];
        if (piece.present && piece.color == side &&
            attacks_square(board, piece.square, destination)) {
            return true;
        }
    }
    return false;
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

} // namespace

void judge_contributions(const chess::Board& root, std::span<const chess::Move> line,
                         std::span<Motif> motifs) {
    const std::vector<chess::Board> positions = positions_after(root, line);
    const chess::Color tactic_side = root.sideToMove();
    const Side report_tactic_side = report_side(tactic_side);

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

        bool direct = is_move_motif(motif.tag) && ply > 0;
        bool indirect = false;
        bool motif_piece_activity = false;
        const chess::Color enemy = ~tactic_side;
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
                tracked_piece_at(pieces, piece_count, move.from()) < piece_count ||
                (castling && tracked_piece_at(pieces, piece_count, move.to()) < piece_count) ||
                (capture && tracked_piece_at(pieces, piece_count, captured) < piece_count);
            if (!motif_piece_activity && !touched_piece && x_move && !castling) {
                indirect = indirect ||
                           motif_attacks(before, pieces, piece_count, enemy, move.to());
            }
            motif_piece_activity = motif_piece_activity || touched_piece;

            if (capture) {
                const std::size_t captured_index = tracked_piece_at(pieces, piece_count, captured);
                if (captured_index < piece_count) {
                    pieces[captured_index].present = false;
                }
            }
            if (castling) {
                const bool king_side = move.to() > move.from();
                const std::size_t king_index =
                    tracked_piece_at(pieces, piece_count, move.from());
                const std::size_t rook_index =
                    tracked_piece_at(pieces, piece_count, move.to());
                if (king_index < piece_count) {
                    pieces[king_index].square =
                        chess::Square::castling_king_square(king_side, before.sideToMove());
                }
                if (rook_index < piece_count) {
                    pieces[rook_index].square =
                        chess::Square::castling_rook_square(king_side, before.sideToMove());
                }
            } else {
                const std::size_t moved_index = tracked_piece_at(pieces, piece_count, move.from());
                if (moved_index < piece_count) {
                    pieces[moved_index].square = move.to();
                }
            }
        }

        motif.contribution = direct ? MotifContribution::Direct
                                    : (indirect ? MotifContribution::Indirect
                                                : MotifContribution::None);
    }
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

        const chess::Square defender_square = captured_square(move);
        const chess::Piece defender = before.at(defender_square);
        if (defender == chess::Piece::NONE || defender.color() != enemy ||
            piece_value(defender.type()) == 0) {
            continue;
        }

        for (int target_index = 0; target_index < 64; ++target_index) {
            const chess::Square target(target_index);
            const chess::Piece target_piece = before.at(target);
            if (target == defender_square || target_piece == chess::Piece::NONE ||
                target_piece.color() != enemy || piece_value(target_piece.type()) == 0 ||
                see(before, target, tactic_side) != 0 ||
                detail::see_without_piece(before, target, tactic_side, defender_square) < 1) {
                continue;
            }

            bool captured_later = false;
            for (std::size_t later = move_index + 1; later < line.size(); ++later) {
                const chess::Board& future_before = positions[later];
                const chess::Move future = line[later];
                if (future_before.sideToMove() == tactic_side && future.to() == target &&
                    future.typeOf() != chess::Move::ENPASSANT &&
                    future_before.isCapture(future) && see(future_before, target, tactic_side) >= 1) {
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
            label.targets = {report_piece(before, defender_square), report_piece(before, target)};
            labels.push_back(std::move(label));
        }
    }
    return labels;
}

} // namespace chesswhy::tactics
