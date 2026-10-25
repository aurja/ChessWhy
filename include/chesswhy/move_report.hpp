// Move report document declarations for report v9.
#pragma once

#include "chesswhy/report.hpp"

#include <optional>
#include <string>
#include <vector>

namespace chesswhy {

enum class ImpactGroup : std::uint8_t {
    ParriedThreat,
    CreatedThreat,
    Motifs,
    Tempo,
    PositionalDynamic,
    PositionalStatic
};

struct TempoGained {
    int ply = 1;
    std::string san;
    Side side = Side::White;
};

struct ParriedThreat {
    Threat threat;
    ParryTest test = ParryTest::Searched;
};

struct MoveReport {
    std::optional<int> rank; // A line number (1-based); null: from search C
    std::string san, uci;
    Eval eval;
    int depth = 0;
    int loss_units = 0;
    Verdict verdict = Verdict::Best;
    bool gives_check = false;
    std::vector<std::string> line_san;
    int walked_plies = 0;
    std::vector<std::string> line_fens;
    Consequence consequence;
    bool is_tactic = false;
    bool allows_tactic = false;
    std::optional<OpportunityJudgement> opportunity;
    std::vector<Motif> motifs, root_motifs; // contribution direct or indirect only
    std::vector<TacticLabel> tactic_labels;
    std::vector<PositionalChange> positional; // net change (§4.6)
    std::optional<TempoValue> tempo_value;
    std::optional<std::vector<TempoGained>> tempo_gained; // null iff no ledger
    std::optional<std::vector<InitiativeRun>> initiative; // null iff no ledger
    std::vector<ImpactGroup> impact_order;
    std::optional<ParriedThreat> parried_threat;
    std::optional<Threat> created_threat; // mate, material or promotion; no motifs
    std::optional<Threat> refutation; // motifs filtered (§4.6)
    std::optional<RefutationSkip> refutation_skipped;
    std::optional<int> games;
};

struct SelectionInfo {
    Selection selection;
    int lines_searched = 0;
    int playable_count = 0;
    bool playable_truncated = false;
};

struct PositionSummary {
    std::string fen;
    Side side_to_move = Side::White;
    bool in_check = false;
    int legal_moves = 0;
    std::optional<Terminal> terminal;
    std::optional<Eval> eval_white;
    std::optional<TempoValue> tempo_value;
};

struct GameSourceSummary {
    std::string id;
    std::string table;
    std::string platform;
    std::string players;
    std::string months;
    std::string credit;
    int max_ply = 0;
    int min_games = 0;
    bool found = false;
    std::optional<int> games;
    std::optional<int> white;
    std::optional<int> draws;
    std::optional<int> black;
};

struct MoveReportDocument {
    int schema_version = kSchemaVersion;
    InputInfo input;
    EngineInfo engine;
    PositionSummary position;
    SelectionInfo selection;
    OpeningInfo opening;
    std::vector<GameSourceSummary> games;
    std::vector<MoveReport> moves;
};

[[nodiscard]] std::vector<ImpactGroup> impact_order(const MoveReport& move, bool tempo_dependent);

[[nodiscard]] MoveReportDocument move_reports(const Report& report); // §4.1, §4.6; pure
[[nodiscard]] nlohmann::ordered_json to_json(const MoveReportDocument& document);

} // namespace chesswhy
