#include "chesswhy/move_report.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace {

using namespace chesswhy;

[[nodiscard]] MoveAssessment assessment(std::string san, std::string uci, Verdict verdict) {
    MoveAssessment value;
    value.san = std::move(san);
    value.uci = std::move(uci);
    value.eval = Eval{.es_units = 900, .cp = 10, .mate = std::nullopt};
    value.depth = 14;
    value.loss_units = 125;
    value.verdict = verdict;
    value.gives_check = true;
    value.line_san = {value.san, "Nf6"};
    value.walked_plies = 2;
    value.consequence = Consequence{.cls = ConsequenceClass::Material,
                                    .mate_in = std::nullopt,
                                    .material = 1,
                                    .promotes = false,
                                    .plies = 2};
    value.is_tactic = true;
    value.allows_tactic = verdict == Verdict::Blunder;
    return value;
}

[[nodiscard]] Report base_report() {
    Report report;
    report.input.fen = "8/8/8/8/8/8/8/K6k w - - 0 1";
    report.input.selection.keywords = {Selection::Keyword::Best};
    report.engine.name = "Test engine";
    report.engine.multipv = 6;
    report.position.fen = report.input.fen;
    report.position.side_to_move = Side::White;
    report.position.legal_moves = 6;
    report.position.eval_white = Eval{.es_units = 1000, .cp = 0, .mate = std::nullopt};
    report.opening.dataset = "test";
    GameSourceCounts games;
    games.id = "test_source";
    games.found = true;
    games.games = 5;
    games.white = 2;
    games.draws = 1;
    games.black = 2;
    games.moves.push_back(GameMoveCount{.san = "a3", .uci = "a2a3", .games = 4});
    report.games.by_source.push_back(std::move(games));
    report.candidates = {
        assessment("a3", "a2a3", Verdict::Best),    assessment("b3", "b2b3", Verdict::Best),
        assessment("c3", "c2c3", Verdict::Okay),    assessment("d3", "d2d3", Verdict::Inaccuracy),
        assessment("e3", "e2e3", Verdict::Mistake), assessment("f3", "f2f3", Verdict::Blunder),
    };
    return report;
}

[[nodiscard]] std::vector<std::string> ucis(const MoveReportDocument& document) {
    std::vector<std::string> result;
    for (const auto& move : document.moves) {
        result.push_back(move.uci);
    }
    return result;
}

[[nodiscard]] Motif motif_with(MotifContribution contribution) {
    Motif motif;
    motif.tag = MotifTag::Fork;
    motif.contribution = contribution;
    return motif;
}

[[nodiscard]] PositionalChange positional_change(PositionalTag tag, Side side, int value, int ply,
                                                 bool holds) {
    return PositionalChange{
        .ply = ply,
        .change = PositionalChangeKind::Appears,
        .holds_at_end = holds,
        .feature =
            PositionalFeature{
                .tag = tag,
                .side = side,
                .pieces = {},
                .file = std::nullopt,
                .value = value,
            },
    };
}

[[nodiscard]] Threat threat(ConsequenceClass cls) {
    Threat value;
    value.side = Side::Black;
    value.root_fen = "8/8/8/8/8/8/8/K6k b - - 0 1";
    value.line_san = {"Kf7"};
    value.consequence.cls = cls;
    value.consequence.material = 2;
    value.motifs = {motif_with(MotifContribution::Direct)};
    value.is_tactic = true;
    value.opportunity = OpportunityJudgement{.forced = false,
                                             .alternative = std::nullopt,
                                             .margin_units = std::nullopt,
                                             .is_opportunity = true};
    TacticLabel label;
    label.side = Side::Black;
    label.ply = 1;
    value.tactic_labels.push_back(label);
    return value;
}

} // namespace

TEST_CASE("move reports select the keyword sets and preserve A order", "[unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    auto report = base_report();

    const std::array cases{
        std::pair{Keyword::Best, std::vector<std::string>{"a2a3"}},
        std::pair{Keyword::Playable, std::vector<std::string>{"a2a3", "b2b3", "c2c3"}},
        std::pair{Keyword::Inaccuracies, std::vector<std::string>{"d2d3"}},
        std::pair{Keyword::Mistakes, std::vector<std::string>{"e2e3"}},
        std::pair{Keyword::Blunders, std::vector<std::string>{"f2f3"}},
        std::pair{Keyword::Unplayable, std::vector<std::string>{"d2d3", "e2e3", "f2f3"}},
        std::pair{Keyword::All,
                  std::vector<std::string>{"a2a3", "b2b3", "c2c3", "d2d3", "e2e3", "f2f3"}},
    };
    for (const auto& [keyword, expected] : cases) {
        report.input.selection = Selection{.keywords = {keyword}, .top = std::nullopt, .moves = {}};
        CHECK(ucis(move_reports(report)) == expected);
    }

    report.input.selection =
        Selection{.keywords = {Keyword::Inaccuracies}, .top = std::nullopt, .moves = {}};
    report.candidates[3].verdict = Verdict::Okay;
    CHECK(ucis(move_reports(report)).empty());
}

TEST_CASE("move report selection unions top, keywords, and asked moves once", "[unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    auto report = base_report();
    report.input.selection = Selection{.keywords = {Keyword::Best, Keyword::Mistakes},
                                       .top = 3,
                                       .moves = {"b2b3", "h2h3", "g2g3"}};
    report.asked_moves = {assessment("h3", "h2h3", Verdict::Blunder),
                          assessment("g3", "g2g3", Verdict::Okay)};
    report.playable_truncated = true;

    const auto document = move_reports(report);
    CHECK(ucis(document) ==
          std::vector<std::string>{"a2a3", "b2b3", "c2c3", "e2e3", "h2h3", "g2g3"});
    CHECK(document.moves[0].rank == 1);
    CHECK(document.moves[1].rank == 2);
    CHECK_FALSE(document.moves[4].rank.has_value());
    CHECK_FALSE(document.moves[5].rank.has_value());
    CHECK(document.selection.lines_searched == 6);
    CHECK(document.selection.playable_count == 3);
    CHECK(document.selection.playable_truncated == report.playable_truncated);
    report.input.selection = Selection{.keywords = {}, .top = 4, .moves = {}};
    CHECK(ucis(move_reports(report)) == std::vector<std::string>{"a2a3", "b2b3", "c2c3", "d2d3"});

    report.input.selection =
        Selection{.keywords = {}, .top = std::nullopt, .moves = {"g2g3", "b2b3", "h2h3"}};
    CHECK(ucis(move_reports(report)) == std::vector<std::string>{"b2b3", "g2g3", "h2h3"});
}

TEST_CASE("move report headers copy only the M4 position and game fields", "[unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    auto report = base_report();
    report.input.moves = "all";
    report.input.selection =
        Selection{.keywords = {Keyword::All}, .top = std::nullopt, .moves = {}};
    report.position.motifs.push_back(motif_with(MotifContribution::Direct));
    report.position.positional.push_back(
        positional_change(PositionalTag::MobilityLead, Side::White, 12, 1, true).feature);
    report.games.by_source.front().moves.push_back(
        GameMoveCount{.san = "c3", .uci = "c2c3", .games = 3});

    const auto document = move_reports(report);
    CHECK(document.schema_version == 6);
    CHECK(document.input.moves == "all");
    CHECK(document.engine.multipv == 6);
    CHECK(document.position.fen == report.position.fen);
    CHECK(document.position.eval_white->es_units == report.position.eval_white->es_units);
    CHECK(document.selection.selection.keywords == report.input.selection.keywords);
    CHECK(document.opening.dataset == report.opening.dataset);
    CHECK(document.games.size() == 1);
    CHECK(document.games[0].games == 5);
    CHECK(document.moves[0].games == 4);
    CHECK(document.moves[2].games == 3);
    auto no_games = report;
    no_games.games.by_source.front().found = false;
    CHECK_FALSE(move_reports(no_games).moves[0].games.has_value());

    report.position.terminal = Terminal::Stalemate;
    CHECK(move_reports(report).selection.lines_searched == 0);
    CHECK(move_reports(report).moves.empty());
}

TEST_CASE("move report relevance filter keeps conditional fields and drops irrelevant ones",
          "[unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    auto report = base_report();
    report.input.selection =
        Selection{.keywords = {Keyword::All}, .top = std::nullopt, .moves = {}};
    auto& best = report.candidates[0];
    auto& okay = report.candidates[2];
    best.opportunity = OpportunityJudgement{.forced = true,
                                            .alternative = std::nullopt,
                                            .margin_units = std::nullopt,
                                            .is_opportunity = false};
    best.motifs = {motif_with(MotifContribution::Direct), motif_with(MotifContribution::Indirect),
                   motif_with(MotifContribution::None), Motif{}};
    best.root_motifs = best.motifs;
    TacticLabel label;
    label.side = Side::White;
    label.ply = 1;
    best.tactic_labels.push_back(label);
    best.positional = {
        positional_change(PositionalTag::MobilityLead, Side::White, 10, 1, true),
        positional_change(PositionalTag::MobilityLead, Side::White, 20, 2, true),
        positional_change(PositionalTag::MobilityLead, Side::Black, 5, 3, true),
        positional_change(PositionalTag::SpaceLead, Side::White, 4, 4, false),
    };
    best.parries_threat = ParryInfo{.parries = true, .test = ParryTest::ThreatMoveIllegal};
    okay.parries_threat = ParryInfo{.parries = false, .test = ParryTest::Searched};
    report.threats.emplace();
    report.threats->black = threat(ConsequenceClass::Material);
    best.creates_threat = threat(ConsequenceClass::Mate);
    okay.creates_threat = threat(ConsequenceClass::Positional);

    auto& blunder = report.candidates[5];
    blunder.refutation = threat(ConsequenceClass::Material);
    blunder.refutation->motifs = {motif_with(MotifContribution::Direct),
                                  motif_with(MotifContribution::Indirect),
                                  motif_with(MotifContribution::None), Motif{}};
    blunder.refutation->opportunity = OpportunityJudgement{.forced = false,
                                                           .alternative = std::nullopt,
                                                           .margin_units = std::nullopt,
                                                           .is_opportunity = true};
    auto& playable_move_with_refutation = report.candidates[1];
    playable_move_with_refutation.refutation = threat(ConsequenceClass::Mate);

    const auto document = move_reports(report);
    const auto& best_report = document.moves[0];
    const auto& okay_report = document.moves[2];
    const auto& blunder_report = document.moves[5];
    CHECK(best_report.rank == 1);
    CHECK(best_report.eval.es_units == best.eval.es_units);
    CHECK(best_report.uci == best.uci);
    CHECK(best_report.depth == best.depth);
    CHECK(best_report.loss_units == best.loss_units);
    CHECK(best_report.verdict == best.verdict);
    CHECK(best_report.gives_check == best.gives_check);
    CHECK(best_report.line_san == best.line_san);
    CHECK(best_report.walked_plies == best.walked_plies);
    CHECK(best_report.consequence.material == best.consequence.material);
    CHECK(best_report.is_tactic == best.is_tactic);
    CHECK(best_report.allows_tactic == best.allows_tactic);
    CHECK(best_report.opportunity.has_value());
    CHECK(best_report.motifs.size() == 2);
    CHECK(best_report.motifs[0].contribution == MotifContribution::Direct);
    CHECK(best_report.motifs[1].contribution == MotifContribution::Indirect);
    CHECK(best_report.root_motifs.size() == 2);
    CHECK(best_report.tactic_labels.size() == best.tactic_labels.size());
    CHECK(best_report.tactic_labels[0].tag == best.tactic_labels[0].tag);
    CHECK(best_report.positional.size() == 2);
    CHECK(best_report.positional[0].ply == 2);
    CHECK(best_report.positional[0].feature.value == 20);
    CHECK(best_report.positional[1].feature.side == Side::Black);
    REQUIRE(best_report.parried_threat.has_value());
    CHECK(best_report.parried_threat->test == ParryTest::ThreatMoveIllegal);
    CHECK(best_report.parried_threat->threat.line_san == report.threats->black->line_san);
    CHECK(best_report.parried_threat->threat.motifs.empty());
    CHECK_FALSE(best_report.parried_threat->threat.is_tactic.has_value());
    CHECK(best_report.created_threat->consequence.cls == ConsequenceClass::Mate);
    CHECK(best_report.created_threat->motifs.empty());
    CHECK_FALSE(okay_report.parried_threat.has_value());
    CHECK_FALSE(okay_report.created_threat.has_value());
    CHECK_FALSE(okay_report.opportunity.has_value());
    CHECK_FALSE(document.moves[1].refutation.has_value());
    CHECK(blunder_report.refutation.has_value());
    CHECK(blunder_report.refutation->motifs.size() == 2);
    CHECK(blunder_report.refutation->motifs[1].contribution == MotifContribution::Indirect);
    CHECK(blunder_report.refutation->opportunity->is_opportunity);
    CHECK(okay_report.tactic_labels.empty());
    CHECK_FALSE(blunder_report.games.has_value());
    CHECK(best_report.games == report.games.by_source.front().moves.front().games);
}

TEST_CASE("move report created threat keeps only the specified consequence classes", "[unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    auto report = base_report();
    report.input.selection =
        Selection{.keywords = {Keyword::All}, .top = std::nullopt, .moves = {}};
    report.candidates[0].creates_threat = threat(ConsequenceClass::Material);
    report.candidates[1].creates_threat = threat(ConsequenceClass::Promotion);
    report.candidates[2].creates_threat = threat(ConsequenceClass::Positional);
    report.candidates[3].creates_threat = threat(ConsequenceClass::Mate);
    report.candidates[4].creates_threat.reset();
    report.candidates[5].creates_threat = threat(ConsequenceClass::Positional);

    const auto document = move_reports(report);
    CHECK(document.moves[0].created_threat.has_value());
    CHECK(document.moves[1].created_threat.has_value());
    CHECK_FALSE(document.moves[2].created_threat.has_value());
    CHECK(document.moves[3].created_threat.has_value());
    CHECK_FALSE(document.moves[4].created_threat.has_value());
    CHECK_FALSE(document.moves[5].created_threat.has_value());
}

TEST_CASE("move report JSON uses the v6 moves shape and stable key order", "[unit]") {
    auto report = base_report();
    report.candidates[0].parries_threat = ParryInfo{.parries = true};
    report.threats.emplace();
    report.threats->black = threat(ConsequenceClass::Material);
    report.candidates[0].refutation.reset();
    const auto json = to_json(move_reports(report));
    const auto keys = [](const nlohmann::ordered_json& value) {
        std::vector<std::string> result;
        for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
            result.push_back(iterator.key());
        }
        return result;
    };
    CHECK(keys(json) == std::vector<std::string>{"schema_version", "document", "input", "engine",
                                                 "position", "selection", "opening", "games",
                                                 "moves"});
    CHECK(keys(json["selection"]) == std::vector<std::string>{"keywords", "top", "moves",
                                                              "lines_searched", "playable_count",
                                                              "playable_truncated"});
    CHECK(keys(json["moves"][0]) ==
          std::vector<std::string>{
              "rank",        "san",           "uci",           "eval",           "depth",
              "loss",        "verdict",       "gives_check",   "line",           "walked_plies",
              "consequence", "is_tactic",     "allows_tactic", "opportunity",    "motifs",
              "root_motifs", "tactic_labels", "positional",    "parried_threat", "created_threat",
              "refutation",  "games"});
    CHECK(keys(json["moves"][0]["parried_threat"]) == std::vector<std::string>{"threat", "test"});
    CHECK(json["schema_version"] == 6);
    CHECK(json["document"] == "moves");
}
