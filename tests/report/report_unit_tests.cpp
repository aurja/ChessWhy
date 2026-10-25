#include "chesswhy/report.hpp"

#include <array>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "example_reports.hpp"

namespace {

[[nodiscard]] std::vector<std::string> keys(const nlohmann::ordered_json& value) {
    std::vector<std::string> result;
    for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
        result.push_back(iterator.key());
    }
    return result;
}

} // namespace

TEST_CASE("enum values use the schema spellings", "[unit]") {
    using namespace chesswhy;

    CHECK(to_string(Side::White) == "white");
    CHECK(to_string(Side::Black) == "black");
    CHECK(to_string(InputSource::Fen) == "fen");
    CHECK(to_string(InputSource::Pgn) == "pgn");
    CHECK(to_string(Terminal::Checkmate) == "checkmate");
    CHECK(to_string(Terminal::Stalemate) == "stalemate");
    CHECK(to_string(DrawKind::Stalemate) == "stalemate");
    CHECK(to_string(DrawKind::InsufficientMaterial) == "insufficient_material");
    CHECK(to_string(RefutationSkip::GameOver) == "game_over");
    CHECK(to_string(ConsequenceClass::Mate) == "mate");
    CHECK(to_string(ConsequenceClass::Material) == "material");
    CHECK(to_string(ConsequenceClass::Promotion) == "promotion");
    CHECK(to_string(ConsequenceClass::Positional) == "positional");
    CHECK(to_string(Verdict::Best) == "best");
    CHECK(to_string(Verdict::Okay) == "okay");
    CHECK(to_string(Verdict::Inaccuracy) == "inaccuracy");
    CHECK(to_string(Verdict::Mistake) == "mistake");
    CHECK(to_string(Verdict::Blunder) == "blunder");
    CHECK(to_string(UrgencyRule::InCheck) == "in_check");
    CHECK(to_string(UrgencyRule::Quiet) == "quiet");
    CHECK(to_string(UrgencyRule::ConsequenceClass) == "consequence_class");
    CHECK(to_string(UrgencyRule::Counterattack) == "counterattack");
    CHECK(to_string(UrgencyRule::Parry) == "parry");
    CHECK(to_string(ParryTest::Searched) == "searched");
    CHECK(to_string(ParryTest::ThreatMoveIllegal) == "threat_move_illegal");
    CHECK(to_string(ParryTest::DeferredByCheck) == "deferred_by_check");
    CHECK(to_string(CreatesThreatSkip::GivesCheck) == "gives_check");
    CHECK(to_string(CreatesThreatSkip::GameOver) == "game_over");
    CHECK(to_string(CreatesThreatSkip::NoLegalMove) == "no_legal_move");
    CHECK(to_string(TempoLabel::TempoDependent) == "tempo_dependent");
    CHECK(to_string(TempoLabel::NonTempo) == "non_tempo");
    CHECK(to_string(TempoLabel::Zugzwang) == "zugzwang");
    CHECK(to_string(TempoPlyLabel::Threatening) == "threatening");
    CHECK(to_string(TempoPlyLabel::PureReply) == "pure_reply");
    CHECK(to_string(TempoPlyLabel::CounterThreat) == "counter_threat");
    CHECK(to_string(TempoPlyLabel::Neither) == "neither");
    CHECK(to_string(TempoPlyLabel::GainingReply) == "gaining_reply");
    CHECK(to_string(TempoPlyLabel::UnforcedReply) == "unforced_reply");
    CHECK(to_string(TempoPlyLabel::UnresolvedReply) == "unresolved_reply");
    CHECK(to_string(InitiativeEnding::Spent) == "spent");
    CHECK(to_string(InitiativeEnding::Contested) == "contested");
    CHECK(to_string(InitiativeEnding::Runs) == "runs");
    CHECK(to_string(InitiativeEnding::Unfinished) == "unfinished");
    CHECK(to_string(InitiativeEnding::Answered) == "answered");
    CHECK(to_string(InitiativeEnding::Unresolved) == "unresolved");
    CHECK(to_string(InitiativeOutcome::Converted) == "converted");
    CHECK(to_string(InitiativeOutcome::Evaporated) == "evaporated");
    CHECK(to_string(InitiativeOutcome::Unknown) == "unknown");
    CHECK(to_string(TempoLedgerSkip::InCheck) == "in_check");
    CHECK(to_string(TempoLedgerSkip::NullMoveUnreliable) == "null_move_unreliable");
    CHECK(to_string(TempoLedgerSkip::NotDetailed) == "not_detailed");
    CHECK(to_string(HeldTo::Kind::File) == "file");
    CHECK(to_string(HeldTo::Kind::Rank) == "rank");
    CHECK(to_string(HeldTo::Kind::Diagonal) == "diagonal");
    CHECK(to_string(HeldTo::Kind::Square) == "square");
    CHECK(to_string(HeldTo::Kind::Squares) == "squares");
}

TEST_CASE("JSON follows v10 report key order", "[unit]") {
    using namespace chesswhy;

    const auto report = test_examples::normal_position();
    const auto json = to_json(report);
    const auto pgn = to_json(test_examples::pgn_opening());
    CHECK(keys(json) == std::vector<std::string>{
                            "schema_version", "document", "input", "engine", "position", "threats",
                            "candidates", "playable_truncated", "asked_moves", "opening", "games"});
    CHECK(keys(json["input"]) == std::vector<std::string>{
                                     "source", "fen", "pgn_game", "pgn_ply", "moves", "selection",
                                     "search_budget", "explanation_budget"});
    CHECK(json["input"]["search_budget"] == 18);
    CHECK(json["input"]["explanation_budget"] == 14);
    CHECK(keys(json["input"]["selection"]) == std::vector<std::string>{"keywords", "top", "moves"});
    CHECK(json["input"]["selection"]["moves"][0] == "g8f6");
    CHECK(keys(json["engine"]) == std::vector<std::string>{"name", "threads", "hash_mb", "multipv",
                                                           "reproducible", "nodes_searched"});
    CHECK(json["engine"]["nodes_searched"] == 5000);
    CHECK(keys(json["position"]) ==
          std::vector<std::string>{"fen", "side_to_move", "in_check", "legal_moves", "terminal",
                                   "eval_white", "tempo_value", "motifs", "positional"});
    CHECK(keys(json["position"]["tempo_value"]) ==
          std::vector<std::string>{"side", "moves", "passes", "value", "label", "source"});
    CHECK(keys(json["position"]["eval_white"]) ==
          std::vector<std::string>{"source", "es", "cp", "mate"});
    CHECK(keys(json["threats"]) ==
          std::vector<std::string>{"white", "black", "null_move_reliable", "urgency"});
    const std::vector<std::string> threat_keys{"source",    "side",        "root_fen",
                                               "line",      "consequence", "motifs",
                                               "is_tactic", "opportunity", "tactic_labels"};
    CHECK(keys(json["threats"]["white"]) == threat_keys);
    CHECK(keys(json["threats"]["black"]) == threat_keys);
    CHECK(json["threats"]["white"]["opportunity"].is_null());
    CHECK(keys(json["threats"]["white"]["consequence"]) ==
          std::vector<std::string>{"source", "class", "mate_in", "material", "promotes", "plies",
                                   "terminal_draw", "settled"});
    CHECK(keys(json["threats"]["urgency"]) ==
          std::vector<std::string>{"source", "more_urgent", "rule"});
    const std::vector<std::string> move_keys{"source",
                                             "san",
                                             "uci",
                                             "eval",
                                             "depth",
                                             "loss",
                                             "verdict",
                                             "in_multipv",
                                             "gives_check",
                                             "line",
                                             "walked_plies",
                                             "consequence",
                                             "is_tactic",
                                             "allows_tactic",
                                             "detailed",
                                             "parries_threat",
                                             "creates_threat",
                                             "creates_threat_skipped",
                                             "tempo_value",
                                             "tempo_ledger",
                                             "tempo_ledger_skipped",
                                             "refutation",
                                             "refutation_skipped",
                                             "motifs",
                                             "opportunity",
                                             "tactic_labels",
                                             "root_motifs",
                                             "positional"};
    CHECK(keys(json["candidates"][0]) == move_keys);
    CHECK(keys(json["candidates"][0]["creates_threat"]) == threat_keys);
    CHECK(json["candidates"][0]["refutation"].is_null());
    CHECK(keys(json["asked_moves"][0]) == move_keys);
    CHECK(keys(json["asked_moves"][0]["refutation"]) == threat_keys);
    CHECK(keys(json["asked_moves"][0]["parries_threat"]) ==
          std::vector<std::string>{"source", "value", "test"});
    CHECK(keys(json["opening"]) ==
          std::vector<std::string>{"source", "dataset", "current", "last_named"});
    CHECK(keys(json["games"]) == std::vector<std::string>{"source", "by_source"});
    CHECK(keys(json["games"]["by_source"][0]) ==
          std::vector<std::string>{"id", "table", "platform", "players", "months", "credit",
                                   "max_ply", "min_games", "found", "games", "white", "draws",
                                   "black", "moves"});
    CHECK(keys(json["games"]["by_source"][0]["moves"][0]) ==
          std::vector<std::string>{"san", "uci", "games"});
    CHECK(keys(json["candidates"][0]["positional"][0]) ==
          std::vector<std::string>{"source", "ply", "change", "holds_at_end", "net_change",
                                   "captured", "in_use", "tag", "side", "pieces", "file", "value"});
    CHECK(json["schema_version"] == 10);
    CHECK(json["document"] == "full");
    CHECK(json["position"]["positional"][0]["source"] == "rule");
    CHECK(json["position"]["positional"][0]["tag"] == "bishop_pair");
    CHECK(json["position"]["positional"][0]["file"].is_null());
    CHECK(json["candidates"][0]["positional"][0]["value"] == 12);
    CHECK(json["candidates"][0]["positional"][0]["source"] == "rule");
    CHECK(json["asked_moves"][0]["positional"][1]["file"] == "c");
    const auto& first_change = report.candidates[0].positional[0];
    CHECK(json["candidates"][0]["positional"][0]["net_change"] == first_change.net_change);
    CHECK(json["candidates"][0]["positional"][0]["in_use"].is_null() == !first_change.in_use);
    CHECK(json["asked_moves"][0]["positional"][1]["pieces"][0]["square"] == "c6");
    CHECK(json["asked_moves"][0]["positional"][0]["change"] == "disappears");
    CHECK(json["asked_moves"][0]["positional"][0]["holds_at_end"] == false);
    CHECK(json["games"]["source"] == "games");
    CHECK(json["games"]["by_source"][0]["found"] == true);
    CHECK(json["games"]["by_source"][0]["moves"][0]["uci"] == "g8f6");
    CHECK(keys(pgn["opening"]["current"]) == std::vector<std::string>{"eco", "name"});
    CHECK(keys(pgn["opening"]["last_named"]) == std::vector<std::string>{"eco", "name", "ply"});
}

TEST_CASE("v10 analysis fields and restriction serialize", "[unit]") {
    const auto json = chesswhy::to_json(chesswhy::test_examples::schema_example());
    const auto& position = json["position"];
    const auto& candidate = json["candidates"][0];

    CHECK(position["legal_moves"] == 10);
    CHECK(candidate["depth"] == 8);
    CHECK(candidate["walked_plies"] == 1);
    CHECK(candidate["creates_threat"].is_null());
    CHECK(candidate["creates_threat_skipped"] == "gives_check");
    CHECK(position["tempo_value"].is_null());
    CHECK(candidate["tempo_ledger"].is_null());
    CHECK(candidate["tempo_ledger_skipped"] == "null_move_unreliable");
    const auto& pin = position["motifs"][0];
    CHECK(pin["tag"] == "pin_absolute");
    CHECK(keys(pin["restriction"]) ==
          std::vector<std::string>{"piece", "restricted", "free", "held_to"});
    CHECK(pin["restriction"]["piece"]["square"] == "e7");
    CHECK(pin["restriction"]["restricted"].size() == 7);
    CHECK(pin["restriction"]["free"].size() == 6);
    CHECK(pin["restriction"]["held_to"]["kind"] == "file");
    CHECK(pin["restriction"]["held_to"]["name"] == "e");
}

TEST_CASE("tempo fields serialize in v10", "[unit]") {
    using namespace chesswhy;

    Report report = test_examples::normal_position();
    report.engine.nodes_searched = 40'000;
    report.position.tempo_value = TempoValue{.side = Side::Black,
                                             .moves_units = 1234,
                                             .passes_units = 606,
                                             .value_units = 628,
                                             .label = TempoLabel::TempoDependent};
    MoveAssessment& candidate = report.candidates.front();
    candidate.tempo_value = TempoValue{.side = Side::White,
                                       .moves_units = 200,
                                       .passes_units = 150,
                                       .value_units = 50,
                                       .label = TempoLabel::NonTempo};
    candidate.tempo_ledger =
        TempoLedger{.plies = {TempoPly{.ply = 1,
                                       .san = "e4",
                                       .side = Side::Black,
                                       .threatening = true,
                                       .by_check = false,
                                       .threat_rank = 2,
                                       .stops_threat = std::nullopt,
                                       .label = TempoPlyLabel::Threatening,
                                       .tempo_gained = true,
                                       .reply_gain = std::nullopt,
                                       .forced_test = std::nullopt},
                              TempoPly{.ply = 2,
                                       .san = "e5",
                                       .side = Side::White,
                                       .threatening = false,
                                       .by_check = false,
                                       .threat_rank = 0,
                                       .stops_threat = true,
                                       .label = TempoPlyLabel::PureReply,
                                       .tempo_gained = false,
                                       .reply_gain = std::nullopt,
                                       .forced_test = std::nullopt},
                              TempoPly{.ply = 3,
                                       .san = "Nf3",
                                       .side = Side::Black,
                                       .threatening = false,
                                       .by_check = false,
                                       .threat_rank = std::nullopt,
                                       .stops_threat = std::nullopt,
                                       .label = TempoPlyLabel::Neither,
                                       .tempo_gained = false,
                                       .reply_gain = std::nullopt,
                                       .forced_test = std::nullopt}},
                    .tempi = Tempi{.white = 0, .black = 1},
                    .runs = {InitiativeRun{.holder = Side::Black,
                                           .start_ply = 1,
                                           .end_ply = 3,
                                           .tempi = 1,
                                           .ending = InitiativeEnding::Spent,
                                           .outcome = InitiativeOutcome::Converted}}};
    candidate.tempo_ledger_skipped.reset();
    candidate.positional.front().net_change = true;
    candidate.positional.front().in_use = true;
    candidate.positional.front().feature.side = Side::Black;

    const auto json = to_json(report);
    const auto& root_value = json["position"]["tempo_value"];
    CHECK(keys(root_value) ==
          std::vector<std::string>{"side", "moves", "passes", "value", "label", "source"});
    CHECK(root_value["moves"].get<double>() == Catch::Approx(0.617));
    CHECK(root_value["passes"].get<double>() == Catch::Approx(0.303));
    CHECK(root_value["value"].get<double>() == Catch::Approx(0.314));
    CHECK(root_value["label"] == "tempo_dependent");
    CHECK(root_value["source"] == "engine");

    const auto& move = json["candidates"][0];
    CHECK(move["tempo_value"]["value"].get<double>() == Catch::Approx(0.025));
    CHECK(move["tempo_value"]["label"] == "non_tempo");
    const auto& ledger = move["tempo_ledger"];
    CHECK(keys(ledger["plies"][0]) ==
          std::vector<std::string>{"ply", "san", "side", "threatening", "by_check", "threat_rank",
                                   "stops_threat", "label", "tempo_gained", "reply_gain",
                                   "forced_test"});
    CHECK(ledger["plies"][0]["reply_gain"].is_null());
    CHECK(ledger["plies"][0]["forced_test"].is_null());
    CHECK(ledger["plies"][0]["label"] == "threatening");
    CHECK(ledger["plies"][0]["tempo_gained"] == true);
    CHECK(ledger["plies"][1]["stops_threat"] == true);
    CHECK(ledger["plies"][1]["label"] == "pure_reply");
    CHECK(keys(ledger["tempi"]) == std::vector<std::string>{"white", "black"});
    CHECK(ledger["tempi"]["black"] == 1);
    CHECK(keys(ledger["runs"][0]) == std::vector<std::string>{"holder", "start_ply", "end_ply",
                                                              "tempi", "ending", "outcome",
                                                              "source"});
    CHECK(ledger["runs"][0]["ending"] == "spent");
    CHECK(ledger["runs"][0]["outcome"] == "converted");
    CHECK(move["tempo_ledger_skipped"].is_null());
    CHECK(move["positional"][0]["net_change"] == true);
    CHECK(move["positional"][0]["in_use"] == true);
}

TEST_CASE("motif JSON uses the schema v10 shape", "[unit]") {
    const auto json = chesswhy::to_json(chesswhy::test_examples::normal_position());

    CHECK(json["schema_version"] == 10);
    CHECK(
        json["position"]["motifs"][0].dump() ==
        R"({"source":"rule","tag":"hanging_piece","side":"white","ply":0,"actor":{"square":"d5","piece":"n"},"targets":[],"gain":3,"threat":null,"restriction":null,"contribution":null})");
    CHECK_FALSE(json["position"]["motifs"][0].contains("also"));
    CHECK(json["threats"]["white"]["motifs"][0]["targets"].size() == 2);
    CHECK(json["candidates"][0]["motifs"][0]["tag"] == "promotion");
    CHECK(json["candidates"][0]["creates_threat"]["motifs"][0]["tag"] == "discovered_attack");
    CHECK(json["asked_moves"][0]["refutation"]["motifs"][0]["gain"] == 3);
    const auto pin = chesswhy::to_json(
        chesswhy::test_examples::tactic_with_labels())["candidates"][0]["motifs"][4];
    CHECK(keys(pin) == std::vector<std::string>{"source", "tag", "side", "ply", "actor", "targets",
                                                "gain", "threat", "restriction", "contribution"});
    CHECK(keys(pin["threat"]) == std::vector<std::string>{"kind", "square"});
    CHECK(pin["tag"] == "pin_tactical");
    CHECK(pin["threat"]["kind"] == "fork");
    CHECK(pin["threat"]["square"] == "f4");
}

TEST_CASE("opportunity judgements and tactic-line fields serialize in v10", "[unit]") {
    using namespace chesswhy;

    const auto forced = to_json(test_examples::forced_opportunity());
    CHECK(
        forced["candidates"][0]["opportunity"].dump() ==
        R"({"source":"rule","forced":true,"alternative":null,"margin":null,"is_opportunity":false})");

    const auto tactic = to_json(test_examples::tactic_with_labels());
    const auto& judgement = tactic["candidates"][0]["opportunity"];
    CHECK(keys(judgement) ==
          std::vector<std::string>{"source", "forced", "alternative", "margin", "is_opportunity"});
    CHECK(judgement["source"] == "engine");
    CHECK(judgement["alternative"]["san"] == "g6");
    CHECK(keys(judgement["alternative"]) == std::vector<std::string>{"san", "uci", "eval"});
    CHECK(keys(judgement["alternative"]["eval"]) ==
          std::vector<std::string>{"source", "es", "cp", "mate"});
    CHECK(judgement["alternative"]["eval"]["source"] == "engine");
    CHECK(judgement["alternative"]["eval"]["es"].get<double>() == Catch::Approx(0.127));
    CHECK(judgement["margin"].get<double>() == Catch::Approx(0.49));
    CHECK(judgement["is_opportunity"] == true);
    CHECK(tactic["candidates"][0]["tactic_labels"].size() == 1);
    CHECK(tactic["candidates"][0]["tactic_labels"][0]["tag"] == "removal_of_defender");
    CHECK(tactic["candidates"][0]["tactic_labels"][0]["targets"][1]["square"] == "e4");
    CHECK(tactic["position"]["motifs"][0]["contribution"].is_null());
    CHECK(tactic["candidates"][0]["motifs"][0]["contribution"] == "direct");
    CHECK(tactic["candidates"][0]["root_motifs"][0]["contribution"] == "indirect");

    const auto negative = to_json(test_examples::negative_margin_opportunity());
    CHECK(negative["candidates"][0]["opportunity"]["margin"].get<double>() == Catch::Approx(-0.05));
    CHECK(negative["candidates"][0]["opportunity"]["is_opportunity"] == false);
    CHECK(negative["candidates"][0]["tactic_labels"].empty());

    const auto refutation = to_json(test_examples::refutation_opportunity());
    CHECK(refutation["candidates"][1]["refutation"]["opportunity"]["margin"].get<double>() ==
          Catch::Approx(0.2));
    CHECK(refutation["candidates"][1]["refutation"]["is_tactic"] == true);
    CHECK(refutation["candidates"][1]["refutation"]["tactic_labels"].size() == 1);
    CHECK(refutation["candidates"][1]["refutation"]["motifs"][0]["contribution"] == "direct");
}

TEST_CASE("scores and optional values are serialized", "[unit]") {
    using namespace chesswhy;

    const auto normal = to_json(test_examples::normal_position());
    CHECK(normal["position"]["eval_white"]["es"].get<double>() == Catch::Approx(0.617));
    CHECK(normal["candidates"][0]["eval"]["es"].get<double>() == Catch::Approx(0.617));
    CHECK(normal["asked_moves"][0]["loss"].get<double>() == Catch::Approx(0.35));
    CHECK(normal["candidates"][0]["depth"] == 12);
    CHECK(normal["candidates"][0]["walked_plies"] == 1);
    CHECK(normal["candidates"][0]["creates_threat_skipped"].is_null());
    CHECK(normal["asked_moves"][0]["eval"]["mate"] == -1);
    CHECK(normal["candidates"][0]["parries_threat"].is_null());
    CHECK(normal["asked_moves"][0]["refutation"]["consequence"]["mate_in"] == 1);

    const auto terminal = to_json(test_examples::terminal_checkmate());
    CHECK(terminal["input"]["pgn_game"].is_null());
    CHECK(terminal["input"]["pgn_ply"].is_null());
    CHECK(terminal["input"]["moves"].is_null());
    CHECK(terminal["position"]["terminal"] == "checkmate");
    CHECK(terminal["position"]["eval_white"].is_null());
    CHECK(terminal["threats"].is_null());
    CHECK(terminal["candidates"].empty());
    CHECK(terminal["engine"]["nodes_searched"] == 0);
    CHECK(terminal["asked_moves"].empty());
    CHECK(terminal["opening"]["current"].is_null());
    CHECK(terminal["opening"]["last_named"].is_null());
    CHECK(terminal["games"]["source"] == "games");
    CHECK(terminal["games"]["by_source"][0]["found"] == false);
    CHECK(terminal["games"]["by_source"][0]["games"].is_null());
    CHECK(terminal["games"]["by_source"][0]["white"].is_null());
    CHECK(terminal["games"]["by_source"][0]["draws"].is_null());
    CHECK(terminal["games"]["by_source"][0]["black"].is_null());
    CHECK(terminal["games"]["by_source"][0]["moves"].empty());

    const auto checked = to_json(test_examples::side_in_check());
    CHECK(checked["threats"]["white"].is_null());
    CHECK(checked["threats"]["null_move_reliable"]["value"].is_null());
    CHECK(checked["threats"]["urgency"]["rule"] == "in_check");
    CHECK(checked["candidates"].size() == 1);

    const auto pgn = to_json(test_examples::pgn_opening());
    CHECK(pgn["input"]["source"] == "pgn");
    CHECK(pgn["input"]["pgn_game"] == 1);
    CHECK(pgn["input"]["pgn_ply"] == 2);
    CHECK(pgn["opening"]["current"]["eco"] == "C20");
    CHECK(pgn["opening"]["last_named"]["ply"] == 2);

    CHECK(normal.dump(2).find("\"es\": 0.617") != std::string::npos);
}
