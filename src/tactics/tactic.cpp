#include "chesswhy/tactics.hpp"

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

[[nodiscard]] bool motif_has_square(const Motif& motif, chess::Square square) {
    if (motif.actor.square == static_cast<std::uint8_t>(square.index())) {
        return true;
    }
    for (const PieceOnSquare& target : motif.targets) {
        if (target.square == static_cast<std::uint8_t>(square.index())) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool motif_attacks(const chess::Board& board, const Motif& motif,
                                 chess::Square destination) {
    const auto is_attacking_piece = [&board, destination](const PieceOnSquare& piece) {
        const chess::Square origin(piece.square);
        return board.at(origin) != chess::Piece::NONE && attacks_square(board, origin, destination);
    };
    if (is_attacking_piece(motif.actor)) {
        return true;
    }
    for (const PieceOnSquare& target : motif.targets) {
        if (is_attacking_piece(target)) {
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
        bool direct = is_move_motif(motif.tag) && ply > 0;
        bool indirect = false;
        for (std::size_t move_index = ply; !direct && move_index < line.size(); ++move_index) {
            const chess::Board& before = positions[move_index];
            const chess::Move move = line[move_index];
            const bool castling = move.typeOf() == chess::Move::CASTLING;
            direct = motif_has_square(motif, move.from()) ||
                     (castling && motif_has_square(motif, move.to()));
            if (!direct && before.isCapture(move)) {
                direct = motif_has_square(motif, captured_square(move));
            }
            if (!direct && before.sideToMove() == tactic_side && !castling) {
                indirect = motif_attacks(before, motif, move.to());
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
