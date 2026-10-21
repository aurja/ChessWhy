// Rule-based tactical motif detectors and tactic rules (provenance "rule"). Frozen by
// Planning/milestones/M2.md (definitions §4, report placement §5, signatures §6) and
// PostM2_EM1.md (contribution §4.3, removal_of_defender §4.4, signatures §6).
// Pure functions of a board and a move list: no engine, no global state, deterministic.
#pragma once

#include "chesswhy/report.hpp"

#include <span>
#include <vector>

#include <chess.hpp>

namespace chesswhy::tactics {

// Material in pawn units: PAWN 1, KNIGHT 3, BISHOP 3, ROOK 5, QUEEN 9, KING 0 (never
// exchanged). Where M2 §4 ranks the king above every piece, callers handle KING explicitly.
// The single piece-value table of the project (analysis uses it too).
[[nodiscard]] int piece_value(chess::PieceType type);

// see(board, square, side) of M2 §4: net material, in pawn units, that `side` wins by the
// best exchange sequence on `square`, `side` capturing first; both sides may stop; least
// valuable attacker first; x-ray attackers behind sliders join; an absolutely pinned
// piece captures only along its pin ray; the king captures only onto a square no longer
// attacked. Returns 0 when `square` holds no piece of the other side or `side` does not
// attack it. Ignores whose turn it is; never negative.
[[nodiscard]] int see(const chess::Board& board, chess::Square square, chess::Color side);

// Which motifs the detectors compute. The analysis uses the default (all). `pin_tactical`
// (PostM2_EM1 §4.5) costs milliseconds per board, so the puzzle-corpus tests switch it off
// outside their sample.
struct MotifOptions {
    bool pin_tactical = true;
};

// State motifs (M2 §4.1, PostM2_EM1 §4.5) of `board`, for both sides, all with ply 0,
// ordered per M2 §5. Empty when the side to move is checkmated or stalemated.
[[nodiscard]] std::vector<Motif> position_motifs(const chess::Board& board,
                                                 MotifOptions options = {});

// Motifs at plies 1..line.size() of `line` played from `root` (M2 §5): the move motifs
// (§4.2) of each move, for its mover, and the state motifs (§4.1, both sides) that hold
// after ply k but not after ply k-1 (ply 0 = `root`). A motif at ply k reads only the
// boards before and after ply k and move k itself, never later moves. `line` holds legal
// moves, already cut to the walked plies by the caller. Ordered per M2 §5.
[[nodiscard]] std::vector<Motif> line_motifs(const chess::Board& root,
                                             std::span<const chess::Move> line,
                                             MotifOptions options = {});

// Tactic labels (PostM2_EM1 §4.4) of the tactic sequence `line` played from `root`, for the
// side to move at `root` (X). `line` holds legal moves, already cut to the walked plies.
// Ordered by ply, then t's square.
[[nodiscard]] std::vector<TacticLabel> tactic_labels(const chess::Board& root,
                                                     std::span<const chess::Move> line);

// Sets `contribution` (PostM2_EM1 §4.3) on every motif of `motifs`, whose plies lie in
// 0..line.size() of the tactic sequence `line` played from `root`; X = side to move at
// `root`. Motifs of the other side get None. `line` holds legal moves, already cut to the
// walked plies.
void judge_contributions(const chess::Board& root, std::span<const chess::Move> line,
                         std::span<Motif> motifs);

} // namespace chesswhy::tactics
