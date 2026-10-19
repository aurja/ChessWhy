// Searches A–E and the rules that turn them into threats, urgency and verdicts.
// Frozen by Planning/milestones/M1.md §Analysis rules.
#pragma once

#include "chesswhy/engine.hpp"
#include "chesswhy/report.hpp"

#include <optional>
#include <span>
#include <string>

#include <chess.hpp>

namespace chesswhy::analysis {

inline constexpr int kMultiPv = 5;          // K
inline constexpr int kHorizonPlies = 6;     // H
inline constexpr int kCaptureExtension = 4; // extra plies while the line keeps capturing
inline constexpr int kLinePlies = 10;       // plies kept in report lines
// Loss thresholds in es_units (1/2000), Captain's decision for SF19's WDL curve:
// fine <= 0.03 < inaccuracy < 0.13 <= mistake < 0.27 <= blunder.
inline constexpr int kFineUnits = 60;
inline constexpr int kMistakeUnits = 260;
inline constexpr int kBlunderUnits = 540;

[[nodiscard]] NodeBudget node_budget(Budget budget);

struct Request {
    chess::Board board;
    std::optional<chess::Move> asked;
    Budget budget = Budget::Standard;
};

// Fills report.engine (except name/threads/hash, set by the caller from the engine),
// report.position, report.threats, report.candidates, report.fine_truncated and
// report.asked_move. Runs no search for a terminal position.
void analyse(engine::Engine& engine, const Request& request, Report& report);

// ---- Pure rules (unit-tested without an engine) ----

// Expected score from WDL permille: 2W + D.
[[nodiscard]] int es_units(const engine::Line& line);

[[nodiscard]] Verdict verdict_for_loss(int loss_units);

// Side to move has at most one piece other than king and pawns.
[[nodiscard]] bool null_move_prone(const chess::Board& board);

// Plies of `pv_uci` the consequence rule walks from `root` (M1 §4.3, M2 §5): the first
// kHorizonPlies plies, then further plies while each one is a capture, at most kLinePlies;
// stops early at the first illegal or missing move. Motif lists cover exactly these plies.
[[nodiscard]] int walked_plies(const chess::Board& root, std::span<const std::string> pv_uci);

// Consequence of `pv_uci` played from `root` for root's side to move, over the first
// walked_plies(root, pv_uci) plies (`plies` equals it). `root_line` is the engine line the
// pv came from (mate score, root side's point of view).
[[nodiscard]] Consequence consequence(const chess::Board& root, std::span<const std::string> pv_uci,
                                      const engine::Line& root_line);

// mate 4, material >= 3 → 3, material >= 1 → 2, promotion 1, positional 0.
[[nodiscard]] int consequence_rank(const Consequence& consequence);

} // namespace chesswhy::analysis
