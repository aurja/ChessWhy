#pragma once

#include "chesswhy/report.hpp"

#include <span>
#include <vector>

#include <chess.hpp>

namespace chesswhy::positional_oracle {

[[nodiscard]] int reference_mobility(const chess::Board& board, chess::Color side);
[[nodiscard]] int reference_space(const chess::Board& board, chess::Color side);
[[nodiscard]] int reference_undeveloped(const chess::Board& board, chess::Color side);
[[nodiscard]] std::vector<PositionalFeature> reference_features(const chess::Board& board);
[[nodiscard]] bool same_identity(const PositionalFeature& left, const PositionalFeature& right);
[[nodiscard]] std::vector<PositionalChange>
reference_line_changes(const chess::Board& root, std::span<const chess::Move> line);

} // namespace chesswhy::positional_oracle
