// Searches A–E and R and the rules that turn them into threats, urgency, verdicts and
// opportunities, tempo and initiative. Frozen by Planning/milestones/PostM4_EM3.md.
#pragma once

#include "chesswhy/engine.hpp"
#include "chesswhy/report.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>

namespace chesswhy::analysis {

inline constexpr int kMinMultiPv = 5;
inline constexpr int kDetailedLines = 5;
inline constexpr int kPlayableUnits = 100;
inline constexpr int kInaccuracyUnits = 200;
inline constexpr int kOpportunityMarginUnits = 260;
inline constexpr int kHorizonPlies = 6;          // H
inline constexpr int kCaptureExtension = 4;      // extra plies while the line keeps capturing
inline constexpr int kLinePlies = 10;            // plies kept in report lines
inline constexpr int kTempoDependentUnits = 200; // label tempo_dependent above this
inline constexpr int kNonTempoUnits = 100;       // label non_tempo from 0 below this

[[nodiscard]] NodeBudget node_budget(Budget budget);

struct Request {
    chess::Board board;
    Selection selection;
    std::vector<chess::Move> asked;
    Budget budget = Budget::Standard;
};

// Fills report.engine (except name/threads/hash, set by the caller from the engine),
// report.position, report.threats, report.candidates, report.playable_truncated and
// report.asked_moves. Runs no search for a terminal position.
void analyse(engine::Engine& engine, const Request& request, Report& report);

// ---- Pure rules (unit-tested without an engine) ----

// Expected score from WDL permille: 2W + D.
[[nodiscard]] int es_units(const engine::Line& line);

[[nodiscard]] bool judges_every_move(const Selection& selection);
[[nodiscard]] int multipv_lines(const Selection& selection, int legal_moves);
[[nodiscard]] bool is_detailed_line(int line_number, bool selected);
[[nodiscard]] std::uint64_t search_a_nodes(Budget budget, int lines);
[[nodiscard]] Verdict verdict(int loss_units, bool same_score_as_best, bool allows_tactic);
[[nodiscard]] bool same_score(const Eval& a, const Eval& b);
[[nodiscard]] bool is_tactic(const Consequence& consequence);
[[nodiscard]] bool allows_tactic(const Consequence& line_consequence, const Eval& eval);
[[nodiscard]] bool is_opportunity_margin(int margin_units);

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
[[nodiscard]] std::optional<TempoLabel> tempo_label(int value_units);
[[nodiscard]] bool tempo_dependent(const std::optional<TempoValue>& root);
// Sets label and tempo_gained from threatening and stops_threat (§6.2).
void label_ledger(std::vector<TempoPly>& plies);
[[nodiscard]] int material_balance(const chess::Board& board, chess::Color side); // §6.3
// §6.3; `line` has one move per ledger ply and `plies` are the output of label_ledger.
[[nodiscard]] std::vector<InitiativeRun> initiative_runs(std::span<const TempoPly> plies,
                                                         const chess::Board& root,
                                                         std::span<const chess::Move> line,
                                                         std::span<const PositionalChange> changes);

} // namespace chesswhy::analysis
