#pragma once

#include <cstdint>
#include <vector>

#include <chess.hpp>

namespace chesswhy::tactics::detail {

enum class MoveSetLegality : std::uint8_t { Legal, PseudoLegal };

// PostM3_EM2 §4.1: the held piece's move set on `board` (its side to move): its legal or
// pseudo-legal moves, plus for a pawn a hypothetical capture onto each on-board square
// diagonally in front of it, whatever stands there (legal when its king is safe after it).
[[nodiscard]] std::vector<chess::Move>
held_piece_move_set(const chess::Board& board, chess::Square held, MoveSetLegality legality);

} // namespace chesswhy::tactics::detail
