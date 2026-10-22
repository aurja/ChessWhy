#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "move_set_internal.hpp"
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

[[nodiscard]] chess::Color motif_color(Side side) {
    return side == Side::White ? chess::Color::WHITE : chess::Color::BLACK;
}

[[nodiscard]] chess::Square held_piece_square(const Motif& motif) {
    if (motif.tag == MotifTag::OverloadedDefender) {
        return {static_cast<int>(motif.actor.square)};
    }
    if (motif.targets.empty()) {
        return chess::Square::NO_SQ;
    }
    return {static_cast<int>(motif.targets.front().square)};
}

[[nodiscard]] HeldTo held_to(std::uint64_t destinations, chess::Square held_square) {
    bool same_file = true;
    bool same_rank = true;
    bool same_diagonal = true;
    int diagonal_slope = 0;
    for (std::uint64_t remaining = destinations; remaining != 0; remaining &= remaining - 1) {
        const chess::Square destination(std::countr_zero(remaining));
        const int file_delta = destination.file() - held_square.file();
        const int rank_delta = destination.rank() - held_square.rank();
        same_file = same_file && destination.file() == held_square.file();
        same_rank = same_rank && destination.rank() == held_square.rank();
        if (file_delta == 0 || rank_delta == 0 ||
            std::abs(file_delta) != std::abs(rank_delta)) {
            same_diagonal = false;
        } else {
            const int slope = (file_delta > 0) == (rank_delta > 0) ? 1 : -1;
            if (diagonal_slope != 0 && diagonal_slope != slope) {
                same_diagonal = false;
            }
            diagonal_slope = slope;
        }
    }

    if (destinations == 0) {
        return HeldTo{.kind = HeldTo::Kind::Square,
                      .name = static_cast<std::string>(held_square)};
    }
    if (same_file) {
        return HeldTo{.kind = HeldTo::Kind::File,
                      .name = std::string(1, static_cast<char>('a' + held_square.file()))};
    }
    if (same_rank) {
        return HeldTo{.kind = HeldTo::Kind::Rank,
                      .name = std::string(1, static_cast<char>('1' + held_square.rank()))};
    }
    if (same_diagonal) {
        chess::Square first = held_square;
        while (true) {
            const chess::Square previous = offset(first, -1, -diagonal_slope);
            if (previous == chess::Square::NO_SQ) {
                break;
            }
            first = previous;
        }
        chess::Square last = first;
        while (true) {
            const chess::Square next = offset(last, 1, diagonal_slope);
            if (next == chess::Square::NO_SQ) {
                break;
            }
            last = next;
        }
        return HeldTo{.kind = HeldTo::Kind::Diagonal,
                      .name = static_cast<std::string>(first) + "-" +
                              static_cast<std::string>(last)};
    }

    const int destination_count = std::popcount(destinations);
    std::string name;
    name.reserve(static_cast<std::size_t>(destination_count * 3 - 1));
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const std::uint64_t destination_bit =
                std::uint64_t{1} << static_cast<unsigned>((rank * 8) + file);
            if ((destinations & destination_bit) == 0) {
                continue;
            }
            if (!name.empty()) {
                name.push_back(',');
            }
            name.push_back(static_cast<char>('a' + file));
            name.push_back(static_cast<char>('1' + rank));
        }
    }
    return HeldTo{.kind = HeldTo::Kind::Squares, .name = std::move(name)};
}

[[nodiscard]] std::optional<MotifRestriction> make_restriction(const chess::Board& board,
                                                                const Motif& motif) {
    const chess::Square held_square = held_piece_square(motif);
    if (held_square == chess::Square::NO_SQ || board.at(held_square) == chess::Piece::NONE) {
        return std::nullopt;
    }

    std::optional<chess::Board> null_moved;
    const chess::Board* by = &board;
    if (board.sideToMove() == motif_color(motif.side)) {
        null_moved.emplace(board);
        null_moved->makeNullMove();
        by = &null_moved.value();
    }
    const std::vector<chess::Move> denied = denied_moves(*by, motif);
    if (denied.empty()) {
        return std::nullopt;
    }

    MotifRestriction restriction;
    restriction.piece = report_piece(board, held_square);
    restriction.restricted.reserve(denied.size());
    for (const chess::Move move : denied) {
        restriction.restricted.push_back(chess::uci::moveToUci(move, by->chess960()));
    }
    std::ranges::sort(restriction.restricted);

    const std::vector<chess::Move> legal_moves =
        detail::held_piece_move_set(*by, held_square, detail::MoveSetLegality::Legal);
    std::uint64_t free_destinations = 0;
    for (const chess::Move move : legal_moves) {
        if (std::ranges::find(denied, move) != denied.end()) {
            continue;
        }
        restriction.free.push_back(chess::uci::moveToUci(move, by->chess960()));
        free_destinations |= std::uint64_t{1} << static_cast<unsigned>(move.to().index());
    }
    std::ranges::sort(restriction.free);
    restriction.held_to = held_to(free_destinations, held_square);
    return restriction;
}

void append_restricted_motif(const chess::Board& board, Motif motif,
                             std::vector<Motif>& motifs) {
    motif.restriction = make_restriction(board, motif);
    if (motif.restriction) {
        motifs.push_back(std::move(motif));
    }
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
            const bool in_check = board.inCheck();
            bool legal_capture_targets_ready = false;
            std::uint64_t legal_capture_targets = 0;
            for (const chess::Square square : hanging_squares) {
                const int gain = cached_see(square);
                if (gain < 3 || (last_capture && square == last_capture->moved_square &&
                                 last_capture->captured_value >= gain)) {
                    continue;
                }
                // SEE ignores check; an in-check side must have a legal capture on the target.
                if (in_check) {
                    if (!legal_capture_targets_ready) {
                        chess::Movelist legal_moves;
                        chess::movegen::legalmoves(legal_moves, board);
                        for (const chess::Move move : legal_moves) {
                            const chess::Square target = move.to();
                            const chess::Piece target_piece = board.at(target);
                            if (target_piece != chess::Piece::NONE &&
                                target_piece.color() == enemy) {
                                legal_capture_targets |= std::uint64_t{1}
                                                         << static_cast<unsigned>(target.index());
                            }
                        }
                        legal_capture_targets_ready = true;
                    }
                    const std::uint64_t target_bit = std::uint64_t{1}
                                                     << static_cast<unsigned>(square.index());
                    if ((legal_capture_targets & target_bit) == 0) {
                        continue;
                    }
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
                // Relative pins need at least one pawn of exposed material.
                if (rear_type == chess::PieceType::KING) {
                    append_restricted_motif(
                        board, make_motif(board, MotifTag::PinAbsolute, side, 0, pinner,
                                          {first, rear}),
                        motifs);
                } else if (comparison_value(rear_type) >
                               comparison_value(board.at<chess::PieceType>(first)) &&
                           detail::see_without_piece(board, rear, side, first) >= 1) {
                    append_restricted_motif(
                        board, make_motif(board, MotifTag::PinRelative, side, 0, pinner,
                                          {first, rear}),
                        motifs);
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
                    append_restricted_motif(
                        board,
                        make_motif(board, MotifTag::OverloadedDefender, side, 0, defender, targets),
                        motifs);
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
                // Null-moving an opponent in check can expose a pseudo-capture of its king.
                if (captured == chess::PieceType::KING) {
                    continue;
                }
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
           left.targets == right.targets && left.threat == right.threat;
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
    if (left.threat.has_value() != right.threat.has_value()) {
        return !left.threat.has_value();
    }
    if (left.threat && right.threat) {
        if (left.threat->kind != right.threat->kind) {
            return left.threat->kind < right.threat->kind;
        }
        if (left.threat->square != right.threat->square) {
            return left.threat->square < right.threat->square;
        }
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
        // A non-king front must leave a profitable rear capture when it vacates the ray.
        if ((front_type == chess::PieceType::KING ||
             comparison_value(front_type) > comparison_value(rear_type)) &&
            see_actor <= 0 &&
            (front_type == chess::PieceType::KING ||
             detail::see_without_piece(after, rear, side, front) >= 1)) {
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
                            const SquareList& checking_pieces, std::vector<Motif>& motifs,
                            bool* checkmate = nullptr) {
    const chess::Color enemy = ~side;
    if (!after.inCheck()) {
        return;
    }
    const bool has_legal_moves = chess::movegen::anylegalmoves(after);
    if (checkmate != nullptr) {
        *checkmate = !has_legal_moves;
    }
    if (has_legal_moves) {
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

void append_move_motifs(const chess::Board& before, const chess::Board& after,
                        const chess::Move& move, std::size_t ply_index, std::vector<Motif>& motifs,
                        bool* checkmate = nullptr) {
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
    append_back_rank_mates(after, side, static_cast<int>(ply_index + 1), checking_pieces, motifs,
                           checkmate);
    if (move.typeOf() == chess::Move::PROMOTION) {
        motifs.push_back(
            make_motif(after, MotifTag::Promotion, side, static_cast<int>(ply_index + 1), actor));
    }
}

constexpr std::array<MotifTag, 7> kPinMoveTags{MotifTag::Fork,
                                               MotifTag::Skewer,
                                               MotifTag::DiscoveredCheck,
                                               MotifTag::DiscoveredAttack,
                                               MotifTag::DoubleCheck,
                                               MotifTag::BackRankMate,
                                               MotifTag::Promotion};
constexpr std::array<ThreatKind, 9> kPinThreatKinds{ThreatKind::Fork,
                                                    ThreatKind::Skewer,
                                                    ThreatKind::DiscoveredCheck,
                                                    ThreatKind::DiscoveredAttack,
                                                    ThreatKind::DoubleCheck,
                                                    ThreatKind::BackRankMate,
                                                    ThreatKind::Promotion,
                                                    ThreatKind::Checkmate,
                                                    ThreatKind::Capture};

struct MoveThreatEffects {
    std::array<bool, kPinMoveTags.size()> move_tags{};
    bool checkmate = false;
};

struct PinThreatCandidate {
    chess::Square square = chess::Square::NO_SQ;
    SquareList targets;
};

using PinThreatSelections =
    std::array<std::array<std::optional<PinThreatCandidate>, 64>, kPinThreatKinds.size()>;

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

[[nodiscard]] chess::Board board_without(const chess::Board& board, chess::Square removed) {
    EditableBoard result(board);
    result.erase(removed);
    return result;
}

[[nodiscard]] chess::Square move_destination(chess::Move move, chess::Color side) {
    if (move.typeOf() == chess::Move::CASTLING) {
        return chess::Square::castling_king_square(move.to() > move.from(), side);
    }
    return move.to();
}

[[nodiscard]] std::uint32_t move_key(chess::Move move) {
    const std::uint32_t base =
        static_cast<std::uint32_t>((move.from().index() * 64) + move.to().index());
    const auto type = static_cast<std::uint32_t>(move.typeOf());
    const std::uint32_t promotion = move.typeOf() == chess::Move::PROMOTION
                                        ? static_cast<std::uint32_t>(move.promotionType())
                                        : 0U;
    return (((base * 8U) + type) * 8U) + promotion;
}

[[nodiscard]] bool could_be_fork(const chess::Board& after, chess::Square actor,
                                 chess::Color side) {
    const auto attacker_type = after.at<chess::PieceType>(actor);
    if (attacker_type == chess::PieceType::KING || see(after, actor, ~side) > 0) {
        return false;
    }
    const chess::Bitboard candidates =
        after.us(~side) & ~after.pieces(chess::PieceType::PAWN, ~side);
    int target_count = 0;
    for (const chess::Square target : squares(candidates.getBits())) {
        if (!attacks_square(after, actor, target)) {
            continue;
        }
        const auto target_type = after.at<chess::PieceType>(target);
        if (target_type == chess::PieceType::KING ||
            comparison_value(target_type) > comparison_value(attacker_type) ||
            (see(after, target, side) >= 1 && !attacks_square(after, target, actor))) {
            if (++target_count == 2) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool move_motif_candidate(const chess::Board& before, const chess::Board& after,
                                        chess::Move move, chess::Color side,
                                        const SquareList& checking_pieces) {
    const chess::Square actor = move_destination(move, side);
    const auto actor_type = after.at<chess::PieceType>(actor);
    if (actor_type != chess::PieceType::KING && could_be_fork(after, actor, side)) {
        return true;
    }
    if (actor_type == chess::PieceType::BISHOP || actor_type == chess::PieceType::ROOK ||
        actor_type == chess::PieceType::QUEEN || !checking_pieces.empty() ||
        move.typeOf() == chess::Move::PROMOTION) {
        return true;
    }
    const chess::Bitboard sliders = before.pieces(chess::PieceType::BISHOP, side) |
                                    before.pieces(chess::PieceType::ROOK, side) |
                                    before.pieces(chess::PieceType::QUEEN, side);
    return std::ranges::any_of(squares(sliders.getBits()), [&](const chess::Square slider) {
        return slider != move.from() && attacks_square(before, slider, move.from());
    });
}

[[nodiscard]] MoveThreatEffects move_threat_effects(const chess::Board& before, chess::Move move,
                                                    chess::Color side,
                                                    std::vector<Motif>* detected_motifs = nullptr) {
    MoveThreatEffects effects;
    chess::Board after = before;
    after.makeMove(move);
    const SquareList checking_pieces = checkers(after, side, after.kingSq(~side));
    if (!move_motif_candidate(before, after, move, side, checking_pieces)) {
        return effects;
    }

    std::vector<Motif> local_motifs;
    append_move_motifs(before, after, move, 0, local_motifs, &effects.checkmate);
    std::ranges::sort(local_motifs, motif_less);
    for (const Motif& motif : local_motifs) {
        const auto* found = std::ranges::find(kPinMoveTags, motif.tag);
        if (found != kPinMoveTags.end()) {
            effects.move_tags[static_cast<std::size_t>(found - kPinMoveTags.begin())] = true;
        }
    }
    if (detected_motifs != nullptr) {
        *detected_motifs = std::move(local_motifs);
    }
    return effects;
}

void append_pin_tactical(const chess::Board& board, std::vector<Motif>& motifs) {
    if (board.isAttacked(board.kingSq(chess::Color::WHITE), chess::Color::BLACK) ||
        board.isAttacked(board.kingSq(chess::Color::BLACK), chess::Color::WHITE) ||
        !chess::movegen::anylegalmoves(board)) {
        return;
    }
    for (const chess::Color side : {chess::Color::WHITE, chess::Color::BLACK}) {
        const chess::Color enemy = ~side;

        chess::Board bx = board;
        if (bx.sideToMove() != side) {
            bx.makeNullMove();
        }
        chess::Movelist baseline_moves;
        chess::movegen::legalmoves(baseline_moves, bx);
        std::unordered_map<std::uint32_t, MoveThreatEffects> baseline;
        baseline.reserve(baseline_moves.size());
        for (const chess::Move move : baseline_moves) {
            baseline.emplace(move_key(move), move_threat_effects(bx, move, side));
        }
        std::array<int, 64> baseline_see_cache{};
        baseline_see_cache.fill(-1);
        const auto baseline_see = [&bx, &baseline_see_cache, side](chess::Square target) {
            int& value = baseline_see_cache[static_cast<std::size_t>(target.index())];
            if (value < 0) {
                value = see(bx, target, side);
            }
            return value;
        };

        const chess::Bitboard enemy_pieces =
            board.us(enemy) & ~board.pieces(chess::PieceType::KING);
        for (const chess::Square p : squares(enemy_pieces.getBits())) {
            const chess::Board minus_p = board_without(bx, p);
            if (minus_p.isAttacked(minus_p.kingSq(enemy), side) ||
                minus_p.isAttacked(minus_p.kingSq(side), enemy)) {
                continue;
            }

            PinThreatSelections selected{};
            // These are X's legal threat moves; hypothetical pawn captures extend only the
            // held piece's move set, which append_restricted_motif checks before emitting.
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, minus_p);
            std::vector<chess::Move> ordered_moves(legal_moves.begin(), legal_moves.end());
            std::ranges::sort(ordered_moves, [side](chess::Move left, chess::Move right) {
                const chess::Square left_to = move_destination(left, side);
                const chess::Square right_to = move_destination(right, side);
                if (left_to != right_to) {
                    return left_to < right_to;
                }
                return move_key(left) < move_key(right);
            });

            for (const chess::Move move : ordered_moves) {
                std::vector<Motif> move_motifs;
                const MoveThreatEffects effects =
                    move_threat_effects(minus_p, move, side, &move_motifs);
                const auto baseline_move = baseline.find(move_key(move));
                const MoveThreatEffects* baseline_effects =
                    baseline_move == baseline.end() ? nullptr : &baseline_move->second;
                const chess::Square destination = move_destination(move, side);

                for (std::size_t kind = 0; kind < kPinMoveTags.size(); ++kind) {
                    if (!effects.move_tags[kind] ||
                        (baseline_effects != nullptr && baseline_effects->move_tags[kind])) {
                        continue;
                    }
                    const std::size_t actor = static_cast<std::size_t>(move.from().index());
                    auto& candidate = selected[kind][actor];
                    if (candidate && candidate->square <= destination) {
                        continue;
                    }
                    const auto found =
                        std::ranges::find_if(move_motifs, [kind](const Motif& motif) {
                            return motif.tag == kPinMoveTags[kind];
                        });
                    if (found == move_motifs.end()) {
                        continue;
                    }
                    SquareList targets;
                    targets.reserve(found->targets.size());
                    for (const PieceOnSquare& target : found->targets) {
                        targets.emplace_back(target.square);
                    }
                    candidate =
                        PinThreatCandidate{.square = destination, .targets = std::move(targets)};
                }

                if (effects.checkmate &&
                    (baseline_effects == nullptr || !baseline_effects->checkmate)) {
                    constexpr auto mate_kind = static_cast<std::size_t>(ThreatKind::Checkmate);
                    const std::size_t actor = static_cast<std::size_t>(move.from().index());
                    auto& candidate = selected[mate_kind][actor];
                    if (!candidate || destination < candidate->square) {
                        candidate = PinThreatCandidate{.square = destination,
                                                       .targets = {minus_p.kingSq(enemy)}};
                    }
                }
            }

            constexpr auto capture_kind = static_cast<std::size_t>(ThreatKind::Capture);
            for (const chess::Square target : squares(enemy_pieces.getBits())) {
                if (target == p || baseline_see(target) > 0 || see(minus_p, target, side) < 1) {
                    continue;
                }
                std::optional<chess::Move> capture;
                int capture_value = 0;
                for (const chess::Move move : ordered_moves) {
                    chess::Square captured_square = move.to();
                    if (move.typeOf() == chess::Move::ENPASSANT) {
                        captured_square = move.to().ep_square();
                    }
                    if (captured_square != target) {
                        continue;
                    }
                    const int attacker_value =
                        comparison_value(minus_p.at<chess::PieceType>(move.from()));
                    if (!capture || attacker_value < capture_value ||
                        (attacker_value == capture_value && move.from() < capture->from())) {
                        capture = move;
                        capture_value = attacker_value;
                    }
                }
                if (!capture) {
                    continue;
                }
                const std::size_t actor = static_cast<std::size_t>(capture->from().index());
                auto& candidate = selected[capture_kind][actor];
                const chess::Square destination = move_destination(*capture, side);
                if (!candidate || destination < candidate->square) {
                    candidate = PinThreatCandidate{.square = destination, .targets = {target}};
                }
            }

            for (std::size_t kind = 0; kind < selected.size(); ++kind) {
                for (std::size_t actor = 0; actor < selected[kind].size(); ++actor) {
                    const auto& selected_candidate = selected[kind][actor];
                    if (!selected_candidate.has_value()) {
                        continue;
                    }
                    const PinThreatCandidate& candidate = selected_candidate.value();
                    SquareList targets{p};
                    targets.insert(targets.end(), candidate.targets.begin(),
                                   candidate.targets.end());
                    Motif motif = make_motif(board, MotifTag::PinTactical, side, 0,
                                             chess::Square(static_cast<int>(actor)), targets);
                    motif.threat =
                        MotifThreat{.kind = kPinThreatKinds[kind],
                                    .square = static_cast<std::uint8_t>(candidate.square.index())};
                    append_restricted_motif(board, std::move(motif), motifs);
                }
            }
        }
    }
}

} // namespace

std::vector<Motif> position_motifs(const chess::Board& board, MotifOptions options) {
    std::vector<Motif> motifs = state_motifs(board);
    if (options.pin_tactical) {
        append_pin_tactical(board, motifs);
    }
    std::ranges::sort(motifs, motif_less);
    return motifs;
}

std::vector<Motif> line_motifs(const chess::Board& root, std::span<const chess::Move> line,
                               MotifOptions options) {
    std::vector<Motif> motifs;
    chess::Board before = root;
    std::vector<Motif> previous_state = state_motifs(before);
    if (options.pin_tactical) {
        append_pin_tactical(before, previous_state);
    }
    for (std::size_t ply = 0; ply < line.size(); ++ply) {
        const chess::Move move = line[ply];
        chess::Board after = before;
        after.makeMove(move);
        append_move_motifs(before, after, move, ply, motifs);

        std::optional<RecentCapture> last_capture;
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
        if (options.pin_tactical) {
            append_pin_tactical(after, current_state);
        }
        for (Motif motif : current_state) {
            const bool already_present =
                std::ranges::any_of(previous_state, [&motif](const Motif& previous) {
                    return same_identity(previous, motif);
                });
            if (already_present) {
                continue;
            }
            motif.ply = static_cast<int>(ply + 1);
            motifs.push_back(std::move(motif));
        }
        previous_state = std::move(current_state);
        before = std::move(after);
    }
    std::ranges::sort(motifs, motif_less);
    return motifs;
}

} // namespace chesswhy::tactics
