// Searches A–E and R and the rules that turn them into threats, urgency, verdicts and
// opportunities, tempo and initiative. Frozen by Planning/milestones/PostM4_EM5.md.
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

struct Walk {
    int plies = 0;
    bool settled = true;
    std::optional<TerminalDraw> terminal_draw;
};
[[nodiscard]] bool insufficient_material(const chess::Board& board); // P1
[[nodiscard]] bool game_over(const chess::Board& board); // §3.1
[[nodiscard]] bool continuation_move(const chess::Board& before, chess::Move move); // §3.1
[[nodiscard]] Walk walk(const chess::Board& root, std::span<const std::string> pv_uci); // P1, P3
[[nodiscard]] int settle_ply(const chess::Board& root, std::span<const chess::Move> line,
                             int k); // P4; line = the walked moves
// Position at ply 0 followed by the position after each move.
[[nodiscard]] std::vector<chess::Board>
board_positions(const chess::Board& root, std::span<const chess::Move> line);

// Consequence of `pv_uci` played from `root` for root's side to move, over the first
// `walk(root, pv_uci).plies` plies. `root_line` is the engine line the pv came from.

[[nodiscard]] Consequence consequence(const chess::Board& root, std::span<const std::string> pv_uci,
                                      const engine::Line& root_line);

// mate 4, material >= 3 → 3, material >= 1 → 2, promotion 1, positional 0.
[[nodiscard]] int consequence_rank(const Consequence& consequence);
[[nodiscard]] std::optional<TempoLabel> tempo_label(int value_units);
[[nodiscard]] bool tempo_dependent(const std::optional<TempoValue>& root);
// Sets label and tempo_gained from threatening and stops_threat (§6.2).
void label_ledger(std::vector<TempoPly>& plies);
// P6 option A: classify the returned acceptable alternatives.
[[nodiscard]] std::optional<bool> forced_test_result(std::span<const ReplyLine> lines,
                                                     int legal_moves);
[[nodiscard]] int material_balance(const chess::Board& board, chess::Color side); // §6.3
// P5 reply material, mate and settled result from the P4 settlement position.
[[nodiscard]] ReplyGain reply_gain(std::span<const chess::Board> positions, int k,
                                   int through_ply, bool walk_settled);
// §6.3/P8/P9; line has one move per ledger ply and plies are labelled by label_ledger.
[[nodiscard]] std::vector<InitiativeRun> initiative_runs(std::span<const TempoPly> plies,
                                                         const chess::Board& root,
                                                         std::span<const chess::Move> line,
                                                         std::span<const PositionalChange> changes,
                                                         bool walk_settled);

} // namespace chesswhy::analysis
