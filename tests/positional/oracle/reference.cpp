#include "reference.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace chesswhy::positional_oracle {
namespace {

using chess::Color;
using chess::Piece;
using chess::PieceType;
using chess::Square;
using PawnsByFile = std::array<std::vector<Square>, 8>;
using PawnFilesBySide = std::array<PawnsByFile, 2>;

[[nodiscard]] Color other(Color side) noexcept {
    return side == Color::WHITE ? Color::BLACK : Color::WHITE;
}

[[nodiscard]] Side report_side(Color side) noexcept {
    return side == Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] std::size_t side_index(Color side) noexcept {
    return side == Color::WHITE ? 0U : 1U;
}

[[nodiscard]] int file_of(Square square) noexcept {
    return static_cast<int>(square.file());
}

[[nodiscard]] int rank_of(Square square) noexcept {
    return static_cast<int>(square.rank());
}

[[nodiscard]] int relative_rank(Square square, Color side) noexcept {
    return side == Color::WHITE ? rank_of(square) + 1 : 8 - rank_of(square);
}

[[nodiscard]] Square square_at(int file, int rank) noexcept {
    return {file + (rank * 8)};
}

[[nodiscard]] std::optional<Square> offset(Square square, int file_delta, int rank_delta) noexcept {
    const int file = file_of(square) + file_delta;
    const int rank = rank_of(square) + rank_delta;
    if (file < 0 || file >= 8 || rank < 0 || rank >= 8) {
        return std::nullopt;
    }
    return square_at(file, rank);
}

[[nodiscard]] bool is_slider(PieceType type) noexcept {
    return type == PieceType::BISHOP || type == PieceType::ROOK || type == PieceType::QUEEN;
}

[[nodiscard]] bool slider_matches(PieceType type, int file_delta, int rank_delta) noexcept {
    const int abs_file = file_delta < 0 ? -file_delta : file_delta;
    const int abs_rank = rank_delta < 0 ? -rank_delta : rank_delta;
    const bool diagonal = abs_file == abs_rank && abs_file != 0;
    const bool straight = (file_delta == 0) != (rank_delta == 0);
    return (diagonal && (type == PieceType::BISHOP || type == PieceType::QUEEN)) ||
           (straight && (type == PieceType::ROOK || type == PieceType::QUEEN));
}

[[nodiscard]] bool piece_attacks(const chess::Board& board, Square from, Square target) noexcept {
    const Piece piece = board.at(from);
    if (piece == Piece::NONE || from == target) {
        return false;
    }

    const int file_delta = file_of(target) - file_of(from);
    const int rank_delta = rank_of(target) - rank_of(from);
    const int abs_file = file_delta < 0 ? -file_delta : file_delta;
    const int abs_rank = rank_delta < 0 ? -rank_delta : rank_delta;

    if (piece.type() == PieceType::PAWN) {
        return abs_file == 1 && rank_delta == (piece.color() == Color::WHITE ? 1 : -1);
    }
    if (piece.type() == PieceType::KNIGHT) {
        return (abs_file == 1 && abs_rank == 2) || (abs_file == 2 && abs_rank == 1);
    }
    if (piece.type() == PieceType::KING) {
        return abs_file <= 1 && abs_rank <= 1;
    }
    if (!is_slider(piece.type()) || !slider_matches(piece.type(), file_delta, rank_delta)) {
        return false;
    }

    const int step_file = static_cast<int>(file_delta > 0) - static_cast<int>(file_delta < 0);
    const int step_rank = static_cast<int>(rank_delta > 0) - static_cast<int>(rank_delta < 0);
    int file = file_of(from) + step_file;
    int rank = rank_of(from) + step_rank;
    while (file != file_of(target) || rank != rank_of(target)) {
        if (board.at(square_at(file, rank)) != Piece::NONE) {
            return false;
        }
        file += step_file;
        rank += step_rank;
    }
    return true;
}

[[nodiscard]] PawnFilesBySide collect_pawns(const chess::Board& board) {
    PawnFilesBySide pawns;
    for (int index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = board.at(square);
        if (piece != Piece::NONE && piece.type() == PieceType::PAWN) {
            pawns[side_index(piece.color())][static_cast<std::size_t>(file_of(square))].push_back(
                square);
        }
    }
    return pawns;
}

[[nodiscard]] std::vector<Square> all_pawns(const PawnsByFile& pawns) {
    std::vector<Square> result;
    for (const auto& file : pawns) {
        result.insert(result.end(), file.begin(), file.end());
    }
    return result;
}

[[nodiscard]] std::optional<Square> pawn_front_square(Square pawn, Color side) noexcept {
    return offset(pawn, 0, side == Color::WHITE ? 1 : -1);
}

[[nodiscard]] bool pawn_attacks_square(const chess::Board& board, const std::vector<Square>& pawns,
                                       Square target) noexcept {
    return std::ranges::any_of(
        pawns, [&board, target](Square pawn) { return piece_attacks(board, pawn, target); });
}

[[nodiscard]] std::array<bool, 64> pawn_attack_map(const std::vector<Square>& pawns,
                                                   Color side) noexcept {
    std::array<bool, 64> attacked{};
    const int rank_delta = side == Color::WHITE ? 1 : -1;
    for (const Square pawn : pawns) {
        for (const int file_delta : {-1, 1}) {
            const auto target = offset(pawn, file_delta, rank_delta);
            if (target.has_value()) {
                attacked[static_cast<std::size_t>(target->index())] = true;
            }
        }
    }
    return attacked;
}
[[nodiscard]] PieceOnSquare printed_piece(const chess::Board& board, Square square) noexcept {
    constexpr std::array<char, 7> letters{'P', 'N', 'B', 'R', 'Q', 'K', ' '};
    const Piece piece = board.at(square);
    char letter = letters[static_cast<std::size_t>(piece.type())];
    if (piece.color() == Color::BLACK && letter != ' ') {
        letter = static_cast<char>((letter - 'A') + 'a');
    }
    return {.square = static_cast<std::uint8_t>(square.index()), .piece = letter};
}

[[nodiscard]] std::vector<PieceOnSquare> printed_pieces(const chess::Board& board,
                                                        const std::vector<Square>& squares) {
    std::vector<PieceOnSquare> result;
    result.reserve(squares.size());
    for (const Square square : squares) {
        result.push_back(printed_piece(board, square));
    }
    return result;
}

void append(std::vector<PositionalFeature>& features, PositionalTag tag, Color side,
            std::vector<PieceOnSquare> pieces = {}, std::optional<std::uint8_t> file = std::nullopt,
            std::optional<int> value = std::nullopt) {
    features.push_back({.tag = tag,
                        .side = report_side(side),
                        .pieces = std::move(pieces),
                        .file = file,
                        .value = value});
}

[[nodiscard]] int square_colour(Square square) noexcept {
    return (file_of(square) + rank_of(square)) % 2;
}

[[nodiscard]] bool feature_less(const PositionalFeature& left,
                                const PositionalFeature& right) noexcept {
    if (left.tag != right.tag) {
        return left.tag < right.tag;
    }
    if (left.side != right.side) {
        return left.side < right.side;
    }
    if (left.file != right.file) {
        return left.file < right.file;
    }
    const std::optional<std::uint8_t> left_first =
        left.pieces.empty() ? std::nullopt
                            : std::optional<std::uint8_t>(left.pieces.front().square);
    const std::optional<std::uint8_t> right_first =
        right.pieces.empty() ? std::nullopt
                             : std::optional<std::uint8_t>(right.pieces.front().square);
    if (left_first != right_first) {
        return left_first < right_first;
    }
    return left.value < right.value;
}

[[nodiscard]] int material(const chess::Board& board, Color side) noexcept {
    int total = 0;
    for (int index = 0; index < 64; ++index) {
        const Piece piece = board.at(Square(index));
        if (piece == Piece::NONE || piece.color() != side) {
            continue;
        }
        if (piece.type() == PieceType::PAWN) {
            total += 1;
        } else if (piece.type() == PieceType::KNIGHT || piece.type() == PieceType::BISHOP) {
            total += 3;
        } else if (piece.type() == PieceType::ROOK) {
            total += 5;
        } else if (piece.type() == PieceType::QUEEN) {
            total += 9;
        }
    }
    return total;
}

[[nodiscard]] int count_type(const chess::Board& board, Color side, PieceType first_type,
                             PieceType second_type = PieceType::NONE) noexcept {
    int count = 0;
    for (int index = 0; index < 64; ++index) {
        const Piece piece = board.at(Square(index));
        if (piece != Piece::NONE && piece.color() == side &&
            (piece.type() == first_type || piece.type() == second_type)) {
            ++count;
        }
    }
    return count;
}

} // namespace

int reference_mobility(const chess::Board& board, Color side) {
    const Color enemy = other(side);
    const PawnFilesBySide pawn_files = collect_pawns(board);
    const std::vector<Square> enemy_pawns = all_pawns(pawn_files[side_index(enemy)]);
    const std::array<bool, 64> enemy_pawn_attacks = pawn_attack_map(enemy_pawns, enemy);
    int total = 0;
    for (int from_index = 0; from_index < 64; ++from_index) {
        const Square from(from_index);
        const Piece piece = board.at(from);
        if (piece == Piece::NONE || piece.color() != side ||
            (piece.type() != PieceType::KNIGHT && piece.type() != PieceType::BISHOP &&
             piece.type() != PieceType::ROOK && piece.type() != PieceType::QUEEN)) {
            continue;
        }
        for (int target_index = 0; target_index < 64; ++target_index) {
            const Square target(target_index);
            const Piece occupant = board.at(target);
            if (!piece_attacks(board, from, target) ||
                (occupant != Piece::NONE && occupant.color() == side) ||
                enemy_pawn_attacks[static_cast<std::size_t>(target_index)]) {
                continue;
            }
            ++total;
        }
    }
    return total;
}

int reference_space(const chess::Board& board, Color side) {
    const Color enemy = other(side);
    const PawnFilesBySide pawn_files = collect_pawns(board);
    const std::vector<Square> enemy_pawns = all_pawns(pawn_files[side_index(enemy)]);
    const std::array<bool, 64> enemy_pawn_attacks = pawn_attack_map(enemy_pawns, enemy);
    int total = 0;
    for (int index = 0; index < 64; ++index) {
        const Square square(index);
        const int file = file_of(square);
        const int relative = relative_rank(square, side);
        if (file < 2 || file > 5 || relative < 2 || relative > 4 ||
            enemy_pawn_attacks[static_cast<std::size_t>(index)]) {
            continue;
        }
        const bool own_pawn_on_square =
            std::ranges::any_of(pawn_files[side_index(side)][static_cast<std::size_t>(file)],
                                [square](Square pawn) { return pawn == square; });
        const bool own_pawn_in_front =
            std::ranges::any_of(pawn_files[side_index(side)][static_cast<std::size_t>(file)],
                                [square, side](Square pawn) {
                                    return relative_rank(pawn, side) > relative_rank(square, side);
                                });
        if (!own_pawn_on_square && own_pawn_in_front) {
            ++total;
        }
    }
    return total;
}

int reference_undeveloped(const chess::Board& board, Color side) {
    int total = 0;
    for (int index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = board.at(square);
        if (piece != Piece::NONE && piece.color() == side &&
            (piece.type() == PieceType::KNIGHT || piece.type() == PieceType::BISHOP) &&
            relative_rank(square, side) == 1) {
            ++total;
        }
    }
    return total;
}

std::vector<PositionalFeature> reference_features(const chess::Board& board) {
    std::vector<PositionalFeature> features;
    const PawnFilesBySide pawn_files = collect_pawns(board);
    constexpr std::array<Color, 2> sides{Color::WHITE, Color::BLACK};
    std::array<int, 2> mobility_counts{};
    std::array<int, 2> space_counts{};
    std::array<int, 2> undeveloped_counts{};
    std::array<int, 2> material_counts{};
    std::array<int, 2> rook_counts{};
    std::array<int, 2> minor_piece_counts{};
    for (std::size_t index = 0; index < sides.size(); ++index) {
        const Color side = sides[index];
        mobility_counts[index] = reference_mobility(board, side);
        space_counts[index] = reference_space(board, side);
        undeveloped_counts[index] = reference_undeveloped(board, side);
        material_counts[index] = material(board, side);
        rook_counts[index] = count_type(board, side, PieceType::ROOK);
        minor_piece_counts[index] = count_type(board, side, PieceType::KNIGHT, PieceType::BISHOP);
    }

    for (const Color side : sides) {
        const Color enemy = other(side);
        const std::size_t own_index = side_index(side);
        const std::size_t enemy_index = side_index(enemy);
        const PawnsByFile& own_by_file = pawn_files[own_index];
        const PawnsByFile& enemy_by_file = pawn_files[enemy_index];
        const std::vector<Square> own_pawns = all_pawns(own_by_file);
        const std::vector<Square> enemy_pawns = all_pawns(enemy_by_file);

        for (int file = 0; file < 8; ++file) {
            const auto& pawns = own_by_file[static_cast<std::size_t>(file)];
            if (pawns.empty()) {
                continue;
            }
            const bool has_adjacent_pawn =
                (file > 0 && !own_by_file[static_cast<std::size_t>(file - 1)].empty()) ||
                (file < 7 && !own_by_file[static_cast<std::size_t>(file) + 1].empty());
            if (!has_adjacent_pawn) {
                append(features, PositionalTag::IsolatedPawn, side, printed_pieces(board, pawns),
                       static_cast<std::uint8_t>(file));
            }
            if (pawns.size() >= 2) {
                append(features, PositionalTag::DoubledPawns, side, printed_pieces(board, pawns),
                       static_cast<std::uint8_t>(file));
            }
            if (pawns.size() >= 3) {
                append(features, PositionalTag::TripledPawns, side, printed_pieces(board, pawns),
                       static_cast<std::uint8_t>(file));
            }

            std::vector<Square> backward;
            for (const Square pawn : pawns) {
                if (!has_adjacent_pawn) {
                    continue;
                }
                bool adjacent_pawn_at_or_behind = false;
                for (const int adjacent_file : {file - 1, file + 1}) {
                    if (adjacent_file < 0 || adjacent_file >= 8) {
                        continue;
                    }
                    for (const Square neighbor :
                         own_by_file[static_cast<std::size_t>(adjacent_file)]) {
                        if (relative_rank(neighbor, side) <= relative_rank(pawn, side)) {
                            adjacent_pawn_at_or_behind = true;
                        }
                    }
                }
                const auto stop = pawn_front_square(pawn, side);
                if (!adjacent_pawn_at_or_behind && stop.has_value() &&
                    pawn_attacks_square(board, enemy_pawns, *stop)) {
                    backward.push_back(pawn);
                }
            }
            if (!backward.empty()) {
                append(features, PositionalTag::BackwardPawn, side, printed_pieces(board, backward),
                       static_cast<std::uint8_t>(file));
            }
        }

        std::array<std::optional<Square>, 8> passed_by_file{};
        for (int file = 0; file < 8; ++file) {
            for (const Square pawn : own_by_file[static_cast<std::size_t>(file)]) {
                const int pawn_rank = relative_rank(pawn, side);
                const bool own_pawn_in_front =
                    std::ranges::any_of(own_by_file[static_cast<std::size_t>(file)],
                                        [pawn_rank, side](Square other_pawn) {
                                            return relative_rank(other_pawn, side) > pawn_rank;
                                        });
                if (own_pawn_in_front) {
                    continue;
                }
                bool enemy_pawn_in_front = false;
                for (const int enemy_file : {file - 1, file, file + 1}) {
                    if (enemy_file < 0 || enemy_file >= 8) {
                        continue;
                    }
                    enemy_pawn_in_front =
                        enemy_pawn_in_front ||
                        std::ranges::any_of(enemy_by_file[static_cast<std::size_t>(enemy_file)],
                                            [pawn_rank, side](Square enemy_pawn) {
                                                return relative_rank(enemy_pawn, side) > pawn_rank;
                                            });
                }
                if (!enemy_pawn_in_front) {
                    passed_by_file[static_cast<std::size_t>(file)] = pawn;
                    append(features, PositionalTag::PassedPawn, side,
                           printed_pieces(board, std::vector<Square>{pawn}),
                           static_cast<std::uint8_t>(file));
                }
            }
        }

        for (int file = 0; file < 8;) {
            if (!passed_by_file[static_cast<std::size_t>(file)].has_value()) {
                ++file;
                continue;
            }
            std::vector<Square> connected;
            while (file < 8 && passed_by_file[static_cast<std::size_t>(file)].has_value()) {
                connected.push_back(passed_by_file[static_cast<std::size_t>(file)].value());
                ++file;
            }
            if (connected.size() >= 2) {
                append(features, PositionalTag::ConnectedPassedPawns, side,
                       printed_pieces(board, connected));
            }
        }

        int islands = 0;
        bool in_island = false;
        for (const auto& pawns : own_by_file) {
            if (pawns.empty()) {
                in_island = false;
            } else if (!in_island) {
                ++islands;
                in_island = true;
            }
        }
        if (!own_pawns.empty()) {
            append(features, PositionalTag::PawnIslands, side, {}, std::nullopt, islands);
        }

        const int pawn_step = side == Color::WHITE ? 1 : -1;
        for (const Square base : own_pawns) {
            for (const int file_step : {-1, 1}) {
                const auto previous = offset(base, -file_step, -pawn_step);
                if (previous.has_value() && board.at(*previous) == Piece(PieceType::PAWN, side)) {
                    continue;
                }
                std::vector<Square> chain{base};
                Square current = base;
                while (true) {
                    const auto next = offset(current, file_step, pawn_step);
                    if (!next.has_value() || board.at(*next) != Piece(PieceType::PAWN, side)) {
                        break;
                    }
                    chain.push_back(*next);
                    current = *next;
                }
                if (chain.size() >= 3) {
                    append(features, PositionalTag::PawnChain, side, printed_pieces(board, chain));
                }
            }
        }

        const Square king = board.kingSq(side);
        const int king_file = file_of(king);
        const int king_rank = relative_rank(king, side);
        for (int file = std::max(0, king_file - 1); file <= std::min(7, king_file + 1); ++file) {
            if (king_rank <= 2) {
                const bool shield_present = std::ranges::any_of(
                    own_by_file[static_cast<std::size_t>(file)], [king_rank, side](Square pawn) {
                        const int rank = relative_rank(pawn, side);
                        return rank == king_rank + 1 || rank == king_rank + 2;
                    });
                if (!shield_present) {
                    append(features, PositionalTag::KingShieldGap, side,
                           printed_pieces(board, std::vector<Square>{king}),
                           static_cast<std::uint8_t>(file));
                }
            }

            const bool own_pawn_on_file = !own_by_file[static_cast<std::size_t>(file)].empty();
            const bool enemy_pawn_on_file = !enemy_by_file[static_cast<std::size_t>(file)].empty();
            if (!own_pawn_on_file && !enemy_pawn_on_file) {
                append(features, PositionalTag::OpenFileNearKing, side,
                       printed_pieces(board, std::vector<Square>{king}),
                       static_cast<std::uint8_t>(file));
            }
            if (!own_pawn_on_file && enemy_pawn_on_file) {
                append(features, PositionalTag::HalfOpenFileNearKing, side,
                       printed_pieces(board, std::vector<Square>{king}),
                       static_cast<std::uint8_t>(file));
            }
        }

        bool enemy_piece_attacks_king_zone = false;
        for (int from_index = 0; from_index < 64 && !enemy_piece_attacks_king_zone; ++from_index) {
            const Square from(from_index);
            const Piece attacker = board.at(from);
            if (attacker == Piece::NONE || attacker.color() != enemy ||
                (attacker.type() != PieceType::KNIGHT && attacker.type() != PieceType::BISHOP &&
                 attacker.type() != PieceType::ROOK && attacker.type() != PieceType::QUEEN)) {
                continue;
            }
            for (int target_index = 0; target_index < 64; ++target_index) {
                const Square target(target_index);
                if (std::max(std::abs(file_of(target) - king_file),
                             std::abs(rank_of(target) - rank_of(king))) <= 1 &&
                    piece_attacks(board, from, target)) {
                    enemy_piece_attacks_king_zone = true;
                    break;
                }
            }
        }
        if (enemy_piece_attacks_king_zone) {
            for (int index = 0; index < 64; ++index) {
                const Square attacker_square(index);
                const Piece attacker = board.at(attacker_square);
                if (attacker == Piece::NONE || attacker.color() != enemy ||
                    (attacker.type() != PieceType::KNIGHT && attacker.type() != PieceType::BISHOP &&
                     attacker.type() != PieceType::ROOK && attacker.type() != PieceType::QUEEN)) {
                    continue;
                }
                bool attacks_zone = false;
                for (int target_index = 0; target_index < 64 && !attacks_zone; ++target_index) {
                    const Square target(target_index);
                    if (std::max(std::abs(file_of(target) - king_file),
                                 std::abs(rank_of(target) - rank_of(king))) <= 1) {
                        attacks_zone = piece_attacks(board, attacker_square, target);
                    }
                }
                if (attacks_zone) {
                    append(features, PositionalTag::KingZoneAttacker, side,
                           printed_pieces(board, std::vector<Square>{attacker_square}));
                }
            }
        }

        const auto& castling = board.castlingRights();
        if (castling.has(side, chess::Board::CastlingRights::Side::KING_SIDE)) {
            append(features, PositionalTag::CastlingKingside, side);
        }
        if (castling.has(side, chess::Board::CastlingRights::Side::QUEEN_SIDE)) {
            append(features, PositionalTag::CastlingQueenside, side);
        }

        std::array<std::vector<Square>, 2> bishops_by_colour;
        for (int index = 0; index < 64; ++index) {
            const Square square(index);
            const Piece piece = board.at(square);
            if (piece != Piece::NONE && piece.color() == side &&
                piece.type() == PieceType::BISHOP) {
                bishops_by_colour[static_cast<std::size_t>(square_colour(square))].push_back(
                    square);
            }
        }
        if (!bishops_by_colour[0].empty() && !bishops_by_colour[1].empty()) {
            std::vector<Square> bishops = bishops_by_colour[0];
            bishops.insert(bishops.end(), bishops_by_colour[1].begin(), bishops_by_colour[1].end());
            std::ranges::sort(bishops, {}, [](Square square) { return square.index(); });
            append(features, PositionalTag::BishopPair, side, printed_pieces(board, bishops));
        }

        for (int file = 0; file < 8; ++file) {
            std::vector<Square> rooks;
            for (int index = 0; index < 64; ++index) {
                const Square square(index);
                const Piece piece = board.at(square);
                if (file_of(square) == file && piece != Piece::NONE && piece.color() == side &&
                    piece.type() == PieceType::ROOK) {
                    rooks.push_back(square);
                }
            }
            if (rooks.empty()) {
                continue;
            }
            const bool own_pawn = !own_by_file[static_cast<std::size_t>(file)].empty();
            const bool enemy_pawn = !enemy_by_file[static_cast<std::size_t>(file)].empty();
            if (!own_pawn && !enemy_pawn) {
                append(features, PositionalTag::RookOpenFile, side, printed_pieces(board, rooks),
                       static_cast<std::uint8_t>(file));
            }
            if (!own_pawn && enemy_pawn) {
                append(features, PositionalTag::RookHalfOpenFile, side,
                       printed_pieces(board, rooks), static_cast<std::uint8_t>(file));
            }
        }

        bool seventh_condition = relative_rank(board.kingSq(enemy), side) == 8;
        for (const Square pawn : enemy_pawns) {
            seventh_condition = seventh_condition || relative_rank(pawn, side) == 7;
        }
        if (seventh_condition) {
            std::vector<Square> rooks;
            for (int index = 0; index < 64; ++index) {
                const Square square(index);
                const Piece piece = board.at(square);
                if (piece != Piece::NONE && piece.color() == side &&
                    piece.type() == PieceType::ROOK && relative_rank(square, side) == 7) {
                    rooks.push_back(square);
                }
            }
            if (!rooks.empty()) {
                append(features, PositionalTag::RookSeventh, side, printed_pieces(board, rooks));
            }
        }

        for (int index = 0; index < 64; ++index) {
            const Square square(index);
            const Piece piece = board.at(square);
            if (piece == Piece::NONE || piece.color() != side ||
                (piece.type() != PieceType::KNIGHT && piece.type() != PieceType::BISHOP) ||
                relative_rank(square, side) < 4 || relative_rank(square, side) > 6) {
                continue;
            }
            const bool defended_by_pawn =
                std::ranges::any_of(own_pawns, [&board, square](Square pawn) {
                    return piece_attacks(board, pawn, square);
                });
            if (!defended_by_pawn) {
                continue;
            }
            bool enemy_pawn_ahead = false;
            for (const int adjacent_file : {file_of(square) - 1, file_of(square) + 1}) {
                if (adjacent_file < 0 || adjacent_file >= 8) {
                    continue;
                }
                enemy_pawn_ahead =
                    enemy_pawn_ahead ||
                    std::ranges::any_of(enemy_by_file[static_cast<std::size_t>(adjacent_file)],
                                        [square, side](Square pawn) {
                                            return relative_rank(pawn, side) >
                                                   relative_rank(square, side);
                                        });
            }
            if (!enemy_pawn_ahead) {
                append(features, PositionalTag::Outpost, side,
                       printed_pieces(board, std::vector<Square>{square}));
            }
        }

        std::array<std::vector<Square>, 2> own_pawns_by_colour;
        for (const Square pawn : own_pawns) {
            own_pawns_by_colour[static_cast<std::size_t>(square_colour(pawn))].push_back(pawn);
        }
        for (int colour = 0; colour < 2; ++colour) {
            if (own_pawns_by_colour[static_cast<std::size_t>(colour)].size() <=
                own_pawns_by_colour[static_cast<std::size_t>(1 - colour)].size()) {
                continue;
            }
            std::vector<Square> blocked_pawns;
            for (const Square pawn : own_pawns_by_colour[static_cast<std::size_t>(colour)]) {
                const auto front = pawn_front_square(pawn, side);
                if (front.has_value() && board.at(*front) == Piece(PieceType::PAWN, enemy)) {
                    blocked_pawns.push_back(pawn);
                }
            }
            std::ranges::sort(blocked_pawns, {}, [](Square square) { return square.index(); });
            if (blocked_pawns.size() < 2 ||
                bishops_by_colour[static_cast<std::size_t>(colour)].empty()) {
                continue;
            }
            std::vector<Square> pieces = bishops_by_colour[static_cast<std::size_t>(colour)];
            pieces.insert(pieces.end(), blocked_pawns.begin(), blocked_pawns.end());
            append(features, PositionalTag::BadBishop, side, printed_pieces(board, pieces));
        }

        const int mobility_difference = mobility_counts[own_index] - mobility_counts[enemy_index];
        if (mobility_difference >= 10) {
            append(features, PositionalTag::MobilityLead, side, {}, std::nullopt,
                   mobility_difference);
        }
        const int space_difference = space_counts[own_index] - space_counts[enemy_index];
        if (space_difference >= 3) {
            append(features, PositionalTag::SpaceLead, side, {}, std::nullopt, space_difference);
        }
        const int development_difference =
            undeveloped_counts[enemy_index] - undeveloped_counts[own_index];
        if (development_difference >= 2) {
            append(features, PositionalTag::DevelopmentLead, side, {}, std::nullopt,
                   development_difference);
        }

        const int material_difference = material_counts[own_index] - material_counts[enemy_index];
        if (material_difference >= 1) {
            append(features, PositionalTag::MaterialAhead, side, {}, std::nullopt,
                   material_difference);
        }
        if (rook_counts[own_index] > rook_counts[enemy_index] &&
            minor_piece_counts[enemy_index] > minor_piece_counts[own_index]) {
            append(features, PositionalTag::ExchangeUp, side);
        }
    }

    std::ranges::stable_sort(features, feature_less);
    return features;
}

bool same_identity(const PositionalFeature& left, const PositionalFeature& right) {
    if (left.tag != right.tag || left.side != right.side) {
        return false;
    }
    switch (left.tag) {
    case PositionalTag::IsolatedPawn:
    case PositionalTag::DoubledPawns:
    case PositionalTag::TripledPawns:
    case PositionalTag::BackwardPawn:
    case PositionalTag::PassedPawn:
    case PositionalTag::KingShieldGap:
    case PositionalTag::OpenFileNearKing:
    case PositionalTag::HalfOpenFileNearKing:
    case PositionalTag::RookOpenFile:
    case PositionalTag::RookHalfOpenFile:
        return left.file == right.file;
    case PositionalTag::ConnectedPassedPawns: {
        std::vector<std::uint8_t> left_files;
        std::vector<std::uint8_t> right_files;
        left_files.reserve(left.pieces.size());
        right_files.reserve(right.pieces.size());
        for (const PieceOnSquare piece : left.pieces) {
            left_files.push_back(static_cast<std::uint8_t>(piece.square % 8));
        }
        for (const PieceOnSquare piece : right.pieces) {
            right_files.push_back(static_cast<std::uint8_t>(piece.square % 8));
        }
        return left_files == right_files;
    }
    case PositionalTag::PawnIslands:
    case PositionalTag::MaterialAhead:
        return left.value == right.value;
    case PositionalTag::PawnChain:
        return left.pieces.size() == right.pieces.size() &&
               std::ranges::equal(left.pieces, right.pieces,
                                  [](const PieceOnSquare& a, const PieceOnSquare& b) {
                                      return a.square == b.square;
                                  });
    case PositionalTag::Outpost:
    case PositionalTag::KingZoneAttacker:
        return left.pieces.size() == 1 && right.pieces.size() == 1 &&
               left.pieces.front().square == right.pieces.front().square &&
               left.pieces.front().piece == right.pieces.front().piece;
    case PositionalTag::BadBishop:
        return !left.pieces.empty() && !right.pieces.empty() &&
               (left.pieces.front().square / 8 + left.pieces.front().square % 8) % 2 ==
                   (right.pieces.front().square / 8 + right.pieces.front().square % 8) % 2;
    case PositionalTag::CastlingKingside:
    case PositionalTag::CastlingQueenside:
    case PositionalTag::BishopPair:
    case PositionalTag::RookSeventh:
    case PositionalTag::MobilityLead:
    case PositionalTag::SpaceLead:
    case PositionalTag::DevelopmentLead:
    case PositionalTag::ExchangeUp:
        return true;
    }
    return false;
}

std::vector<PositionalChange> reference_line_changes(const chess::Board& root,
                                                     std::span<const chess::Move> line) {
    std::vector<std::vector<PositionalFeature>> positions;
    positions.reserve(line.size() + 1);
    positions.push_back(reference_features(root));
    chess::Board walked = root;
    for (const chess::Move move : line) {
        walked.makeMove(move);
        positions.push_back(reference_features(walked));
    }

    const auto& final_features = positions.back();
    std::vector<PositionalChange> changes;
    for (std::size_t ply = 1; ply < positions.size(); ++ply) {
        const auto& previous = positions[ply - 1];
        const auto& current = positions[ply];
        for (const PositionalFeature& feature : current) {
            const bool was_present =
                std::ranges::any_of(previous, [&feature](const PositionalFeature& old) {
                    return same_identity(feature, old);
                });
            if (was_present) {
                continue;
            }
            const bool holds_at_end = std::ranges::any_of(
                final_features, [&feature](const PositionalFeature& final_feature) {
                    return same_identity(feature, final_feature);
                });
            changes.push_back({.ply = static_cast<int>(ply),
                               .change = PositionalChangeKind::Appears,
                               .holds_at_end = holds_at_end,
                               .feature = feature});
        }
        for (const PositionalFeature& feature : previous) {
            const bool remains_present =
                std::ranges::any_of(current, [&feature](const PositionalFeature& next) {
                    return same_identity(feature, next);
                });
            if (remains_present) {
                continue;
            }
            const bool holds_at_end = !std::ranges::any_of(
                final_features, [&feature](const PositionalFeature& final_feature) {
                    return same_identity(feature, final_feature);
                });
            changes.push_back({.ply = static_cast<int>(ply),
                               .change = PositionalChangeKind::Disappears,
                               .holds_at_end = holds_at_end,
                               .feature = feature});
        }
    }

    std::ranges::stable_sort(changes,
                             [](const PositionalChange& left, const PositionalChange& right) {
                                 if (left.ply != right.ply) {
                                     return left.ply < right.ply;
                                 }
                                 if (left.change != right.change) {
                                     return left.change < right.change;
                                 }
                                 return feature_less(left.feature, right.feature);
                             });
    return changes;
}

} // namespace chesswhy::positional_oracle
