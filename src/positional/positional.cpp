#include "chesswhy/positional.hpp"

#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

namespace chesswhy::positional {
namespace {

using Bits = std::uint64_t;
using SquareList = std::vector<chess::Square>;
using PawnsByFile = std::array<SquareList, 8>;

constexpr std::array kColors{chess::Color::WHITE, chess::Color::BLACK};
constexpr std::array kPieceTypes{chess::PieceType::PAWN, chess::PieceType::KNIGHT,
                                 chess::PieceType::BISHOP, chess::PieceType::ROOK,
                                 chess::PieceType::QUEEN};

[[nodiscard]] Side report_side(chess::Color color) {
    return color == chess::Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] int relative_rank(chess::Square square, chess::Color side) {
    return side == chess::Color::WHITE ? static_cast<int>(square.rank()) + 1
                                       : 8 - static_cast<int>(square.rank());
}

[[nodiscard]] Bits square_bit(chess::Square square) {
    return Bits{1} << static_cast<unsigned>(square.index());
}

[[nodiscard]] SquareList squares(Bits bits) {
    SquareList result;
    result.reserve(static_cast<std::size_t>(std::popcount(bits)));
    while (bits != 0) {
        const int index = std::countr_zero(bits);
        bits &= bits - 1;
        result.emplace_back(index);
    }
    return result;
}

[[nodiscard]] chess::Square offset(chess::Square square, int file_delta, int rank_delta) {
    const int file = static_cast<int>(square.file()) + file_delta;
    const int rank = static_cast<int>(square.rank()) + rank_delta;
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return chess::Square::NO_SQ;
    }
    return chess::Square{(rank * 8) + file};
}

[[nodiscard]] char piece_letter(const chess::Board& board, chess::Square square) {
    return static_cast<std::string>(board.at(square)).front();
}

[[nodiscard]] PieceOnSquare report_piece(const chess::Board& board, chess::Square square) {
    return PieceOnSquare{.square = static_cast<std::uint8_t>(square.index()),
                         .piece = piece_letter(board, square)};
}

[[nodiscard]] PositionalFeature make_feature(const chess::Board& board, PositionalTag tag,
                                             chess::Color side,
                                             const SquareList& piece_squares = {},
                                             std::optional<std::uint8_t> file = std::nullopt,
                                             std::optional<int> value = std::nullopt) {
    PositionalFeature result;
    result.tag = tag;
    result.side = report_side(side);
    result.file = file;
    result.value = value;
    result.pieces.reserve(piece_squares.size());
    for (const chess::Square square : piece_squares) {
        result.pieces.push_back(report_piece(board, square));
    }
    return result;
}

[[nodiscard]] Bits piece_bits(const chess::Board& board, chess::PieceType type,
                              chess::Color color) {
    return board.pieces(type, color).getBits();
}

[[nodiscard]] Bits pawn_attack_bits(const chess::Board& board, chess::Color color) {
    Bits attacks = 0;
    for (const chess::Square pawn : squares(piece_bits(board, chess::PieceType::PAWN, color))) {
        attacks |= chess::attacks::pawn(color, pawn).getBits();
    }
    return attacks;
}

[[nodiscard]] Bits attacks_from(const chess::Board& board, chess::Square origin) {
    const chess::Piece piece = board.at(origin);
    switch (piece.type().internal()) {
    case chess::PieceType::PAWN:
        return chess::attacks::pawn(piece.color(), origin).getBits();
    case chess::PieceType::KNIGHT:
        return chess::attacks::knight(origin).getBits();
    case chess::PieceType::BISHOP:
        return chess::attacks::bishop(origin, board.occ()).getBits();
    case chess::PieceType::ROOK:
        return chess::attacks::rook(origin, board.occ()).getBits();
    case chess::PieceType::QUEEN:
        return chess::attacks::queen(origin, board.occ()).getBits();
    case chess::PieceType::KING:
        return chess::attacks::king(origin).getBits();
    default:
        return 0;
    }
}

[[nodiscard]] PawnsByFile pawns_by_file(const chess::Board& board, chess::Color side) {
    PawnsByFile result;
    for (const chess::Square pawn : squares(piece_bits(board, chess::PieceType::PAWN, side))) {
        result[static_cast<std::size_t>(pawn.file())].push_back(pawn);
    }
    return result;
}

[[nodiscard]] bool has_pawn_in_front(const SquareList& pawns, chess::Square square,
                                     chess::Color side) {
    return std::ranges::any_of(pawns, [square, side](chess::Square pawn) {
        return pawn.file() == square.file() &&
               relative_rank(pawn, side) > relative_rank(square, side);
    });
}

[[nodiscard]] bool square_is_light(chess::Square square) {
    return ((static_cast<int>(square.file()) + static_cast<int>(square.rank())) & 1) != 0;
}

[[nodiscard]] bool feature_less(const PositionalFeature& left, const PositionalFeature& right) {
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
        left.pieces.empty() ? std::nullopt : std::optional(left.pieces.front().square);
    const std::optional<std::uint8_t> right_first =
        right.pieces.empty() ? std::nullopt : std::optional(right.pieces.front().square);
    if (left_first != right_first) {
        return left_first < right_first;
    }
    return left.value < right.value;
}

void add_pawn_features(const chess::Board& board, chess::Color side,
                       std::vector<PositionalFeature>& result) {
    const chess::Color enemy = ~side;
    const PawnsByFile own_by_file = pawns_by_file(board, side);
    const PawnsByFile enemy_by_file = pawns_by_file(board, enemy);
    const Bits enemy_pawn_attacks = pawn_attack_bits(board, enemy);
    std::array<std::optional<chess::Square>, 8> passed_by_file{};
    int islands = 0;
    bool in_island = false;

    for (std::size_t file = 0; file < own_by_file.size(); ++file) {
        const SquareList& own = own_by_file[file];
        if (!own.empty()) {
            if (!in_island) {
                ++islands;
            }
            in_island = true;
        } else {
            in_island = false;
        }
        if (own.empty()) {
            continue;
        }

        const bool left_has_pawn = file > 0 && !own_by_file[file - 1].empty();
        const bool right_has_pawn = file + 1 < own_by_file.size() && !own_by_file[file + 1].empty();
        const bool isolated = !left_has_pawn && !right_has_pawn;
        if (isolated) {
            result.push_back(make_feature(board, PositionalTag::IsolatedPawn, side, own,
                                          static_cast<std::uint8_t>(file)));
        }
        if (own.size() >= 2) {
            result.push_back(make_feature(board, PositionalTag::DoubledPawns, side, own,
                                          static_cast<std::uint8_t>(file)));
        }

        SquareList backward;
        for (const chess::Square pawn : own) {
            if (isolated) {
                continue;
            }
            const int pawn_rank = relative_rank(pawn, side);
            bool has_neighbor_at_or_behind = false;
            for (const std::size_t neighbor_file :
                 {file == 0 ? file : file - 1, file + 1 < own_by_file.size() ? file + 1 : file}) {
                if (neighbor_file == file) {
                    continue;
                }
                has_neighbor_at_or_behind =
                    has_neighbor_at_or_behind ||
                    std::ranges::any_of(own_by_file[neighbor_file],
                                        [side, pawn_rank](chess::Square neighbor) {
                                            return relative_rank(neighbor, side) <= pawn_rank;
                                        });
            }
            if (has_neighbor_at_or_behind) {
                continue;
            }
            const chess::Square stop = offset(pawn, 0, side == chess::Color::WHITE ? 1 : -1);
            if (stop != chess::Square::NO_SQ && (enemy_pawn_attacks & square_bit(stop)) != 0) {
                backward.push_back(pawn);
            }
        }
        if (!backward.empty()) {
            result.push_back(make_feature(board, PositionalTag::BackwardPawn, side, backward,
                                          static_cast<std::uint8_t>(file)));
        }
    }

    if (islands > 0) {
        result.push_back(
            make_feature(board, PositionalTag::PawnIslands, side, {}, std::nullopt, islands));
    }

    for (std::size_t file = 0; file < own_by_file.size(); ++file) {
        for (const chess::Square pawn : own_by_file[file]) {
            bool passed = true;
            const int pawn_rank = relative_rank(pawn, side);
            for (std::size_t enemy_file = file == 0 ? file : file - 1;
                 enemy_file <= std::min(file + 1, enemy_by_file.size() - 1); ++enemy_file) {
                if (std::ranges::any_of(enemy_by_file[enemy_file],
                                        [side, pawn_rank](chess::Square other) {
                                            return relative_rank(other, side) > pawn_rank;
                                        })) {
                    passed = false;
                    break;
                }
            }
            if (passed && !has_pawn_in_front(own_by_file[file], pawn, side)) {
                passed_by_file[file] = pawn;
                result.push_back(make_feature(board, PositionalTag::PassedPawn, side, {pawn},
                                              static_cast<std::uint8_t>(file)));
            }
        }
    }

    std::size_t file = 0;
    while (file < passed_by_file.size()) {
        if (!passed_by_file[file]) {
            ++file;
            continue;
        }
        SquareList run;
        while (file < passed_by_file.size() && passed_by_file[file]) {
            run.push_back(*passed_by_file[file]);
            ++file;
        }
        if (run.size() >= 2) {
            result.push_back(make_feature(board, PositionalTag::ConnectedPassedPawns, side, run));
        }
    }

    const int forward = side == chess::Color::WHITE ? 1 : -1;
    for (const int file_step : {-1, 1}) {
        for (const SquareList& file_pawns : own_by_file) {
            for (const chess::Square base : file_pawns) {
                const chess::Square behind = offset(base, -file_step, -forward);
                if (behind != chess::Square::NO_SQ &&
                    board.at(behind) == chess::Piece(chess::PieceType::PAWN, side)) {
                    continue;
                }
                SquareList chain{base};
                chess::Square next = offset(base, file_step, forward);
                while (next != chess::Square::NO_SQ &&
                       board.at(next) == chess::Piece(chess::PieceType::PAWN, side)) {
                    chain.push_back(next);
                    next = offset(next, file_step, forward);
                }
                if (chain.size() >= 3) {
                    result.push_back(make_feature(board, PositionalTag::PawnChain, side, chain));
                }
            }
        }
    }
}

void add_king_features(const chess::Board& board, chess::Color side,
                       std::vector<PositionalFeature>& result) {
    using CastleSide = chess::Board::CastlingRights::Side;
    const auto castling = board.castlingRights();
    if (castling.has(side, CastleSide::KING_SIDE)) {
        result.push_back(make_feature(board, PositionalTag::CastlingKingside, side));
    }
    if (castling.has(side, CastleSide::QUEEN_SIDE)) {
        result.push_back(make_feature(board, PositionalTag::CastlingQueenside, side));
    }

    const chess::Square king = board.kingSq(side);
    const int king_rank = relative_rank(king, side);
    const int king_file = static_cast<int>(king.file());
    const SquareList king_piece{king};
    const PawnsByFile own_pawns = pawns_by_file(board, side);
    const PawnsByFile enemy_pawns = pawns_by_file(board, ~side);

    for (int file = std::max(0, king_file - 1); file <= std::min(7, king_file + 1); ++file) {
        const auto file_index = static_cast<std::size_t>(file);
        if (king_rank <= 2) {
            const bool shielded =
                std::ranges::any_of(own_pawns[file_index], [side, king_rank](chess::Square pawn) {
                    const int rank = relative_rank(pawn, side);
                    return rank == king_rank + 1 || rank == king_rank + 2;
                });
            if (!shielded) {
                result.push_back(make_feature(board, PositionalTag::KingShieldGap, side, king_piece,
                                              static_cast<std::uint8_t>(file)));
            }
        }
        if (own_pawns[file_index].empty() && enemy_pawns[file_index].empty()) {
            result.push_back(make_feature(board, PositionalTag::OpenFileNearKing, side, king_piece,
                                          static_cast<std::uint8_t>(file)));
        } else if (own_pawns[file_index].empty() && !enemy_pawns[file_index].empty()) {
            result.push_back(make_feature(board, PositionalTag::HalfOpenFileNearKing, side,
                                          king_piece, static_cast<std::uint8_t>(file)));
        }
    }

    const Bits king_zone = chess::attacks::king(king).getBits() | square_bit(king);
    for (const chess::PieceType type : {chess::PieceType::KNIGHT, chess::PieceType::BISHOP,
                                        chess::PieceType::ROOK, chess::PieceType::QUEEN}) {
        for (const chess::Square attacker : squares(piece_bits(board, type, ~side))) {
            if ((attacks_from(board, attacker) & king_zone) != 0) {
                result.push_back(
                    make_feature(board, PositionalTag::KingZoneAttacker, side, {attacker}));
            }
        }
    }
}

void add_piece_features(const chess::Board& board, chess::Color side,
                        std::vector<PositionalFeature>& result) {
    const chess::Color enemy = ~side;
    const Bits own_pawns = piece_bits(board, chess::PieceType::PAWN, side);
    const Bits enemy_pawns = piece_bits(board, chess::PieceType::PAWN, enemy);
    const Bits own_pawn_attacks = pawn_attack_bits(board, side);
    const SquareList own_pawn_squares = squares(own_pawns);
    const PawnsByFile own_by_file = pawns_by_file(board, side);
    const PawnsByFile enemy_by_file = pawns_by_file(board, enemy);

    const SquareList bishops = squares(piece_bits(board, chess::PieceType::BISHOP, side));
    bool has_light_bishop = false;
    bool has_dark_bishop = false;
    for (const chess::Square bishop : bishops) {
        has_light_bishop = has_light_bishop || square_is_light(bishop);
        has_dark_bishop = has_dark_bishop || !square_is_light(bishop);
    }
    if (has_light_bishop && has_dark_bishop) {
        result.push_back(make_feature(board, PositionalTag::BishopPair, side, bishops));
    }

    const SquareList rooks = squares(piece_bits(board, chess::PieceType::ROOK, side));
    for (int file = 0; file < 8; ++file) {
        SquareList rooks_on_file;
        for (const chess::Square rook : rooks) {
            if (static_cast<int>(rook.file()) == file) {
                rooks_on_file.push_back(rook);
            }
        }
        if (rooks_on_file.empty()) {
            continue;
        }
        const auto file_index = static_cast<std::size_t>(file);
        const bool own_pawn_on_file = !own_by_file[file_index].empty();
        const bool enemy_pawn_on_file = !enemy_by_file[file_index].empty();
        if (!own_pawn_on_file && !enemy_pawn_on_file) {
            result.push_back(make_feature(board, PositionalTag::RookOpenFile, side, rooks_on_file,
                                          static_cast<std::uint8_t>(file)));
        } else if (!own_pawn_on_file && enemy_pawn_on_file) {
            result.push_back(make_feature(board, PositionalTag::RookHalfOpenFile, side,
                                          rooks_on_file, static_cast<std::uint8_t>(file)));
        }
    }

    const chess::Square enemy_king = board.kingSq(enemy);
    const bool enemy_king_on_eighth = relative_rank(enemy_king, side) == 8;
    bool enemy_pawn_on_seventh = false;
    for (const chess::Square pawn : squares(enemy_pawns)) {
        enemy_pawn_on_seventh = enemy_pawn_on_seventh || relative_rank(pawn, side) == 7;
    }
    if (enemy_king_on_eighth || enemy_pawn_on_seventh) {
        SquareList seventh_rooks;
        for (const chess::Square rook : rooks) {
            if (relative_rank(rook, side) == 7) {
                seventh_rooks.push_back(rook);
            }
        }
        if (!seventh_rooks.empty()) {
            result.push_back(make_feature(board, PositionalTag::RookSeventh, side, seventh_rooks));
        }
    }

    for (const chess::PieceType type : {chess::PieceType::KNIGHT, chess::PieceType::BISHOP}) {
        for (const chess::Square piece : squares(piece_bits(board, type, side))) {
            const int rank = relative_rank(piece, side);
            if (rank < 4 || rank > 6 || (own_pawn_attacks & square_bit(piece)) == 0) {
                continue;
            }
            bool challenged = false;
            for (int enemy_file = std::max(0, static_cast<int>(piece.file()) - 1);
                 enemy_file <= std::min(7, static_cast<int>(piece.file()) + 1); ++enemy_file) {
                if (enemy_file == static_cast<int>(piece.file())) {
                    continue;
                }
                challenged = challenged || std::ranges::any_of(
                                               enemy_by_file[static_cast<std::size_t>(enemy_file)],
                                               [side, rank](chess::Square pawn) {
                                                   return relative_rank(pawn, side) > rank;
                                               });
            }
            if (!challenged) {
                result.push_back(make_feature(board, PositionalTag::Outpost, side, {piece}));
            }
        }
    }

    std::array<int, 2> pawn_color_counts{};
    for (const chess::Square pawn : own_pawn_squares) {
        ++pawn_color_counts[static_cast<std::size_t>(square_is_light(pawn))];
    }
    for (const bool bishop_color : {false, true}) {
        const std::size_t color_index = static_cast<std::size_t>(bishop_color);
        const std::size_t other_index = 1U - color_index;
        if (pawn_color_counts[color_index] <= pawn_color_counts[other_index]) {
            continue;
        }
        SquareList same_color_bishops;
        std::copy_if(bishops.begin(), bishops.end(), std::back_inserter(same_color_bishops),
                     [bishop_color](chess::Square bishop) {
                         return square_is_light(bishop) == bishop_color;
                     });
        if (same_color_bishops.empty()) {
            continue;
        }
        SquareList blocked_pawns;
        for (const chess::Square pawn : own_pawn_squares) {
            if (square_is_light(pawn) != bishop_color) {
                continue;
            }
            const chess::Square front = offset(pawn, 0, side == chess::Color::WHITE ? 1 : -1);
            if (front != chess::Square::NO_SQ &&
                board.at(front) == chess::Piece(chess::PieceType::PAWN, enemy)) {
                blocked_pawns.push_back(pawn);
            }
        }
        if (blocked_pawns.size() >= 2) {
            same_color_bishops.insert(same_color_bishops.end(), blocked_pawns.begin(),
                                      blocked_pawns.end());
            result.push_back(
                make_feature(board, PositionalTag::BadBishop, side, same_color_bishops));
        }
    }
}

[[nodiscard]] int material(const chess::Board& board, chess::Color side) {
    int total = 0;
    for (const chess::PieceType type : kPieceTypes) {
        total += tactics::piece_value(type) * std::popcount(piece_bits(board, type, side));
    }
    return total;
}

[[nodiscard]] int count_pieces(const chess::Board& board, chess::PieceType type,
                               chess::Color side) {
    return std::popcount(piece_bits(board, type, side));
}

[[nodiscard]] int undeveloped_count(const chess::Board& board, chess::Color side) {
    const int home_rank = side == chess::Color::WHITE ? 0 : 7;
    const Bits home_rank_mask = Bits{0xff} << static_cast<unsigned>(home_rank * 8);
    const Bits minor_pieces = piece_bits(board, chess::PieceType::KNIGHT, side) |
                              piece_bits(board, chess::PieceType::BISHOP, side);
    return std::popcount(minor_pieces & home_rank_mask);
}

void add_leads_and_material(const chess::Board& board, std::vector<PositionalFeature>& result) {
    const std::array mobility_counts{mobility(board, chess::Color::WHITE),
                                     mobility(board, chess::Color::BLACK)};
    const std::array space_counts{space(board, chess::Color::WHITE),
                                  space(board, chess::Color::BLACK)};
    const std::array undeveloped_counts{undeveloped(board, chess::Color::WHITE),
                                        undeveloped(board, chess::Color::BLACK)};
    const std::array material_counts{material(board, chess::Color::WHITE),
                                     material(board, chess::Color::BLACK)};
    const std::array rook_counts{count_pieces(board, chess::PieceType::ROOK, chess::Color::WHITE),
                                 count_pieces(board, chess::PieceType::ROOK, chess::Color::BLACK)};
    const std::array minor_counts{
        count_pieces(board, chess::PieceType::KNIGHT, chess::Color::WHITE) +
            count_pieces(board, chess::PieceType::BISHOP, chess::Color::WHITE),
        count_pieces(board, chess::PieceType::KNIGHT, chess::Color::BLACK) +
            count_pieces(board, chess::PieceType::BISHOP, chess::Color::BLACK)};

    for (const chess::Color side : kColors) {
        const std::size_t side_index = side == chess::Color::WHITE ? 0 : 1;
        const std::size_t enemy_index = 1U - side_index;
        const int mobility_difference = mobility_counts[side_index] - mobility_counts[enemy_index];
        if (mobility_difference >= kMobilityLead) {
            result.push_back(make_feature(board, PositionalTag::MobilityLead, side, {},
                                          std::nullopt, mobility_difference));
        }
        const int space_difference = space_counts[side_index] - space_counts[enemy_index];
        if (space_difference >= kSpaceLead) {
            result.push_back(make_feature(board, PositionalTag::SpaceLead, side, {}, std::nullopt,
                                          space_difference));
        }
        const int development_difference =
            undeveloped_counts[enemy_index] - undeveloped_counts[side_index];
        if (development_difference >= kDevelopmentLead) {
            result.push_back(make_feature(board, PositionalTag::DevelopmentLead, side, {},
                                          std::nullopt, development_difference));
        }

        const int material_difference = material_counts[side_index] - material_counts[enemy_index];
        if (material_difference >= 1) {
            result.push_back(make_feature(board, PositionalTag::MaterialAhead, side, {},
                                          std::nullopt, material_difference));
        }
        if (rook_counts[side_index] > rook_counts[enemy_index] &&
            minor_counts[enemy_index] > minor_counts[side_index]) {
            result.push_back(make_feature(board, PositionalTag::ExchangeUp, side));
        }
    }
}

[[nodiscard]] std::optional<bool> bishop_square_color(const PositionalFeature& feature) {
    if (feature.pieces.empty()) {
        return std::nullopt;
    }
    return square_is_light(chess::Square(feature.pieces.front().square));
}

[[nodiscard]] bool same_piece_squares(const std::vector<PieceOnSquare>& left,
                                      const std::vector<PieceOnSquare>& right, bool compare_piece) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].square != right[index].square ||
            (compare_piece && left[index].piece != right[index].piece)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool contains_identity(const std::vector<PositionalFeature>& features,
                                     const PositionalFeature& target) {
    return std::ranges::any_of(features, [&target](const PositionalFeature& candidate) {
        return same_identity(candidate, target);
    });
}

} // namespace

int mobility(const chess::Board& board, chess::Color side) {
    const Bits own_pieces = board.us(side).getBits();
    const Bits enemy_pawn_attacks = pawn_attack_bits(board, ~side);
    int result = 0;
    for (const chess::PieceType type : {chess::PieceType::KNIGHT, chess::PieceType::BISHOP,
                                        chess::PieceType::ROOK, chess::PieceType::QUEEN}) {
        for (const chess::Square piece : squares(piece_bits(board, type, side))) {
            const Bits available = attacks_from(board, piece) & ~own_pieces & ~enemy_pawn_attacks;
            result += std::popcount(available);
        }
    }
    return result;
}

int space(const chess::Board& board, chess::Color side) {
    const Bits own_pawns = piece_bits(board, chess::PieceType::PAWN, side);
    std::array<int, 8> frontmost_pawn_rank{};
    Bits pawns_remaining = own_pawns;
    while (pawns_remaining != 0) {
        const int index = std::countr_zero(pawns_remaining);
        pawns_remaining &= pawns_remaining - 1;
        const chess::Square pawn(index);
        const std::size_t file = static_cast<std::size_t>(pawn.file());
        frontmost_pawn_rank[file] = std::max(frontmost_pawn_rank[file], relative_rank(pawn, side));
    }
    const Bits enemy_pawn_attacks = pawn_attack_bits(board, ~side);
    int result = 0;
    for (int file = 2; file <= 5; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const chess::Square square{chess::File(file), chess::Rank(rank)};
            const int relative = relative_rank(square, side);
            const std::size_t file_index = static_cast<std::size_t>(file);
            if (relative >= 2 && relative <= 4 && (own_pawns & square_bit(square)) == 0 &&
                (enemy_pawn_attacks & square_bit(square)) == 0 &&
                frontmost_pawn_rank[file_index] > relative) {
                ++result;
            }
        }
    }
    return result;
}

int undeveloped(const chess::Board& board, chess::Color side) {
    return undeveloped_count(board, side);
}

std::vector<PositionalFeature> features(const chess::Board& board) {
    std::vector<PositionalFeature> result;
    for (const chess::Color side : kColors) {
        add_pawn_features(board, side, result);
        add_king_features(board, side, result);
        add_piece_features(board, side, result);
    }
    add_leads_and_material(board, result);
    std::ranges::stable_sort(result, feature_less);
    return result;
}

bool same_identity(const PositionalFeature& a, const PositionalFeature& b) {
    if (a.tag != b.tag || a.side != b.side) {
        return false;
    }
    switch (a.tag) {
    case PositionalTag::IsolatedPawn:
    case PositionalTag::DoubledPawns:
    case PositionalTag::BackwardPawn:
    case PositionalTag::PassedPawn:
    case PositionalTag::KingShieldGap:
    case PositionalTag::OpenFileNearKing:
    case PositionalTag::HalfOpenFileNearKing:
    case PositionalTag::RookOpenFile:
    case PositionalTag::RookHalfOpenFile:
        return a.file == b.file;
    case PositionalTag::ConnectedPassedPawns:
        if (a.pieces.size() != b.pieces.size()) {
            return false;
        }
        for (std::size_t index = 0; index < a.pieces.size(); ++index) {
            if (chess::Square(a.pieces[index].square).file() !=
                chess::Square(b.pieces[index].square).file()) {
                return false;
            }
        }
        return true;
    case PositionalTag::PawnIslands:
    case PositionalTag::MaterialAhead:
        return a.value == b.value;
    case PositionalTag::PawnChain:
        return same_piece_squares(a.pieces, b.pieces, false);
    case PositionalTag::Outpost:
    case PositionalTag::KingZoneAttacker:
        return same_piece_squares(a.pieces, b.pieces, true);
    case PositionalTag::BadBishop:
        return bishop_square_color(a) == bishop_square_color(b);
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

std::vector<PositionalChange> line_changes(const chess::Board& root,
                                           std::span<const chess::Move> line) {
    chess::Board board = root;
    std::vector<PositionalFeature> previous = features(board);
    std::vector<PositionalChange> changes;
    for (std::size_t index = 0; index < line.size(); ++index) {
        board.makeMove(line[index]);
        std::vector<PositionalFeature> current = features(board);
        const int ply = static_cast<int>(index) + 1;
        for (const PositionalFeature& feature : current) {
            if (!contains_identity(previous, feature)) {
                changes.push_back(PositionalChange{.ply = ply,
                                                   .change = PositionalChangeKind::Appears,
                                                   .holds_at_end = false,
                                                   .feature = feature});
            }
        }
        for (const PositionalFeature& feature : previous) {
            if (!contains_identity(current, feature)) {
                changes.push_back(PositionalChange{.ply = ply,
                                                   .change = PositionalChangeKind::Disappears,
                                                   .holds_at_end = false,
                                                   .feature = feature});
            }
        }
        previous = std::move(current);
    }

    for (PositionalChange& change : changes) {
        const bool present_at_end = contains_identity(previous, change.feature);
        change.holds_at_end =
            change.change == PositionalChangeKind::Appears ? present_at_end : !present_at_end;
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

} // namespace chesswhy::positional
