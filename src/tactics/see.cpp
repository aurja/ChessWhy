#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>

#include "see_internal.hpp"

namespace chesswhy::tactics {
namespace {

using BoardState = std::array<chess::PieceType, 64>;
using ColorBoards = std::array<std::array<std::uint64_t, 6>, 2>;

struct Position {
    BoardState types{};
    std::array<chess::Color, 64> colors{};
    ColorBoards pieces{};
    std::array<std::uint64_t, 2> occupied{};

    explicit Position(const chess::Board& board) {
        for (int index = 0; index < 64; ++index) {
            const chess::Square square(index);
            const chess::Piece piece = board.at(square);
            if (piece.type() != chess::PieceType::NONE) {
                add(square, piece.type(), piece.color());
            }
        }
    }

    void add(chess::Square square, chess::PieceType type, chess::Color color) {
        const std::uint64_t bit = std::uint64_t{1} << square.index();
        types[static_cast<std::size_t>(square.index())] = type;
        colors[static_cast<std::size_t>(square.index())] = color;
        pieces[static_cast<std::size_t>(static_cast<int>(color))][static_cast<std::size_t>(type)] |=
            bit;
        occupied[static_cast<std::size_t>(static_cast<int>(color))] |= bit;
    }

    void remove(chess::Square square) {
        const auto index = static_cast<std::size_t>(square.index());
        const chess::PieceType type = types[index];
        if (type == chess::PieceType::NONE) {
            return;
        }
        const auto color = static_cast<std::size_t>(static_cast<int>(colors[index]));
        const std::uint64_t bit = std::uint64_t{1} << square.index();
        pieces[color][static_cast<std::size_t>(type)] &= ~bit;
        occupied[color] &= ~bit;
        types[index] = chess::PieceType::NONE;
        colors[index] = chess::Color::NONE;
    }
};

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

[[nodiscard]] bool capture_stays_on_pin_ray(const Position& position, chess::Square source,
                                            chess::Square target, chess::Color side) {
    const chess::Square king = [&position, side] {
        const std::uint64_t kings =
            position.pieces[static_cast<std::size_t>(static_cast<int>(side))]
                           [static_cast<std::size_t>(chess::PieceType::KING)];
        return chess::Square{std::countr_zero(kings)};
    }();

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

    for (const auto& direction : directions) {
        const int file_delta = direction[0];
        const int rank_delta = direction[1];
        chess::Square cursor = offset(king, file_delta, rank_delta);
        if (cursor == chess::Square::NO_SQ) {
            continue;
        }
        while (cursor != chess::Square::NO_SQ) {
            const auto cursor_index = static_cast<std::size_t>(cursor.index());
            if (position.types[cursor_index] != chess::PieceType::NONE) {
                if (position.colors[cursor_index] != side || cursor != source) {
                    break;
                }
                cursor = offset(cursor, file_delta, rank_delta);
                while (cursor != chess::Square::NO_SQ) {
                    const auto pinner_index = static_cast<std::size_t>(cursor.index());
                    if (position.types[pinner_index] != chess::PieceType::NONE) {
                        if (position.colors[pinner_index] == side ||
                            !slider_matches(position.types[pinner_index], file_delta, rank_delta)) {
                            break;
                        }
                        chess::Square pin_ray = offset(king, file_delta, rank_delta);
                        while (pin_ray != chess::Square::NO_SQ) {
                            if (pin_ray == target) {
                                return true;
                            }
                            if (pin_ray == cursor) {
                                break;
                            }
                            pin_ray = offset(pin_ray, file_delta, rank_delta);
                        }
                        return false;
                    }
                    cursor = offset(cursor, file_delta, rank_delta);
                }
                break;
            }
            cursor = offset(cursor, file_delta, rank_delta);
        }
    }
    return true;
}

[[nodiscard]] bool attacked_by(const Position& position, chess::Square square, chess::Color side) {
    const auto color = static_cast<std::size_t>(static_cast<int>(side));
    const auto& pieces = position.pieces[color];
    const chess::Bitboard occupied(position.occupied[0] | position.occupied[1]);

    return static_cast<bool>(
               chess::attacks::pawn(~side, square) &
               chess::Bitboard(pieces[static_cast<std::size_t>(chess::PieceType::PAWN)])) ||
           static_cast<bool>(
               chess::attacks::knight(square) &
               chess::Bitboard(pieces[static_cast<std::size_t>(chess::PieceType::KNIGHT)])) ||
           static_cast<bool>(
               chess::attacks::king(square) &
               chess::Bitboard(pieces[static_cast<std::size_t>(chess::PieceType::KING)])) ||
           static_cast<bool>(
               chess::attacks::bishop(square, occupied) &
               chess::Bitboard(pieces[static_cast<std::size_t>(chess::PieceType::BISHOP)] |
                               pieces[static_cast<std::size_t>(chess::PieceType::QUEEN)])) ||
           static_cast<bool>(
               chess::attacks::rook(square, occupied) &
               chess::Bitboard(pieces[static_cast<std::size_t>(chess::PieceType::ROOK)] |
                               pieces[static_cast<std::size_t>(chess::PieceType::QUEEN)]));
}

[[nodiscard]] std::uint64_t attackers_to(const Position& position, chess::Square square,
                                         chess::Color side, chess::PieceType type) {
    const auto color = static_cast<std::size_t>(static_cast<int>(side));
    const std::uint64_t typed = position.pieces[color][static_cast<std::size_t>(type)];
    const chess::Bitboard occupied(position.occupied[0] | position.occupied[1]);
    switch (type.internal()) {
    case chess::PieceType::PAWN:
        return (chess::attacks::pawn(~side, square) & chess::Bitboard(typed)).getBits();
    case chess::PieceType::KNIGHT:
        return (chess::attacks::knight(square) & chess::Bitboard(typed)).getBits();
    case chess::PieceType::BISHOP:
        return (chess::attacks::bishop(square, occupied) & chess::Bitboard(typed)).getBits();
    case chess::PieceType::ROOK:
        return (chess::attacks::rook(square, occupied) & chess::Bitboard(typed)).getBits();
    case chess::PieceType::QUEEN:
        return ((chess::attacks::bishop(square, occupied) |
                 chess::attacks::rook(square, occupied)) &
                chess::Bitboard(typed))
            .getBits();
    case chess::PieceType::KING:
        return (chess::attacks::king(square) & chess::Bitboard(typed)).getBits();
    default:
        return 0;
    }
}

[[nodiscard]] int exchange_value(chess::PieceType type) {
    return type == chess::PieceType::KING ? 100 : piece_value(type);
}

[[nodiscard]] bool least_attacker(const Position& position, chess::Square target, chess::Color side,
                                  chess::Square& source, chess::PieceType& type) {
    constexpr std::array<chess::PieceType, 6> attacker_order{
        chess::PieceType::PAWN, chess::PieceType::KNIGHT, chess::PieceType::BISHOP,
        chess::PieceType::ROOK, chess::PieceType::QUEEN,  chess::PieceType::KING,
    };
    for (const chess::PieceType candidate_type : attacker_order) {
        std::uint64_t attackers = attackers_to(position, target, side, candidate_type);
        while (attackers != 0) {
            const int index = std::countr_zero(attackers);
            attackers &= attackers - 1;
            const chess::Square candidate(index);
            if (candidate_type != chess::PieceType::KING &&
                !capture_stays_on_pin_ray(position, candidate, target, side)) {
                continue;
            }

            Position after = position;
            after.remove(candidate);
            after.remove(target);
            after.add(target, candidate_type, side);
            if (candidate_type == chess::PieceType::KING && attacked_by(after, target, ~side)) {
                continue;
            }

            source = candidate;
            type = candidate_type;
            return true;
        }
    }
    return false;
}

[[nodiscard]] int exchange(const Position& position, chess::Square target, chess::Color side) {
    const auto target_index = static_cast<std::size_t>(target.index());
    const chess::PieceType victim = position.types[target_index];
    if (victim == chess::PieceType::NONE || position.colors[target_index] == side ||
        victim == chess::PieceType::KING) {
        return 0;
    }

    chess::Square attacker;
    chess::PieceType attacker_type;
    if (!least_attacker(position, target, side, attacker, attacker_type)) {
        return 0;
    }

    Position after = position;
    after.remove(attacker);
    after.remove(target);
    chess::PieceType resulting_type = attacker_type;
    int promotion_gain = 0;
    if (attacker_type == chess::PieceType::PAWN &&
        static_cast<int>(target.rank()) == (side == chess::Color::WHITE ? 7 : 0)) {
        resulting_type = chess::PieceType::QUEEN;
        promotion_gain = piece_value(resulting_type) - piece_value(attacker_type);
    }
    after.add(target, resulting_type, side);

    const int capture_gain = exchange_value(victim) + promotion_gain;
    return std::max(0, capture_gain - exchange(after, target, ~side));
}

[[nodiscard]] int see_position(const Position& position, chess::Square square, chess::Color side) {
    if (!square.is_valid()) {
        return 0;
    }
    const auto index = static_cast<std::size_t>(square.index());
    if (position.types[index] == chess::PieceType::NONE || position.colors[index] == side ||
        position.types[index] == chess::PieceType::KING) {
        return 0;
    }
    return exchange(position, square, side);
}

} // namespace

int piece_value(chess::PieceType type) {
    switch (type.internal()) {
    case chess::PieceType::PAWN:
        return 1;
    case chess::PieceType::KNIGHT:
    case chess::PieceType::BISHOP:
        return 3;
    case chess::PieceType::ROOK:
        return 5;
    case chess::PieceType::QUEEN:
        return 9;
    default:
        return 0;
    }
}

int see(const chess::Board& board, chess::Square square, chess::Color side) {
    return see_position(Position(board), square, side);
}

} // namespace chesswhy::tactics

namespace chesswhy::tactics::detail {

int see_without_piece(const chess::Board& board, chess::Square square, chess::Color side,
                      chess::Square removed_square) {
    Position position(board);
    if (removed_square.is_valid()) {
        position.remove(removed_square);
    }
    return see_position(position, square, side);
}

void see_without_piece(const chess::Board& board, std::span<const chess::Square> squares,
                       chess::Color side, chess::Square removed_square, std::span<int> results) {
    assert(squares.size() == results.size());
    Position position(board);
    if (removed_square.is_valid()) {
        position.remove(removed_square);
    }
    for (std::size_t index = 0; index < squares.size(); ++index) {
        results[index] = see_position(position, squares[index], side);
    }
}

} // namespace chesswhy::tactics::detail
