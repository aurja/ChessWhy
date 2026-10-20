#pragma once

#include "chesswhy/report.hpp"

#include <span>
#include <vector>

#include <chess.hpp>

namespace chesswhy::oracle {

[[nodiscard]] int piece_value(chess::PieceType type) noexcept;
[[nodiscard]] int see(const chess::Board& board, chess::Square square, chess::Color side);
[[nodiscard]] std::vector<Motif> reference_position_motifs(const chess::Board& board);
[[nodiscard]] std::vector<Motif> reference_line_motifs(const chess::Board& root,
                                                       std::span<const chess::Move> line);

} // namespace chesswhy::oracle
