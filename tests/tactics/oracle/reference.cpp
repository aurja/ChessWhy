#include "oracle/reference.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace chesswhy::oracle {
namespace {

using chess::Color;
using chess::Move;
using chess::Piece;
using chess::PieceType;
using chess::Square;

constexpr std::array<std::pair<int, int>, 8> kDirections{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
    {1, 1},
    {1, -1},
    {-1, 1},
    {-1, -1},
}};

class EditableBoard : public chess::Board {
public:
    explicit EditableBoard(const chess::Board& board) : chess::Board(board) {}

    void erase(Square square) {
        const Piece piece = at(square);
        if (piece != Piece::NONE) {
            removePiece(piece, square);
        }
    }
    void erase_for_pin(Square square) {
        const Piece piece = at(square);
        bool metadata_changed = false;
        if (piece.type() == PieceType::ROOK &&
            square.rank() == (piece.color() == Color::WHITE ? chess::Rank::underlying::RANK_1
                                                            : chess::Rank::underlying::RANK_8)) {
            constexpr std::array sides{chess::Board::CastlingRights::Side::KING_SIDE,
                                       chess::Board::CastlingRights::Side::QUEEN_SIDE};
            for (const auto castle : sides) {
                if (castlingRights().getRookFile(piece.color(), castle) == square.file()) {
                    cr_.clear(piece.color(), castle);
                    metadata_changed = true;
                }
            }
        }
        if (piece.type() == PieceType::PAWN && ep_sq_ != Square::NO_SQ &&
            ep_sq_.ep_square() == square) {
            ep_sq_ = Square::NO_SQ;
            metadata_changed = true;
        }
        erase(square);
        if (metadata_changed) {
            key_ = zobrist();
        }
    }
    void relocate(Square from, Square to) {
        const Piece moving = at(from);
        const Piece captured = at(to);
        removePiece(moving, from);
        if (captured != Piece::NONE) {
            removePiece(captured, to);
        }
        placePiece(moving, to);
    }
    void make_hypothetical_capture(Move move) {
        erase_for_pin(move.to());
        key_ = zobrist();
        makeMove(move);
    }
};

[[nodiscard]] Color other(Color color) noexcept {
    return color == Color::WHITE ? Color::BLACK : Color::WHITE;
}

[[nodiscard]] Side report_side(Color color) noexcept {
    return color == Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] char fen_piece(Piece piece) noexcept {
    constexpr std::array<char, 7> letters{'P', 'N', 'B', 'R', 'Q', 'K', ' '};
    char letter = letters[static_cast<std::size_t>(piece.type())];
    if (piece.color() == Color::BLACK && letter != ' ') {
        letter = static_cast<char>(letter - 'A' + 'a');
    }
    return letter;
}

[[nodiscard]] PieceOnSquare printed_piece(const chess::Board& board, Square square) noexcept {
    return {.square = static_cast<std::uint8_t>(square.index()),
            .piece = fen_piece(board.at(square))};
}

[[nodiscard]] bool is_slider(PieceType type) noexcept {
    return type == PieceType::BISHOP || type == PieceType::ROOK || type == PieceType::QUEEN;
}

[[nodiscard]] bool slider_matches(PieceType type, int df, int dr) noexcept {
    const int abs_file = df < 0 ? -df : df;
    const int abs_rank = dr < 0 ? -dr : dr;
    const bool diagonal = abs_file == abs_rank && abs_file != 0;
    const bool straight = (df == 0) != (dr == 0);
    return (diagonal && (type == PieceType::BISHOP || type == PieceType::QUEEN)) ||
           (straight && (type == PieceType::ROOK || type == PieceType::QUEEN));
}

[[nodiscard]] bool piece_attacks(const chess::Board& board, Square from, Square target) noexcept {
    const Piece piece = board.at(from);
    if (piece == Piece::NONE || from == target) {
        return false;
    }
    const int df = target.file() - from.file();
    const int dr = target.rank() - from.rank();
    const int abs_file = df < 0 ? -df : df;
    const int abs_rank = dr < 0 ? -dr : dr;
    if (piece.type() == PieceType::PAWN) {
        return abs_file == 1 && dr == (piece.color() == Color::WHITE ? 1 : -1);
    }
    if (piece.type() == PieceType::KNIGHT) {
        return (abs_file == 1 && abs_rank == 2) || (abs_file == 2 && abs_rank == 1);
    }
    if (piece.type() == PieceType::KING) {
        return abs_file <= 1 && abs_rank <= 1;
    }
    if (!is_slider(piece.type()) || !slider_matches(piece.type(), df, dr)) {
        return false;
    }
    const int step_file = static_cast<int>(df > 0) - static_cast<int>(df < 0);
    const int step_rank = static_cast<int>(dr > 0) - static_cast<int>(dr < 0);
    int file = from.file() + step_file;
    int rank = from.rank() + step_rank;
    while (file >= 0 && file < 8 && rank >= 0 && rank < 8 &&
           (file != target.file() || rank != target.rank())) {
        if (board.at(Square(file + (rank * 8))) != Piece::NONE) {
            return false;
        }
        file += step_file;
        rank += step_rank;
    }
    return file == target.file() && rank == target.rank();
}

[[nodiscard]] bool aligned(Square a, Square b, Square c) noexcept {
    const int ab_file = b.file() - a.file();
    const int ab_rank = b.rank() - a.rank();
    const int ac_file = c.file() - a.file();
    const int ac_rank = c.rank() - a.rank();
    if (ab_file == 0) {
        return ac_file == 0;
    }
    if (ab_rank == 0) {
        return ac_rank == 0;
    }
    return (ab_file == ab_rank || ab_file == -ab_rank) &&
           (ac_file == ac_rank || ac_file == -ac_rank) && ab_file * ac_rank == ab_rank * ac_file;
}

[[nodiscard]] bool absolutely_pinned(const chess::Board& board, Square from, Color side) noexcept {
    const Square king = board.kingSq(side);
    const int df = from.file() - king.file();
    const int dr = from.rank() - king.rank();
    if ((df == 0 && dr == 0) || (df != 0 && dr != 0 && df != dr && df != -dr)) {
        return false;
    }
    const int step_file = static_cast<int>(df > 0) - static_cast<int>(df < 0);
    const int step_rank = static_cast<int>(dr > 0) - static_cast<int>(dr < 0);
    int file = king.file() + step_file;
    int rank = king.rank() + step_rank;
    while (file >= 0 && file < 8 && rank >= 0 && rank < 8 &&
           (file != from.file() || rank != from.rank())) {
        if (board.at(Square(file + (rank * 8))) != Piece::NONE) {
            return false;
        }
        file += step_file;
        rank += step_rank;
    }
    if (file != from.file() || rank != from.rank()) {
        return false;
    }
    file += step_file;
    rank += step_rank;
    while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
        const Piece piece = board.at(Square(file + (rank * 8)));
        if (piece != Piece::NONE) {
            return piece.color() != side && slider_matches(piece.type(), step_file, step_rank);
        }
        file += step_file;
        rank += step_rank;
    }
    return false;
}

[[nodiscard]] int comparison_value(PieceType type) noexcept {
    return type == PieceType::KING ? std::numeric_limits<int>::max() : piece_value(type);
}

void set_turn(chess::Board& board, Color side) {
    if (board.sideToMove() != side) {
        board.makeNullMove();
    }
}

[[nodiscard]] int exchange_sequence(const chess::Board& board, Square target, Color side) {
    const Piece victim = board.at(target);
    if (victim == Piece::NONE || victim.color() == side || victim.type() == PieceType::KING) {
        return 0;
    }

    int least_attacker_value = std::numeric_limits<int>::max();
    std::vector<Move> attackers;
    attackers.reserve(8);
    const Square king = board.kingSq(side);
    for (int index = 0; index < 64; ++index) {
        const Square from(index);
        const Piece attacker = board.at(from);
        if (attacker == Piece::NONE || attacker.color() != side ||
            !piece_attacks(board, from, target)) {
            continue;
        }
        if (absolutely_pinned(board, from, side) && !aligned(king, from, target)) {
            continue;
        }

        const int value = comparison_value(attacker.type());
        if (value > least_attacker_value) {
            continue;
        }
        const Move move = attacker.type() == PieceType::PAWN &&
                                  ((side == Color::WHITE && static_cast<int>(target.rank()) == 7) ||
                                   (side == Color::BLACK && static_cast<int>(target.rank()) == 0))
                              ? Move::make<Move::PROMOTION>(from, target, PieceType::QUEEN)
                              : Move::make(from, target);
        if (attacker.type() == PieceType::KING) {
            EditableBoard after(board);
            after.relocate(from, target);
            if (after.isAttacked(target, other(side))) {
                continue;
            }
        }
        if (value < least_attacker_value) {
            least_attacker_value = value;
            attackers.clear();
        }
        attackers.push_back(move);
    }

    int best = 0;
    for (const Move move : attackers) {
        chess::Board after = board;
        set_turn(after, side);
        after.makeMove(move);
        const int promotion_gain =
            move.typeOf() == Move::PROMOTION
                ? piece_value(move.promotionType()) - piece_value(PieceType::PAWN)
                : 0;
        const int score = piece_value(victim.type()) + promotion_gain -
                          exchange_sequence(after, target, other(side));
        best = std::max(best, score);
    }
    return std::max(0, best);
}

[[nodiscard]] bool legal_capture_on(const chess::Board& board, Square target, Color side) {
    chess::Board side_to_move = board;
    set_turn(side_to_move, side);
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, side_to_move);
    return std::ranges::any_of(moves, [&side_to_move, target](Move move) {
        return move.to() == target && side_to_move.isCapture(move);
    });
}

[[nodiscard]] Motif make_motif(MotifTag tag, Color side, int ply, PieceOnSquare actor,
                               std::vector<PieceOnSquare> targets = {},
                               std::optional<int> gain = std::nullopt) {
    return {.tag = tag,
            .side = report_side(side),
            .ply = ply,
            .actor = actor,
            .targets = std::move(targets),
            .gain = gain,
            .threat = std::nullopt,
            .restriction = std::nullopt,
            .contribution = std::nullopt};
}

[[nodiscard]] bool terminal_position(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    return moves.empty();
}

[[nodiscard]] std::vector<Motif> pin_tactical_motifs(const chess::Board& board, Color side,
                                                     int ply);

[[nodiscard]] std::optional<int> captured_value(const chess::Board& before, Move move) {
    const auto captured = before.getCapturing<PieceType>(move);
    if (captured == PieceType::NONE) {
        return std::nullopt;
    }
    return piece_value(captured);
}

[[nodiscard]] bool trade_in_progress(const chess::Board& board, Square square,
                                     const chess::Board* before, const Move* previous_move,
                                     int see_value) {
    if (before == nullptr || previous_move == nullptr || previous_move->to() != square ||
        !before->isCapture(*previous_move)) {
        return false;
    }
    const Piece mover_before = before->at(previous_move->from());
    const Piece mover_after = board.at(square);
    if (mover_before == Piece::NONE || mover_after == Piece::NONE ||
        mover_before.color() != mover_after.color()) {
        return false;
    }
    const std::optional<int> value = captured_value(*before, *previous_move);
    return value.has_value() && *value >= see_value;
}

void apply_restrictions(const chess::Board& board, std::vector<Motif>& motifs);

[[nodiscard]] std::vector<Motif> state_motifs(const chess::Board& board, int ply,
                                              const chess::Board* before = nullptr,
                                              const Move* previous_move = nullptr,
                                              bool include_pin_tactical = true) {
    std::vector<Motif> motifs;
    if (terminal_position(board)) {
        return motifs;
    }

    for (const Color side : {Color::WHITE, Color::BLACK}) {
        const Color enemy = other(side);
        if (board.sideToMove() == side) {
            const bool side_in_check = board.isAttacked(board.kingSq(side), enemy);
            for (int index = 0; index < 64; ++index) {
                const Square square(index);
                const Piece piece = board.at(square);
                if (piece == Piece::NONE || piece.color() != enemy ||
                    (piece.type() != PieceType::KNIGHT && piece.type() != PieceType::BISHOP &&
                     piece.type() != PieceType::ROOK && piece.type() != PieceType::QUEEN) ||
                    !board.isAttacked(square, side)) {
                    continue;
                }
                const int gain = see(board, square, side);
                if (gain >= 3 && (!side_in_check || legal_capture_on(board, square, side)) &&
                    !trade_in_progress(board, square, before, previous_move, gain)) {
                    motifs.push_back(make_motif(MotifTag::HangingPiece, side, ply,
                                                printed_piece(board, square), {}, gain));
                }
            }
        }

        for (int index = 0; index < 64; ++index) {
            const Square slider_square(index);
            const Piece slider = board.at(slider_square);
            if (slider == Piece::NONE || slider.color() != side || !is_slider(slider.type())) {
                continue;
            }
            for (const auto& [df, dr] : kDirections) {
                if (!slider_matches(slider.type(), df, dr)) {
                    continue;
                }
                int file = slider_square.file() + df;
                int rank = slider_square.rank() + dr;
                Square front;
                while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                    front = Square(file + (rank * 8));
                    if (board.at(front) != Piece::NONE) {
                        break;
                    }
                    file += df;
                    rank += dr;
                }
                if (file < 0 || file >= 8 || rank < 0 || rank >= 8 ||
                    board.at(front) == Piece::NONE || board.at(front).color() != enemy ||
                    board.at<PieceType>(front) == PieceType::KING) {
                    continue;
                }
                file += df;
                rank += dr;
                while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                    const Square rear(file + (rank * 8));
                    if (board.at(rear) != Piece::NONE) {
                        const Piece rear_piece = board.at(rear);
                        if (rear_piece.color() == enemy && rear_piece.type() == PieceType::KING) {
                            motifs.push_back(make_motif(
                                MotifTag::PinAbsolute, side, ply,
                                printed_piece(board, slider_square),
                                {printed_piece(board, front), printed_piece(board, rear)}));
                        } else if (rear_piece.color() == enemy &&
                                   piece_value(rear_piece.type()) >
                                       piece_value(board.at<PieceType>(front))) {
                            EditableBoard without_front(board);
                            without_front.erase(front);
                            if (see(without_front, rear, side) >= 1) {
                                motifs.push_back(make_motif(
                                    MotifTag::PinRelative, side, ply,
                                    printed_piece(board, slider_square),
                                    {printed_piece(board, front), printed_piece(board, rear)}));
                            }
                        }
                        break;
                    }
                    file += df;
                    rank += dr;
                }
            }
        }

        const Square king_square = board.kingSq(enemy);
        if (static_cast<int>(king_square.rank()) == (enemy == Color::WHITE ? 0 : 7)) {
            const int front_rank = king_square.rank() + (enemy == Color::WHITE ? 1 : -1);
            bool all_front_squares_held = true;
            for (int df = -1; df <= 1; ++df) {
                const int file = king_square.file() + df;
                if (file < 0 || file >= 8 || front_rank < 0 || front_rank >= 8) {
                    continue;
                }
                const Square square(file + (front_rank * 8));
                const Piece piece = board.at(square);
                if ((piece == Piece::NONE || piece.color() != enemy) &&
                    !board.isAttacked(square, side)) {
                    all_front_squares_held = false;
                    break;
                }
            }
            bool attacker_has_heavy_piece = false;
            bool king_has_home_heavy_piece = false;
            for (int index = 0; index < 64; ++index) {
                const Square square(index);
                const Piece piece = board.at(square);
                if (piece == Piece::NONE ||
                    (piece.type() != PieceType::ROOK && piece.type() != PieceType::QUEEN)) {
                    continue;
                }
                attacker_has_heavy_piece = attacker_has_heavy_piece || piece.color() == side;
                king_has_home_heavy_piece =
                    king_has_home_heavy_piece ||
                    (piece.color() == enemy && square.rank() == king_square.rank());
            }
            if (all_front_squares_held && attacker_has_heavy_piece && !king_has_home_heavy_piece) {
                motifs.push_back(make_motif(MotifTag::BackRankWeakness, side, ply,
                                            printed_piece(board, king_square)));
            }
        }

        for (int defender_index = 0; defender_index < 64; ++defender_index) {
            const Square defender_square(defender_index);
            const Piece defender = board.at(defender_square);
            if (defender == Piece::NONE || defender.color() != enemy ||
                defender.type() == PieceType::KING) {
                continue;
            }
            EditableBoard without_defender(board);
            without_defender.erase(defender_square);
            std::vector<PieceOnSquare> targets;
            for (int target_index = 0; target_index < 64; ++target_index) {
                const Square target_square(target_index);
                const Piece target = board.at(target_square);
                if (target == Piece::NONE || target.color() != enemy ||
                    target.type() == PieceType::KING || target_square == defender_square ||
                    !board.isAttacked(target_square, side) || see(board, target_square, side) > 0 ||
                    see(without_defender, target_square, side) < 1) {
                    continue;
                }
                targets.push_back(printed_piece(board, target_square));
            }
            if (targets.size() >= 2) {
                motifs.push_back(make_motif(MotifTag::OverloadedDefender, side, ply,
                                            printed_piece(board, defender_square),
                                            std::move(targets)));
            }
        }

        const Square enemy_king = board.kingSq(enemy);
        if (!board.isAttacked(enemy_king, side)) {
            chess::Board enemy_turn = board;
            set_turn(enemy_turn, enemy);
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, enemy_turn);
            for (int index = 0; index < 64; ++index) {
                const Square square(index);
                const Piece piece = board.at(square);
                if (piece == Piece::NONE || piece.color() != enemy ||
                    (piece.type() != PieceType::KNIGHT && piece.type() != PieceType::BISHOP &&
                     piece.type() != PieceType::ROOK && piece.type() != PieceType::QUEEN) ||
                    !board.isAttacked(square, side)) {
                    continue;
                }
                const int gain = see(board, square, side);
                if (gain < 1) {
                    continue;
                }
                bool trapped = true;
                for (const Move move : legal_moves) {
                    if (move.from() != square) {
                        continue;
                    }
                    const auto captured = enemy_turn.getCapturing<PieceType>(move);
                    if (captured == PieceType::KING) {
                        continue;
                    }
                    if (piece_value(captured) >= piece_value(piece.type())) {
                        trapped = false;
                        break;
                    }
                    chess::Board after = enemy_turn;
                    after.makeMove(move);
                    if (see(after, move.to(), side) < 1) {
                        trapped = false;
                        break;
                    }
                }
                if (trapped) {
                    motifs.push_back(make_motif(MotifTag::TrappedPiece, side, ply,
                                                printed_piece(board, square), {}, gain));
                }
            }
        }
        if (include_pin_tactical) {
            std::vector<Motif> pin_motifs = pin_tactical_motifs(board, side, ply);
            motifs.insert(motifs.end(), std::make_move_iterator(pin_motifs.begin()),
                          std::make_move_iterator(pin_motifs.end()));
        }
    }
    apply_restrictions(board, motifs);
    return motifs;
}

[[nodiscard]] std::vector<PieceOnSquare> checkers(const chess::Board& board, Color attacker,
                                                  Square king_square) {
    std::vector<PieceOnSquare> result;
    for (int index = 0; index < 64; ++index) {
        const Square square(index);
        const Piece piece = board.at(square);
        if (piece != Piece::NONE && piece.color() == attacker &&
            piece_attacks(board, square, king_square)) {
            result.push_back(printed_piece(board, square));
        }
    }
    return result;
}

[[nodiscard]] bool strict_between(Square first, Square second, Square candidate) noexcept {
    const int df = second.file() - first.file();
    const int dr = second.rank() - first.rank();
    if (!aligned(first, second, candidate) || candidate == first || candidate == second) {
        return false;
    }
    return (candidate.file() - first.file()) * df + (candidate.rank() - first.rank()) * dr > 0 &&
           (candidate.file() - second.file()) * (-df) + (candidate.rank() - second.rank()) * (-dr) >
               0;
}

[[nodiscard]] std::vector<Motif> move_motifs(const chess::Board& before, const chess::Board& after,
                                             Move move, int ply) {
    std::vector<Motif> motifs;
    const Piece moved_before = before.at(move.from());
    const Color side = moved_before.color();
    const Color enemy = other(side);
    const Piece moved_after = after.at(move.to());
    const PieceOnSquare king_target = printed_piece(after, after.kingSq(enemy));

    const bool castling = move.typeOf() == Move::CASTLING;
    Square castling_rook_square;
    if (castling) {
        const bool king_side = move.to().file() > move.from().file();
        castling_rook_square = Square::castling_rook_square(king_side, side);
    }
    const auto is_moved_piece_square = [move, castling, castling_rook_square](Square square) {
        return square == move.to() || (castling && square == castling_rook_square);
    };

    if (moved_before.type() != PieceType::KING && see(after, move.to(), enemy) <= 0) {
        std::vector<PieceOnSquare> targets;
        for (int index = 0; index < 64; ++index) {
            const Square target(index);
            const Piece target_piece = after.at(target);
            if (target_piece == Piece::NONE || target_piece.color() != enemy ||
                target_piece.type() == PieceType::PAWN ||
                !piece_attacks(after, move.to(), target)) {
                continue;
            }
            const bool king = target_piece.type() == PieceType::KING;
            const bool higher_value =
                piece_value(target_piece.type()) > piece_value(moved_after.type());
            const bool unsafe_and_not_counterattacking =
                see(after, target, side) >= 1 && !piece_attacks(after, target, move.to());
            if (king || higher_value || unsafe_and_not_counterattacking) {
                targets.push_back(printed_piece(after, target));
            }
        }
        if (targets.size() >= 2) {
            motifs.push_back(make_motif(MotifTag::Fork, side, ply, printed_piece(after, move.to()),
                                        std::move(targets)));
        }
    }

    if (is_slider(moved_after.type()) && see(after, move.to(), enemy) <= 0) {
        for (const auto& [df, dr] : kDirections) {
            if (!slider_matches(moved_after.type(), df, dr)) {
                continue;
            }
            int file = move.to().file() + df;
            int rank = move.to().rank() + dr;
            Square front;
            while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                front = Square(file + (rank * 8));
                if (after.at(front) != Piece::NONE) {
                    break;
                }
                file += df;
                rank += dr;
            }
            if (file < 0 || file >= 8 || rank < 0 || rank >= 8 || after.at(front) == Piece::NONE ||
                after.at(front).color() != enemy) {
                continue;
            }
            file += df;
            rank += dr;
            while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                const Square rear(file + (rank * 8));
                if (after.at(rear) != Piece::NONE) {
                    const Piece front_piece = after.at(front);
                    const Piece rear_piece = after.at(rear);
                    if (rear_piece.color() == enemy && rear_piece.type() != PieceType::PAWN &&
                        (front_piece.type() == PieceType::KING ||
                         comparison_value(front_piece.type()) >
                             comparison_value(rear_piece.type()))) {
                        bool exposed = true;
                        if (front_piece.type() != PieceType::KING) {
                            EditableBoard without_front(after);
                            without_front.erase(front);
                            exposed = see(without_front, rear, side) >= 1;
                        }
                        if (exposed) {
                            motifs.push_back(make_motif(
                                MotifTag::Skewer, side, ply, printed_piece(after, move.to()),
                                {printed_piece(after, front), printed_piece(after, rear)}));
                        }
                    }
                    break;
                }
                file += df;
                rank += dr;
            }
        }
    }

    const std::vector<PieceOnSquare> king_checkers = checkers(after, side, after.kingSq(enemy));
    bool discovered_check = false;
    for (const PieceOnSquare checker : king_checkers) {
        if (!is_moved_piece_square(Square(checker.square))) {
            discovered_check = true;
            motifs.push_back(
                make_motif(MotifTag::DiscoveredCheck, side, ply, checker, {king_target}));
        }
    }
    if (king_checkers.size() >= 2) {
        motifs.push_back(make_motif(MotifTag::DoubleCheck, side, ply,
                                    printed_piece(after, move.to()), {king_target}));
    }

    if (!discovered_check) {
        for (int slider_index = 0; slider_index < 64; ++slider_index) {
            const Square slider_square(slider_index);
            const Piece slider = before.at(slider_square);
            if (slider == Piece::NONE || slider.color() != side || !is_slider(slider.type())) {
                continue;
            }
            for (int target_index = 0; target_index < 64; ++target_index) {
                const Square target(target_index);
                const Piece target_piece = after.at(target);
                if (target_piece == Piece::NONE || target_piece.color() != enemy ||
                    target_piece.type() == PieceType::PAWN ||
                    !strict_between(slider_square, target, move.from()) ||
                    strict_between(slider_square, target, move.to()) ||
                    !piece_attacks(after, slider_square, target)) {
                    continue;
                }
                if (comparison_value(target_piece.type()) > comparison_value(slider.type()) ||
                    see(after, target, side) >= 1) {
                    motifs.push_back(make_motif(MotifTag::DiscoveredAttack, side, ply,
                                                printed_piece(after, slider_square),
                                                {printed_piece(after, target)}));
                }
            }
        }
    }

    const Square mate_king = after.kingSq(enemy);
    bool back_rank_mate_shape =
        static_cast<int>(mate_king.rank()) == (enemy == Color::WHITE ? 0 : 7);
    if (back_rank_mate_shape) {
        const int mate_front_rank = mate_king.rank() + (enemy == Color::WHITE ? 1 : -1);
        for (int file_offset = -1; file_offset <= 1; ++file_offset) {
            const int file = mate_king.file() + file_offset;
            if (file < 0 || file >= 8 || mate_front_rank < 0 || mate_front_rank >= 8) {
                continue;
            }
            const Piece front_piece = after.at(Square(file + (mate_front_rank * 8)));
            if (front_piece == Piece::NONE || front_piece.color() != enemy) {
                back_rank_mate_shape = false;
                break;
            }
        }
    }
    if (!king_checkers.empty() && back_rank_mate_shape && !chess::movegen::anylegalmoves(after)) {
        for (const PieceOnSquare checker : king_checkers) {
            if (checker.square / 8 == static_cast<std::uint8_t>(mate_king.rank())) {
                motifs.push_back(
                    make_motif(MotifTag::BackRankMate, side, ply, checker, {king_target}));
            }
        }
    }

    if (move.typeOf() == Move::PROMOTION) {
        motifs.push_back(
            make_motif(MotifTag::Promotion, side, ply, printed_piece(after, move.to())));
    }
    return motifs;
}

struct MoveOutcome {
    Move move;
    std::vector<Motif> motifs;
    bool checkmate = false;
};

[[nodiscard]] bool checkmate_after(const chess::Board& board, Color attacker, Color defender) {
    return board.isAttacked(board.kingSq(defender), attacker) &&
           !chess::movegen::anylegalmoves(board);
}

[[nodiscard]] Square captured_square(Move move) {
    return move.typeOf() == Move::ENPASSANT ? move.to().ep_square() : move.to();
}

[[nodiscard]] Square move_destination(Move move, Color side) {
    if (move.typeOf() == Move::CASTLING) {
        return Square::castling_king_square(move.to() > move.from(), side);
    }
    return move.to();
}

[[nodiscard]] std::optional<ThreatKind> threat_kind(MotifTag tag) {
    switch (tag) {
    case MotifTag::Fork:
        return ThreatKind::Fork;
    case MotifTag::Skewer:
        return ThreatKind::Skewer;
    case MotifTag::DiscoveredCheck:
        return ThreatKind::DiscoveredCheck;
    case MotifTag::DiscoveredAttack:
        return ThreatKind::DiscoveredAttack;
    case MotifTag::DoubleCheck:
        return ThreatKind::DoubleCheck;
    case MotifTag::BackRankMate:
        return ThreatKind::BackRankMate;
    case MotifTag::Promotion:
        return ThreatKind::Promotion;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::vector<Motif> pin_tactical_motifs(const chess::Board& board, Color side,
                                                     int ply) {
    const Color enemy = other(side);
    if (board.isAttacked(board.kingSq(enemy), side) ||
        board.isAttacked(board.kingSq(side), enemy)) {
        return {};
    }

    chess::Board side_to_move = board;
    set_turn(side_to_move, side);
    chess::Movelist original_moves;
    chess::movegen::legalmoves(original_moves, side_to_move);

    std::vector<MoveOutcome> original_outcomes;
    original_outcomes.reserve(original_moves.size());
    for (const Move move : original_moves) {
        chess::Board after = side_to_move;
        after.makeMove(move);
        original_outcomes.push_back({move, move_motifs(side_to_move, after, move, ply),
                                     checkmate_after(after, side, enemy)});
    }

    std::vector<Motif> result;
    const auto add = [&result, side, ply](PieceOnSquare actor, std::vector<PieceOnSquare> targets,
                                          ThreatKind kind, Square destination) {
        Motif motif = make_motif(MotifTag::PinTactical, side, ply, actor, std::move(targets));
        motif.threat =
            MotifThreat{.kind = kind, .square = static_cast<std::uint8_t>(destination.index())};
        const auto existing = std::ranges::find_if(result, [&motif](const Motif& candidate) {
            return candidate.actor.square == motif.actor.square &&
                   candidate.targets.front().square == motif.targets.front().square &&
                   candidate.threat->kind == motif.threat->kind;
        });
        if (existing == result.end()) {
            result.push_back(std::move(motif));
        } else if (motif.threat->square < existing->threat->square) {
            *existing = std::move(motif);
        }
    };

    for (int index = 0; index < 64; ++index) {
        const Square pinned_square(index);
        const Piece pinned_piece = board.at(pinned_square);
        if (pinned_piece == Piece::NONE || pinned_piece.color() != enemy ||
            pinned_piece.type() == PieceType::KING) {
            continue;
        }

        EditableBoard without_piece(side_to_move);
        without_piece.erase_for_pin(pinned_square);
        if (without_piece.isAttacked(without_piece.kingSq(enemy), side) ||
            without_piece.isAttacked(without_piece.kingSq(side), enemy)) {
            continue;
        }

        chess::Movelist exposed_moves;
        chess::movegen::legalmoves(exposed_moves, without_piece);
        for (const Move move : exposed_moves) {
            const auto original =
                std::ranges::find_if(original_outcomes, [move](const MoveOutcome& outcome) {
                    return outcome.move == move;
                });
            chess::Board after = without_piece;
            after.makeMove(move);
            const PieceOnSquare actor = printed_piece(without_piece, move.from());
            for (const Motif& threat : move_motifs(without_piece, after, move, ply)) {
                const std::optional<ThreatKind> kind = threat_kind(threat.tag);
                if (!kind || (original != original_outcomes.end() &&
                              std::ranges::any_of(original->motifs, [&threat](const Motif& motif) {
                                  return motif.tag == threat.tag;
                              }))) {
                    continue;
                }
                std::vector<PieceOnSquare> targets{printed_piece(board, pinned_square)};
                targets.insert(targets.end(), threat.targets.begin(), threat.targets.end());
                add(actor, std::move(targets), *kind, move_destination(move, side));
            }

            if (checkmate_after(after, side, enemy) &&
                (original == original_outcomes.end() || !original->checkmate)) {
                add(actor,
                    {printed_piece(board, pinned_square),
                     printed_piece(after, after.kingSq(enemy))},
                    ThreatKind::Checkmate, move_destination(move, side));
            }
        }

        for (int target_index = 0; target_index < 64; ++target_index) {
            const Square target(target_index);
            const Piece victim = without_piece.at(target);
            if (target == pinned_square || victim == Piece::NONE || victim.color() != enemy ||
                victim.type() == PieceType::KING || see(side_to_move, target, side) > 0 ||
                see(without_piece, target, side) < 1) {
                continue;
            }

            Move best_capture = Move::NO_MOVE;
            int best_value = std::numeric_limits<int>::max();
            int best_from = 64;
            bool best_is_queen_promotion = false;
            for (const Move move : exposed_moves) {
                if (captured_square(move) != target || !without_piece.isCapture(move)) {
                    continue;
                }
                const Piece capturer = without_piece.at(move.from());
                const int value = comparison_value(capturer.type());
                const bool queen_promotion =
                    move.typeOf() == Move::PROMOTION && move.promotionType() == PieceType::QUEEN;
                if (value < best_value ||
                    (value == best_value && move.from().index() < best_from) ||
                    (value == best_value && move.from().index() == best_from && queen_promotion &&
                     !best_is_queen_promotion)) {
                    best_capture = move;
                    best_value = value;
                    best_from = move.from().index();
                    best_is_queen_promotion = queen_promotion;
                }
            }
            if (best_capture == Move::NO_MOVE) {
                continue;
            }
            add(printed_piece(without_piece, best_capture.from()),
                {printed_piece(board, pinned_square), printed_piece(without_piece, target)},
                ThreatKind::Capture, best_capture.to());
        }
    }
    return result;
}

[[nodiscard]] std::string square_name(Square square) {
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + square.file());
    result[1] = static_cast<char>('1' + square.rank());
    return result;
}

[[nodiscard]] std::vector<Move> pseudo_legal_moves_from(const chess::Board& board, Square from) {
    const Piece piece = board.at(from);
    if (piece == Piece::NONE || piece.color() != board.sideToMove()) {
        return {};
    }

    std::vector<Move> result;
    const auto add_move = [&result, &board, from, piece](Square to) {
        const Piece destination = board.at(to);
        if (destination != Piece::NONE &&
            (destination.color() == piece.color() || destination.type() == PieceType::KING)) {
            return;
        }
        if (piece.type() == PieceType::PAWN &&
            static_cast<int>(to.rank()) == (piece.color() == Color::WHITE ? 7 : 0)) {
            for (const PieceType promotion :
                 {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                result.push_back(Move::make<Move::PROMOTION>(from, to, promotion));
            }
        } else {
            result.push_back(Move::make(from, to));
        }
    };

    if (piece.type() == PieceType::PAWN) {
        const int direction = piece.color() == Color::WHITE ? 1 : -1;
        const int next_rank = from.rank() + direction;
        if (next_rank >= 0 && next_rank < 8) {
            const Square one_step(from.file() + (next_rank * 8));
            if (board.at(one_step) == Piece::NONE) {
                add_move(one_step);
                const int start_rank = piece.color() == Color::WHITE ? 1 : 6;
                const int double_rank = from.rank() + (2 * direction);
                if (static_cast<int>(from.rank()) == start_rank &&
                    board.at(Square(from.file() + (double_rank * 8))) == Piece::NONE) {
                    result.push_back(Move::make(from, Square(from.file() + (double_rank * 8))));
                }
            }
            for (const int file_delta : {-1, 1}) {
                const int next_file = from.file() + file_delta;
                if (next_file < 0 || next_file >= 8) {
                    continue;
                }
                const Square to(next_file + (next_rank * 8));
                const Piece destination = board.at(to);
                if ((destination != Piece::NONE && destination.color() != piece.color() &&
                     destination.type() != PieceType::KING) ||
                    (to == board.enpassantSq() && destination == Piece::NONE)) {
                    if (to == board.enpassantSq() && destination == Piece::NONE) {
                        result.push_back(Move::make<Move::ENPASSANT>(from, to));
                    } else {
                        add_move(to);
                    }
                }
            }
        }
        return result;
    }

    constexpr std::array<std::pair<int, int>, 8> knight_steps{
        {{1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}}};
    constexpr std::array<std::pair<int, int>, 8> king_steps{
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
    const auto add_step = [&board, from, piece, &add_move](int file_delta, int rank_delta,
                                                           bool slide) {
        int file = from.file() + file_delta;
        int rank = from.rank() + rank_delta;
        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            const Square to(file + (rank * 8));
            const Piece destination = board.at(to);
            if (destination == Piece::NONE ||
                (destination.color() != piece.color() && destination.type() != PieceType::KING)) {
                add_move(to);
            }
            if (!slide || destination != Piece::NONE) {
                break;
            }
            file += file_delta;
            rank += rank_delta;
        }
    };

    if (piece.type() == PieceType::KNIGHT) {
        for (const auto& [file_delta, rank_delta] : knight_steps) {
            add_step(file_delta, rank_delta, false);
        }
    } else if (piece.type() == PieceType::KING) {
        for (const auto& [file_delta, rank_delta] : king_steps) {
            add_step(file_delta, rank_delta, false);
        }
    } else {
        for (const auto& [file_delta, rank_delta] : kDirections) {
            if (slider_matches(piece.type(), file_delta, rank_delta)) {
                add_step(file_delta, rank_delta, true);
            }
        }
    }
    return result;
}

[[nodiscard]] bool on_segment(Square first, Square last, Square candidate) noexcept {
    const int df = last.file() - first.file();
    const int dr = last.rank() - first.rank();
    const int cf = candidate.file() - first.file();
    const int cr = candidate.rank() - first.rank();
    return cf * dr == cr * df && cf * df + cr * dr >= 0 && cf * df + cr * dr <= df * df + dr * dr;
}

[[nodiscard]] std::vector<std::string> uci_strings(const std::vector<Move>& moves) {
    std::vector<std::string> result;
    result.reserve(moves.size());
    for (const Move move : moves) {
        result.push_back(chess::uci::moveToUci(move));
    }
    std::ranges::sort(result);
    return result;
}

[[nodiscard]] HeldTo held_to(Square held_square, const std::vector<Move>& free_moves) {
    if (free_moves.empty()) {
        return {.kind = HeldTo::Kind::Square, .name = square_name(held_square)};
    }
    std::vector<Square> destinations;
    destinations.reserve(free_moves.size());
    for (const Move move : free_moves) {
        if (std::ranges::find(destinations, move.to()) == destinations.end()) {
            destinations.push_back(move.to());
        }
    }
    std::ranges::sort(destinations, [](Square left, Square right) {
        return left.file() == right.file() ? left.rank() < right.rank()
                                           : left.file() < right.file();
    });

    const bool one_file = std::ranges::all_of(
        destinations, [held_square](Square square) { return square.file() == held_square.file(); });
    if (one_file) {
        return {.kind = HeldTo::Kind::File,
                .name = std::string(1, static_cast<char>('a' + held_square.file()))};
    }
    const bool one_rank = std::ranges::all_of(
        destinations, [held_square](Square square) { return square.rank() == held_square.rank(); });
    if (one_rank) {
        return {.kind = HeldTo::Kind::Rank,
                .name = std::string(1, static_cast<char>('1' + held_square.rank()))};
    }
    const Square diagonal_destination = destinations.front();
    const int diagonal_file_delta = diagonal_destination.file() - held_square.file();
    const int diagonal_rank_delta = diagonal_destination.rank() - held_square.rank();
    const bool one_diagonal =
        diagonal_file_delta != 0 &&
        (diagonal_file_delta == diagonal_rank_delta ||
         diagonal_file_delta == -diagonal_rank_delta) &&
        std::ranges::all_of(destinations, [held_square, diagonal_destination](Square square) {
            return aligned(held_square, diagonal_destination, square);
        });
    if (one_diagonal) {
        const int file_delta = destinations.front().file() - held_square.file();
        const int rank_delta = destinations.front().rank() - held_square.rank();
        const int step_file = static_cast<int>(file_delta > 0) - static_cast<int>(file_delta < 0);
        const int step_rank = static_cast<int>(rank_delta > 0) - static_cast<int>(rank_delta < 0);
        int first_file = held_square.file();
        int first_rank = held_square.rank();
        while (first_file - step_file >= 0 && first_file - step_file < 8 &&
               first_rank - step_rank >= 0 && first_rank - step_rank < 8) {
            first_file -= step_file;
            first_rank -= step_rank;
        }
        int last_file = held_square.file();
        int last_rank = held_square.rank();
        while (last_file + step_file >= 0 && last_file + step_file < 8 &&
               last_rank + step_rank >= 0 && last_rank + step_rank < 8) {
            last_file += step_file;
            last_rank += step_rank;
        }
        Square first(first_file + (first_rank * 8));
        Square last(last_file + (last_rank * 8));
        if (first.file() > last.file()) {
            std::swap(first, last);
        }
        return {.kind = HeldTo::Kind::Diagonal,
                .name = square_name(first) + '-' + square_name(last)};
    }

    std::string name;
    for (const Square square : destinations) {
        if (!name.empty()) {
            name += ',';
        }
        name += square_name(square);
    }
    return {.kind = HeldTo::Kind::Squares, .name = std::move(name)};
}

[[nodiscard]] bool same_uci_move(Move left, Move right) noexcept {
    if (left.from() != right.from() || left.to() != right.to()) {
        return false;
    }
    const bool left_is_promotion = left.typeOf() == Move::PROMOTION;
    const bool right_is_promotion = right.typeOf() == Move::PROMOTION;
    return left_is_promotion == right_is_promotion &&
           (!left_is_promotion || left.promotionType() == right.promotionType());
}

[[nodiscard]] bool has_uci_move(const std::vector<Move>& moves, Move candidate) {
    return std::ranges::any_of(moves,
                               [candidate](Move move) { return same_uci_move(move, candidate); });
}

void append_unique_uci_move(std::vector<Move>& moves, Move candidate) {
    if (!has_uci_move(moves, candidate)) {
        moves.push_back(candidate);
    }
}

[[nodiscard]] std::vector<Move> hypothetical_pawn_captures(const chess::Board& board, Square from) {
    const Piece pawn = board.at(from);
    if (pawn.type() != PieceType::PAWN) {
        return {};
    }
    const int next_rank = from.rank() + (pawn.color() == Color::WHITE ? 1 : -1);
    if (next_rank < 0 || next_rank >= 8) {
        return {};
    }

    std::vector<Move> result;
    for (const int file_delta : {-1, 1}) {
        const int next_file = from.file() + file_delta;
        if (next_file < 0 || next_file >= 8) {
            continue;
        }
        const Square to(next_file + (next_rank * 8));
        if (board.at<PieceType>(to) == PieceType::KING) {
            continue;
        }
        if (next_rank == (pawn.color() == Color::WHITE ? 7 : 0)) {
            for (const PieceType promotion :
                 {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                result.push_back(Move::make<Move::PROMOTION>(from, to, promotion));
            }
        } else {
            result.push_back(Move::make(from, to));
        }
    }
    return result;
}

[[nodiscard]] bool legal_hypothetical_capture(const chess::Board& board, Move move, Color side) {
    EditableBoard after(board);
    after.make_hypothetical_capture(move);
    const Square king = after.kingSq(side);
    return king != Square::NO_SQ && !after.isAttacked(king, other(side));
}

[[nodiscard]] std::vector<Move> move_set(const chess::Board& board, Square held_square,
                                         const std::vector<Move>& legal_moves, Color side) {
    std::vector<Move> result = legal_moves;
    for (const Move move : hypothetical_pawn_captures(board, held_square)) {
        if (!has_uci_move(result, move) && legal_hypothetical_capture(board, move, side)) {
            result.push_back(move);
        }
    }
    return result;
}

[[nodiscard]] std::vector<Move> absolute_pin_candidates(const chess::Board& board,
                                                        Square held_square) {
    std::vector<Move> result = pseudo_legal_moves_from(board, held_square);
    for (const Move move : hypothetical_pawn_captures(board, held_square)) {
        append_unique_uci_move(result, move);
    }
    return result;
}

[[nodiscard]] std::optional<MotifRestriction> restriction_for(const chess::Board& position,
                                                              const Motif& motif) {
    const Color x = motif.side == Side::White ? Color::WHITE : Color::BLACK;
    const Color y = other(x);
    Square held_square;
    switch (motif.tag) {
    case MotifTag::PinAbsolute:
    case MotifTag::PinRelative:
    case MotifTag::PinTactical:
        held_square = Square(motif.targets.at(0).square);
        break;
    case MotifTag::OverloadedDefender:
        held_square = Square(motif.actor.square);
        break;
    default:
        return std::nullopt;
    }

    chess::Board by = position;
    set_turn(by, y);
    chess::Movelist all_legal_moves;
    chess::movegen::legalmoves(all_legal_moves, by);
    std::vector<Move> legal_moves;
    for (const Move move : all_legal_moves) {
        if (move.from() == held_square && by.at<PieceType>(move.to()) != PieceType::KING) {
            legal_moves.push_back(move);
        }
    }
    const std::vector<Move> legal_move_set = move_set(by, held_square, legal_moves, y);

    std::vector<Move> restricted_moves;
    if (motif.tag == MotifTag::PinAbsolute) {
        const Square king = position.kingSq(y);
        for (const Move move : absolute_pin_candidates(by, held_square)) {
            if (!on_segment(Square(motif.actor.square), king, move.to())) {
                restricted_moves.push_back(move);
            }
        }
    } else if (motif.tag == MotifTag::PinRelative) {
        const Square target(motif.targets.at(1).square);
        const Square attacker(motif.actor.square);
        for (const Move move : legal_move_set) {
            chess::Board after = by;
            after.makeMove(move);
            if (!on_segment(attacker, target, move.to()) && see(after, target, x) >= 1) {
                restricted_moves.push_back(move);
            }
        }
    } else if (motif.tag == MotifTag::OverloadedDefender) {
        for (const Move move : legal_move_set) {
            chess::Board after = by;
            after.makeMove(move);
            const bool exposes_target =
                std::ranges::any_of(motif.targets, [&after, x](const PieceOnSquare& target) {
                    const Square target_square(target.square);
                    return printed_piece(after, target_square) == target &&
                           see(after, target_square, x) >= 1;
                });
            if (exposes_target) {
                restricted_moves.push_back(move);
            }
        }
    } else if (motif.tag == MotifTag::PinTactical && motif.threat.has_value()) {
        for (const Move move : legal_move_set) {
            chess::Board after_y = by;
            after_y.makeMove(move);
            chess::Movelist x_moves;
            chess::movegen::legalmoves(x_moves, after_y);
            const Square destination(motif.threat->square);
            bool enabled = false;
            for (const Move x_move : x_moves) {
                if (x_move.from() != Square(motif.actor.square) ||
                    move_destination(x_move, x) != destination) {
                    continue;
                }
                if (motif.threat->kind == ThreatKind::Capture) {
                    enabled = after_y.isCapture(x_move) && see(after_y, destination, x) >= 1;
                } else {
                    chess::Board after_x = after_y;
                    after_x.makeMove(x_move);
                    if (motif.threat->kind == ThreatKind::Checkmate) {
                        enabled = checkmate_after(after_x, x, y);
                    } else {
                        const std::vector<Motif> produced =
                            move_motifs(after_y, after_x, x_move, 1);
                        enabled = std::ranges::any_of(produced, [&motif](const Motif& candidate) {
                            return threat_kind(candidate.tag) == motif.threat->kind;
                        });
                    }
                }
                if (enabled) {
                    break;
                }
            }
            if (enabled) {
                restricted_moves.push_back(move);
            }
        }
    }

    std::vector<std::string> restricted = uci_strings(restricted_moves);
    if (restricted.empty()) {
        return std::nullopt;
    }
    std::vector<Move> free_moves;
    for (const Move move : legal_move_set) {
        const std::string uci = chess::uci::moveToUci(move);
        if (!std::ranges::binary_search(restricted, uci)) {
            free_moves.push_back(move);
        }
    }
    return MotifRestriction{.piece = printed_piece(position, held_square),
                            .restricted = std::move(restricted),
                            .free = uci_strings(free_moves),
                            .held_to = held_to(held_square, free_moves)};
}

void apply_restrictions(const chess::Board& board, std::vector<Motif>& motifs) {
    for (auto motif = motifs.begin(); motif != motifs.end();) {
        motif->restriction = restriction_for(board, *motif);
        const bool restricted_tag =
            motif->tag == MotifTag::PinAbsolute || motif->tag == MotifTag::PinRelative ||
            motif->tag == MotifTag::OverloadedDefender || motif->tag == MotifTag::PinTactical;
        if (restricted_tag && !motif->restriction.has_value()) {
            motif = motifs.erase(motif);
        } else {
            ++motif;
        }
    }
}

[[nodiscard]] bool identity_equal(const Motif& left, const Motif& right) {
    if (left.tag != right.tag || left.side != right.side ||
        left.actor.square != right.actor.square || left.targets.size() != right.targets.size() ||
        left.threat != right.threat) {
        return false;
    }
    for (std::size_t index = 0; index < left.targets.size(); ++index) {
        if (left.targets[index].square != right.targets[index].square) {
            return false;
        }
    }
    return true;
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
    const auto left_targets = left.targets | std::views::transform(&PieceOnSquare::square);
    const auto right_targets = right.targets | std::views::transform(&PieceOnSquare::square);
    if (!std::ranges::equal(left_targets, right_targets)) {
        return std::ranges::lexicographical_compare(left_targets, right_targets);
    }
    if (left.threat.has_value() != right.threat.has_value()) {
        return !left.threat.has_value();
    }
    if (left.threat.has_value()) {
        if (left.threat->kind != right.threat->kind) {
            return left.threat->kind < right.threat->kind;
        }
        if (left.threat->square != right.threat->square) {
            return left.threat->square < right.threat->square;
        }
    }
    return left.side < right.side;
}

[[nodiscard]] std::vector<Motif> sorted(std::vector<Motif> motifs) {
    std::ranges::sort(motifs, motif_less);
    return motifs;
}

} // namespace

int piece_value(PieceType type) noexcept {
    if (type == PieceType::PAWN) {
        return 1;
    }
    if (type == PieceType::KNIGHT || type == PieceType::BISHOP) {
        return 3;
    }
    if (type == PieceType::ROOK) {
        return 5;
    }
    if (type == PieceType::QUEEN) {
        return 9;
    }
    return 0;
}

int see(const chess::Board& board, Square square, Color side) {
    const Piece target = board.at(square);
    if (target == Piece::NONE || target.color() == side || target.type() == PieceType::KING) {
        return 0;
    }
    return exchange_sequence(board, square, side);
}

std::vector<Motif> reference_position_motifs(const chess::Board& board,
                                             chesswhy::tactics::MotifOptions options) {
    return sorted(state_motifs(board, 0, nullptr, nullptr, options.pin_tactical));
}

std::vector<Motif> reference_line_motifs(const chess::Board& root, std::span<const Move> line,
                                         chesswhy::tactics::MotifOptions options) {
    std::vector<Motif> result;
    chess::Board current = root;
    std::vector<Motif> previous_state =
        state_motifs(root, 0, nullptr, nullptr, options.pin_tactical);
    for (std::size_t index = 0; index < line.size(); ++index) {
        const Move move = line[index];
        const chess::Board before = current;
        current.makeMove(move);
        const int ply = static_cast<int>(index + 1);

        std::vector<Motif> current_state =
            state_motifs(current, ply, &before, &move, options.pin_tactical);
        for (Motif& motif : current_state) {
            if (std::ranges::none_of(previous_state, [&motif](const Motif& previous) {
                    return identity_equal(motif, previous);
                })) {
                result.push_back(motif);
            }
        }
        std::vector<Motif> current_move = move_motifs(before, current, move, ply);
        result.insert(result.end(), std::make_move_iterator(current_move.begin()),
                      std::make_move_iterator(current_move.end()));
        previous_state = std::move(current_state);
    }
    return sorted(std::move(result));
}

} // namespace chesswhy::oracle
