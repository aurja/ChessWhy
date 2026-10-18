#pragma once

#include <array>

#include <chess.hpp>

namespace chesswhy::analysis::detail {

[[nodiscard]] inline bool move_matches_uci(const chess::Board& board, chess::Move move,
                                           std::string_view uci) noexcept {
    const bool promotion = move.typeOf() == chess::Move::PROMOTION;
    if (uci.size() != (promotion ? 5U : 4U)) {
        return false;
    }

    const auto from = move.from();
    auto to = move.to();
    if (move.typeOf() == chess::Move::CASTLING && !board.chess960()) {
        to = chess::Square(to > from ? chess::File::FILE_G : chess::File::FILE_C, from.rank());
    }
    if (uci[0] != static_cast<char>('a' + static_cast<int>(from.file())) ||
        uci[1] != static_cast<char>('1' + static_cast<int>(from.rank())) ||
        uci[2] != static_cast<char>('a' + static_cast<int>(to.file())) ||
        uci[3] != static_cast<char>('1' + static_cast<int>(to.rank()))) {
        return false;
    }
    if (!promotion) {
        return true;
    }

    constexpr std::array<char, 7> promotion_chars{'p', 'n', 'b', 'r', 'q', 'k', ' '};
    const int promotion_type = static_cast<int>(move.promotionType());
    return promotion_type >= 0 && promotion_type < 7 && uci[4] == promotion_chars[promotion_type];
}

[[nodiscard]] inline chess::Move legal_move(const chess::Board& board, std::string_view uci) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const auto& move : moves) {
        if (move_matches_uci(board, move, uci)) {
            return move;
        }
    }
    return chess::Move::NO_MOVE;
}

} // namespace chesswhy::analysis::detail
