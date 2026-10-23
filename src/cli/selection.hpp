#pragma once

#include "chesswhy/report.hpp"

#include <optional>
#include <string_view>
#include <vector>

#include <chess.hpp>

namespace chesswhy::cli::detail {

struct ResolvedMoves {
    Selection selection;
    std::vector<chess::Move> asked;
};

// Resolve the typed --moves value against its analysed position.
[[nodiscard]] ResolvedMoves resolve_moves(const chess::Board& board,
                                          std::optional<std::string_view> moves);

} // namespace chesswhy::cli::detail
