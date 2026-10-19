#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include "see_internal.hpp"

namespace chesswhy::tactics {
namespace {

using SquareList = std::vector<chess::Square>;
struct RecentCapture {
    chess::Square moved_square;
    int captured_value;
};

[[nodiscard]] Side report_side(chess::Color color) {
    return color == chess::Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] int comparison_value(chess::PieceType type) {
    return type == chess::PieceType::KING ? 100 : piece_value(type);
}

[[nodiscard]] char piece_letter(const chess::Board& board, chess::Square square) {
    return static_cast<std::string>(board.at(square)).front();
}

[[nodiscard]] PieceOnSquare report_piece(const chess::Board& board, chess::Square square) {
    return PieceOnSquare{.square = static_cast<std::uint8_t>(square.index()),
                         .piece = piece_letter(board, square)};
}

[[nodiscard]] Motif make_motif(const chess::Board& board, MotifTag tag, chess::Color side, int ply,
                               chess::Square actor, const SquareList& targets = {},
                               std::optional<int> gain = std::nullopt) {
    Motif motif;
    motif.tag = tag;
    motif.side = report_side(side);
    motif.ply = ply;
    motif.actor = report_piece(board, actor);
    motif.gain = gain;
    motif.targets.reserve(targets.size());
    for (const chess::Square target : targets) {
        motif.targets.push_back(report_piece(board, target));
    }
    return motif;
}

[[nodiscard]] chess::Square offset(chess::Square square, int file_delta, int rank_delta) {
    const int file = square.file() + file_delta;
    const int rank = square.rank() + rank_delta;
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return chess::Square::NO_SQ;
    }
    return {(rank * 8) + file};
}

[[nodiscard]] bool slider_matches(chess::PieceType type, int file_delta, int rank_delta) {
    if (file_delta != 0 && rank_delta != 0) {
        return type == chess::PieceType::BISHOP || type == chess::PieceType::QUEEN;
    }
    return type == chess::PieceType::ROOK || type == chess::PieceType::QUEEN;
}

[[nodiscard]] SquareList squares(std::uint64_t bits) {
    SquareList result;
    while (bits != 0) {
        const int index = std::countr_zero(bits);
        bits &= bits - 1;
        result.emplace_back(index);
    }
    return result;
}

[[nodiscard]] bool attacks_square(const chess::Board& board, chess::Square origin,
                                  chess::Square destination) {
    const chess::Piece piece = board.at(origin);
    if (piece.type() == chess::PieceType::NONE) {
        return false;
    }
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

[[nodiscard]] int direction(int from, int to) {
    if (from > to) {
        return 1;
    }
    if (from < to) {
        return -1;
    }
    return 0;
}

[[nodiscard]] std::vector<Motif>
state_motifs(const chess::Board& board, std::optional<RecentCapture> last_capture = std::nullopt) {
    std::vector<Motif> motifs;
    if (!chess::movegen::anylegalmoves(board)) {
        return motifs;
    }

    for (const chess::Color side : {chess::Color::WHITE, chess::Color::BLACK}) {
        const chess::Color enemy = ~side;
        std::array<int, 64> see_cache{};
        see_cache.fill(-1);
        const auto cached_see = [&board, &see_cache, side](chess::Square square) {
            int& value = see_cache[static_cast<std::size_t>(square.index())];
            if (value < 0) {
                value = see(board, square, side);
            }
            return value;
        };

        const chess::Bitboard hanging_candidates = board.pieces(chess::PieceType::KNIGHT, enemy) |
                                                   board.pieces(chess::PieceType::BISHOP, enemy) |
                                                   board.pieces(chess::PieceType::ROOK, enemy) |
                                                   board.pieces(chess::PieceType::QUEEN, enemy);
        const SquareList hanging_squares = squares(hanging_candidates.getBits());
        if (board.sideToMove() == side) {
            for (const chess::Square square : hanging_squares) {
                const int gain = cached_see(square);
                if (gain < 1 || (last_capture && square == last_capture->moved_square &&
                                 last_capture->captured_value >= gain)) {
                    continue;
                }
                motifs.push_back(
                    make_motif(board, MotifTag::HangingPiece, side, 0, square, {}, gain));
            }
        }

        constexpr std::array<std::array<int, 2>, 8> directions{{
            {{1, 0}},
            {{-1, 0}},
            {{0, 1}},
            {{0, -1}},
            {{1, 1}},
            {{1, -1}},
            {{-1, 1}},
            {{-1, -1}},
        }};
        const chess::Bitboard pinners = board.pieces(chess::PieceType::BISHOP, side) |
                                        board.pieces(chess::PieceType::ROOK, side) |
                                        board.pieces(chess::PieceType::QUEEN, side);
        for (const chess::Square pinner : squares(pinners.getBits())) {
            const auto pinner_type = board.at<chess::PieceType>(pinner);
            for (const auto& direction : directions) {
                const int file_delta = direction[0];
                const int rank_delta = direction[1];
                if (!slider_matches(pinner_type, file_delta, rank_delta)) {
                    continue;
                }
                chess::Square first = offset(pinner, file_delta, rank_delta);
                while (first != chess::Square::NO_SQ && board.at(first) == chess::Piece::NONE) {
                    first = offset(first, file_delta, rank_delta);
                }
                if (first == chess::Square::NO_SQ || board.at(first).color() != enemy ||
                    board.at<chess::PieceType>(first) == chess::PieceType::KING) {
                    continue;
                }
                chess::Square rear = offset(first, file_delta, rank_delta);
                while (rear != chess::Square::NO_SQ && board.at(rear) == chess::Piece::NONE) {
                    rear = offset(rear, file_delta, rank_delta);
                }
                if (rear == chess::Square::NO_SQ || board.at(rear).color() != enemy) {
                    continue;
                }
                const auto rear_type = board.at<chess::PieceType>(rear);
                if (rear_type == chess::PieceType::KING) {
                    motifs.push_back(
                        make_motif(board, MotifTag::PinAbsolute, side, 0, pinner, {first, rear}));
                } else if (comparison_value(rear_type) >
                           comparison_value(board.at<chess::PieceType>(first))) {
                    motifs.push_back(
                        make_motif(board, MotifTag::PinRelative, side, 0, pinner, {first, rear}));
                }
            }
        }

        const chess::Square enemy_king = board.kingSq(enemy);
        const int back_rank = enemy == chess::Color::WHITE ? 0 : 7;
        const std::uint64_t back_rank_mask = 0xffULL << (back_rank * 8);
        const std::uint64_t enemy_heavy = (board.pieces(chess::PieceType::ROOK, enemy) |
                                           board.pieces(chess::PieceType::QUEEN, enemy))
                                              .getBits();
        if (static_cast<int>(enemy_king.rank()) == back_rank &&
            (board.pieces(chess::PieceType::ROOK, side) ||
             board.pieces(chess::PieceType::QUEEN, side)) &&
            (enemy_heavy & back_rank_mask) == 0) {
            const int rank_delta = enemy == chess::Color::WHITE ? 1 : -1;
            bool weak = true;
            for (int file_delta = -1; file_delta <= 1; ++file_delta) {
                const chess::Square front = offset(enemy_king, file_delta, rank_delta);
                if (front == chess::Square::NO_SQ) {
                    continue;
                }
                const chess::Piece occupant = board.at(front);
                if (!((occupant != chess::Piece::NONE && occupant.color() == enemy) ||
                      board.isAttacked(front, side))) {
                    weak = false;
                    break;
                }
            }
            if (weak) {
                motifs.push_back(
                    make_motif(board, MotifTag::BackRankWeakness, side, 0, enemy_king));
            }
        }

        const chess::Bitboard enemy_non_kings =
            board.us(enemy) & ~board.pieces(chess::PieceType::KING);
        const SquareList enemy_non_king_squares = squares(enemy_non_kings.getBits());
        SquareList vulnerable_targets;
        for (const chess::Square target : enemy_non_king_squares) {
            if (board.isAttacked(target, side) && cached_see(target) <= 0) {
                vulnerable_targets.push_back(target);
            }
        }
        if (!vulnerable_targets.empty()) {
            std::vector<int> removal_see(vulnerable_targets.size());
            for (const chess::Square defender : enemy_non_king_squares) {
                detail::see_without_piece(board, vulnerable_targets, side, defender, removal_see);
                SquareList targets;
                for (std::size_t index = 0; index < vulnerable_targets.size(); ++index) {
                    if (vulnerable_targets[index] != defender && removal_see[index] >= 1) {
                        targets.push_back(vulnerable_targets[index]);
                    }
                }
                if (targets.size() >= 2) {
                    motifs.push_back(make_motif(board, MotifTag::OverloadedDefender, side, 0,
                                                defender, targets));
                }
            }
        }

        const bool enemy_in_check = board.isAttacked(enemy_king, side);
        if (enemy_in_check) {
            continue;
        }
        bool has_hanging_candidate = false;
        for (const chess::Square square : hanging_squares) {
            if (cached_see(square) >= 1) {
                has_hanging_candidate = true;
                break;
            }
        }
        if (!has_hanging_candidate) {
            continue;
        }

        chess::Board enemy_to_move = board;
        if (enemy_to_move.sideToMove() != enemy) {
            enemy_to_move.makeNullMove();
        }
        chess::Movelist legal_moves;
        chess::movegen::legalmoves(legal_moves, enemy_to_move);
        for (const chess::Square candidate : hanging_squares) {
            const int gain = cached_see(candidate);
            if (gain < 1) {
                continue;
            }
            const auto candidate_type = board.at<chess::PieceType>(candidate);
            bool trapped = true;
            for (const chess::Move move : legal_moves) {
                if (move.from() != candidate) {
                    continue;
                }
                chess::Square captured_square = move.to();
                if (move.typeOf() == chess::Move::ENPASSANT) {
                    captured_square = move.to().ep_square();
                }
                const auto captured = enemy_to_move.at<chess::PieceType>(captured_square);
                if (captured != chess::PieceType::NONE &&
                    comparison_value(captured) >= comparison_value(candidate_type)) {
                    trapped = false;
                    break;
                }
                chess::Board after = enemy_to_move;
                after.makeMove(move);
                if (see(after, move.to(), side) < 1) {
                    trapped = false;
                    break;
                }
            }
            if (trapped) {
                motifs.push_back(
                    make_motif(board, MotifTag::TrappedPiece, side, 0, candidate, {}, gain));
            }
        }
    }

    return motifs;
}

[[nodiscard]] bool same_identity(const Motif& left, const Motif& right) {
    return left.tag == right.tag && left.side == right.side && left.actor == right.actor &&
           left.targets == right.targets;
}

[[nodiscard]] bool motif_less(const Motif& left, const Motif& right) {
    if (left.ply != right.ply) {
        return left.ply < right.ply;
    }
    if (left.tag != right.tag) {
        return left.tag < right.tag;
    }
    if (left.actor.square != right.actor.square) {
        return left.actor.square < right.actor.square;
    }
    const std::size_t shared_targets = std::min(left.targets.size(), right.targets.size());
    for (std::size_t index = 0; index < shared_targets; ++index) {
        if (left.targets[index].square != right.targets[index].square) {
            return left.targets[index].square < right.targets[index].square;
        }
    }
    if (left.targets.size() != right.targets.size()) {
        return left.targets.size() < right.targets.size();
    }
    if (left.side != right.side) {
        return left.side < right.side;
    }
    if (left.actor.piece != right.actor.piece) {
        return left.actor.piece < right.actor.piece;
    }
    for (std::size_t index = 0; index < shared_targets; ++index) {
        if (left.targets[index].piece != right.targets[index].piece) {
            return left.targets[index].piece < right.targets[index].piece;
        }
    }
    return false;
}

void append_fork(const chess::Board& after, chess::Square actor, chess::Color side, int ply,
                 std::vector<Motif>& motifs) {
    const auto attacker_type = after.at<chess::PieceType>(actor);
    if (attacker_type == chess::PieceType::KING) {
        return;
    }
    const chess::Color enemy = ~side;
    SquareList targets;
    const chess::Bitboard candidates =
        after.us(enemy) & ~after.pieces(chess::PieceType::PAWN, enemy);
    for (const chess::Square target : squares(candidates.getBits())) {
        if (!attacks_square(after, actor, target)) {
            continue;
        }
        const auto target_type = after.at<chess::PieceType>(target);
        const chess::Square defender = target;
        const chess::Square destination = actor;
        if (target_type == chess::PieceType::KING ||
            comparison_value(target_type) > comparison_value(attacker_type) ||
            (see(after, target, side) >= 1 && !attacks_square(after, defender, destination))) {
            targets.push_back(target);
        }
    }
    if (targets.size() >= 2 && see(after, actor, enemy) <= 0) {
        std::ranges::sort(targets);
        motifs.push_back(make_motif(after, MotifTag::Fork, side, ply, actor, targets));
    }
}

void append_skewers(const chess::Board& after, chess::Square actor, chess::Color side, int ply,
                    std::vector<Motif>& motifs) {
    const auto attacker_type = after.at<chess::PieceType>(actor);
    if (attacker_type != chess::PieceType::BISHOP && attacker_type != chess::PieceType::ROOK &&
        attacker_type != chess::PieceType::QUEEN) {
        return;
    }
    constexpr std::array<std::array<int, 2>, 8> directions{{
        {{1, 0}},
        {{-1, 0}},
        {{0, 1}},
        {{0, -1}},
        {{1, 1}},
        {{1, -1}},
        {{-1, 1}},
        {{-1, -1}},
    }};
    const chess::Color enemy = ~side;
    const int see_actor = see(after, actor, enemy);
    for (const auto& direction : directions) {
        const int file_delta = direction[0];
        const int rank_delta = direction[1];
        if (!slider_matches(attacker_type, file_delta, rank_delta)) {
            continue;
        }
        chess::Square front = offset(actor, file_delta, rank_delta);
        while (front != chess::Square::NO_SQ && after.at(front) == chess::Piece::NONE) {
            front = offset(front, file_delta, rank_delta);
        }
        if (front == chess::Square::NO_SQ || after.at(front).color() != enemy) {
            continue;
        }
        chess::Square rear = offset(front, file_delta, rank_delta);
        while (rear != chess::Square::NO_SQ && after.at(rear) == chess::Piece::NONE) {
            rear = offset(rear, file_delta, rank_delta);
        }
        if (rear == chess::Square::NO_SQ || after.at(rear).color() != enemy ||
            after.at<chess::PieceType>(rear) == chess::PieceType::PAWN) {
            continue;
        }
        const auto front_type = after.at<chess::PieceType>(front);
        const auto rear_type = after.at<chess::PieceType>(rear);
        if ((front_type == chess::PieceType::KING ||
             comparison_value(front_type) > comparison_value(rear_type)) &&
            (front_type == chess::PieceType::KING || see_actor <= 0)) {
            motifs.push_back(make_motif(after, MotifTag::Skewer, side, ply, actor, {front, rear}));
        }
    }
}

[[nodiscard]] SquareList checkers(const chess::Board& board, chess::Color side,
                                  chess::Square king) {
    return squares(chess::attacks::attackers(board, side, king).getBits());
}

void append_discovered_attacks(const chess::Board& before, const chess::Board& after,
                               chess::Square from, chess::Square to, chess::Color side, int ply,
                               bool discovered_check, std::vector<Motif>& motifs) {
    if (discovered_check) {
        return;
    }
    const chess::Color enemy = ~side;
    const chess::Bitboard sliders = before.pieces(chess::PieceType::BISHOP, side) |
                                    before.pieces(chess::PieceType::ROOK, side) |
                                    before.pieces(chess::PieceType::QUEEN, side);
    for (const chess::Square slider : squares(sliders.getBits())) {
        if (slider == from || after.at(slider) != before.at(slider)) {
            continue;
        }
        const auto slider_type = before.at<chess::PieceType>(slider);
        const int file_delta = direction(from.file(), slider.file());
        const int rank_delta = direction(from.rank(), slider.rank());
        if ((file_delta == 0 && rank_delta == 0) ||
            (file_delta != 0 && rank_delta != 0 &&
             std::abs(from.file() - slider.file()) != std::abs(from.rank() - slider.rank())) ||
            !slider_matches(slider_type, file_delta, rank_delta)) {
            continue;
        }
        SquareList targets;
        chess::Square target = offset(from, file_delta, rank_delta);
        while (target != chess::Square::NO_SQ) {
            const chess::Piece occupant = after.at(target);
            if (occupant != chess::Piece::NONE) {
                if (occupant.color() == enemy && occupant.type() != chess::PieceType::PAWN &&
                    target != to && attacks_square(after, slider, target) &&
                    (comparison_value(occupant.type()) > comparison_value(slider_type) ||
                     see(after, target, side) >= 1)) {
                    targets.push_back(target);
                }
                break;
            }
            if (target == to) {
                break;
            }
            target = offset(target, file_delta, rank_delta);
        }
        if (!targets.empty()) {
            std::ranges::sort(targets);
            motifs.push_back(
                make_motif(after, MotifTag::DiscoveredAttack, side, ply, slider, targets));
        }
    }
}

void append_back_rank_mates(const chess::Board& after, chess::Color side, int ply,
                            const SquareList& checking_pieces, std::vector<Motif>& motifs) {
    const chess::Color enemy = ~side;
    if (!after.inCheck() || chess::movegen::anylegalmoves(after)) {
        return;
    }
    const chess::Square king = after.kingSq(enemy);
    const int first_rank = enemy == chess::Color::WHITE ? 0 : 7;
    if (static_cast<int>(king.rank()) != first_rank) {
        return;
    }
    const int rank_delta = enemy == chess::Color::WHITE ? 1 : -1;
    for (int file_delta = -1; file_delta <= 1; ++file_delta) {
        const chess::Square front = offset(king, file_delta, rank_delta);
        if (front != chess::Square::NO_SQ &&
            (after.at(front) == chess::Piece::NONE || after.at(front).color() != enemy)) {
            return;
        }
    }
    for (const chess::Square checker : checking_pieces) {
        if (checker.rank() == king.rank()) {
            motifs.push_back(make_motif(after, MotifTag::BackRankMate, side, ply, checker, {king}));
        }
    }
}

void append_removal_of_defender(const chess::Board& before, const chess::Board& after,
                                const chess::Move& move, const std::vector<chess::Board>& positions,
                                std::span<const chess::Move> line, std::size_t ply_index,
                                chess::Color side, std::vector<Motif>& motifs) {
    chess::Square defender = move.to();
    if (move.typeOf() == chess::Move::ENPASSANT) {
        defender = move.to().ep_square();
    }
    if (!before.isCapture(move) || before.at(defender) == chess::Piece::NONE ||
        before.at(defender).color() == side ||
        before.at<chess::PieceType>(defender) == chess::PieceType::KING) {
        return;
    }
    const chess::Color enemy = ~side;
    const chess::Bitboard targets = before.us(enemy) & ~before.pieces(chess::PieceType::KING);
    const chess::Square actor = move.to();
    for (const chess::Square target : squares(targets.getBits())) {
        if (target == defender || see(before, target, side) > 0 ||
            detail::see_without_piece(before, target, side, defender) < 1) {
            continue;
        }
        bool captured_later = false;
        for (std::size_t later = ply_index + 1; later < line.size(); ++later) {
            const chess::Move future = line[later];
            const chess::Board& future_before = positions[later];
            if (future_before.sideToMove() == side && future.to() == target &&
                future_before.isCapture(future) && future.typeOf() != chess::Move::ENPASSANT) {
                captured_later = true;
                break;
            }
        }
        if (captured_later) {
            Motif motif = make_motif(after, MotifTag::RemovalOfDefender, side,
                                     static_cast<int>(ply_index + 1), actor);
            motif.targets = {report_piece(before, defender), report_piece(before, target)};
            motifs.push_back(std::move(motif));
        }
    }
}

void append_move_motifs(const chess::Board& before, const chess::Board& after,
                        const chess::Move& move, const std::vector<chess::Board>& positions,
                        std::span<const chess::Move> line, std::size_t ply_index,
                        std::vector<Motif>& motifs) {
    const chess::Color side = before.sideToMove();
    const chess::Square from = move.from();
    const bool is_castling = move.typeOf() == chess::Move::CASTLING;
    const bool king_side_castle = move.to() > from;
    const chess::Square actor =
        is_castling ? chess::Square::castling_king_square(king_side_castle, side) : move.to();
    const chess::Square castling_rook_destination =
        is_castling ? chess::Square::castling_rook_square(king_side_castle, side)
                    : chess::Square::NO_SQ;
    const chess::Square king = after.kingSq(~side);

    const SquareList checking_pieces = checkers(after, side, king);
    const bool discovered_check = std::ranges::any_of(
        checking_pieces, [actor, castling_rook_destination](chess::Square square) {
            return square != actor && square != castling_rook_destination;
        });

    append_fork(after, actor, side, static_cast<int>(ply_index + 1), motifs);
    append_skewers(after, actor, side, static_cast<int>(ply_index + 1), motifs);
    for (const chess::Square checker : checking_pieces) {
        if (checker != actor && checker != castling_rook_destination) {
            motifs.push_back(make_motif(after, MotifTag::DiscoveredCheck, side,
                                        static_cast<int>(ply_index + 1), checker, {king}));
        }
    }
    append_discovered_attacks(before, after, from, actor, side, static_cast<int>(ply_index + 1),
                              discovered_check, motifs);
    if (checking_pieces.size() >= 2) {
        motifs.push_back(make_motif(after, MotifTag::DoubleCheck, side,
                                    static_cast<int>(ply_index + 1), actor, {king}));
    }
    append_back_rank_mates(after, side, static_cast<int>(ply_index + 1), checking_pieces, motifs);
    append_removal_of_defender(before, after, move, positions, line, ply_index, side, motifs);
    if (move.typeOf() == chess::Move::PROMOTION) {
        motifs.push_back(
            make_motif(after, MotifTag::Promotion, side, static_cast<int>(ply_index + 1), actor));
    }
}

} // namespace

std::vector<Motif> position_motifs(const chess::Board& board) {
    std::vector<Motif> motifs = state_motifs(board);
    std::ranges::sort(motifs, motif_less);
    return motifs;
}

std::vector<Motif> line_motifs(const chess::Board& root, std::span<const chess::Move> line) {
    std::vector<chess::Board> positions;
    positions.reserve(line.size() + 1);
    positions.push_back(root);
    for (const chess::Move move : line) {
        chess::Board next = positions.back();
        next.makeMove(move);
        positions.push_back(std::move(next));
    }

    std::vector<Motif> motifs;
    std::vector<Motif> previous_state = state_motifs(positions.front());
    for (std::size_t ply = 0; ply < line.size(); ++ply) {
        const chess::Board& before = positions[ply];
        const chess::Board& after = positions[ply + 1];
        append_move_motifs(before, after, line[ply], positions, line, ply, motifs);

        std::optional<RecentCapture> last_capture;
        const chess::Move move = line[ply];
        if (before.isCapture(move)) {
            chess::Square captured_square = move.to();
            if (move.typeOf() == chess::Move::ENPASSANT) {
                captured_square = move.to().ep_square();
            }
            last_capture = RecentCapture{
                .moved_square = move.to(),
                .captured_value = comparison_value(before.at<chess::PieceType>(captured_square))};
        }
        std::vector<Motif> current_state = state_motifs(after, last_capture);
        for (Motif motif : current_state) {
            const bool already_present =
                std::ranges::any_of(previous_state, [&motif](const Motif& previous) {
                    return same_identity(previous, motif);
                });
            if (!already_present) {
                motif.ply = static_cast<int>(ply + 1);
                motifs.push_back(std::move(motif));
            }
        }
        previous_state = std::move(current_state);
    }
    std::ranges::sort(motifs, motif_less);
    return motifs;
}

} // namespace chesswhy::tactics
