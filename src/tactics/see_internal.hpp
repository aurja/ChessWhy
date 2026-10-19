#pragma once

#include <span>

#include <chess.hpp>

namespace chesswhy::tactics::detail {

[[nodiscard]] int see_without_piece(const chess::Board& board, chess::Square square,
                                    chess::Color side, chess::Square removed_square);

void see_without_piece(const chess::Board& board, std::span<const chess::Square> squares,
                       chess::Color side, chess::Square removed_square, std::span<int> results);

} // namespace chesswhy::tactics::detail
