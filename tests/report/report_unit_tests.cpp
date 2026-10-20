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

TEST_CASE("report enum values use the schema spellings", "[unit]") {
    using namespace chesswhy;

    CHECK(to_string(Side::White) == "white");
    CHECK(to_string(Side::Black) == "black");
    CHECK(to_string(Budget::Quick) == "quick");
    CHECK(to_string(Budget::Standard) == "standard");
    CHECK(to_string(Budget::Deep) == "deep");
    CHECK(to_string(Level::Beginner) == "beginner");
    CHECK(to_string(Level::Club) == "club");
    CHECK(to_string(Level::Advanced) == "advanced");
    CHECK(to_string(InputSource::Fen) == "fen");
    CHECK(to_string(InputSource::Pgn) == "pgn");
    CHECK(to_string(Terminal::Checkmate) == "checkmate");
    CHECK(to_string(Terminal::Stalemate) == "stalemate");
    CHECK(to_string(ConsequenceClass::Mate) == "mate");
    CHECK(to_string(ConsequenceClass::Material) == "material");
    CHECK(to_string(ConsequenceClass::Promotion) == "promotion");
    CHECK(to_string(ConsequenceClass::Positional) == "positional");
    CHECK(to_string(Verdict::Fine) == "fine");
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
}

TEST_CASE("report JSON follows schema key order", "[unit]") {
    using namespace chesswhy;

    const auto report = test_examples::normal_position();
    const auto json = to_json(report);
    const auto pgn = to_json(test_examples::pgn_opening());
    CHECK(keys(json) == std::vector<std::string>{"schema_version", "input", "engine", "position",
                                                 "threats", "candidates", "fine_truncated",
                                                 "asked_move", "opening"});
    CHECK(keys(json["input"]) == std::vector<std::string>{"source", "fen", "pgn_game", "pgn_ply",
                                                          "move", "level", "budget"});
    CHECK(keys(json["engine"]) == std::vector<std::string>{"name", "threads", "hash_mb", "multipv",
                                                           "reproducible", "nodes",
                                                           "nodes_searched"});
    CHECK(keys(json["engine"]["nodes"]) == std::vector<std::string>{"A", "B", "C", "D", "E"});
    CHECK(keys(json["position"]) == std::vector<std::string>{"fen", "side_to_move", "in_check",
                                                             "terminal", "eval_white", "motifs"});
    CHECK(keys(json["position"]["eval_white"]) ==
          std::vector<std::string>{"source", "es", "cp", "mate"});
    CHECK(keys(json["threats"]) ==
          std::vector<std::string>{"white", "black", "null_move_reliable", "urgency"});
    CHECK(keys(json["threats"]["white"]) ==
          std::vector<std::string>{"source", "side", "root_fen", "line", "consequence", "motifs"});
    CHECK(keys(json["threats"]["white"]["consequence"]) ==
          std::vector<std::string>{"source", "class", "mate_in", "material", "promotes", "plies"});
    CHECK(keys(json["threats"]["urgency"]) ==
          std::vector<std::string>{"source", "more_urgent", "rule"});
    CHECK(keys(json["candidates"][0]) ==
          std::vector<std::string>{"source", "san", "uci", "eval", "loss", "verdict", "in_multipv",
                                   "gives_check", "line", "parries_threat", "creates_threat",
                                   "refutation", "motifs"});
    CHECK(keys(json["candidates"][0]["creates_threat"]) ==
          std::vector<std::string>{"source", "side", "root_fen", "line", "consequence", "motifs"});
    CHECK(keys(json["asked_move"]["parries_threat"]) ==
          std::vector<std::string>{"source", "value", "test"});
    CHECK(keys(json["opening"]) ==
          std::vector<std::string>{"source", "dataset", "current", "last_named"});
    CHECK(keys(pgn["opening"]["current"]) == std::vector<std::string>{"eco", "name"});
    CHECK(keys(pgn["opening"]["last_named"]) == std::vector<std::string>{"eco", "name", "ply"});
}

TEST_CASE("motif JSON uses the schema v2 shape", "[unit]") {
    const auto json = chesswhy::to_json(chesswhy::test_examples::normal_position());

    CHECK(json["schema_version"] == 2);
    CHECK(
        json["position"]["motifs"][0].dump() ==
        R"({"source":"rule","tag":"hanging_piece","side":"white","ply":0,"actor":{"square":"d5","piece":"n"},"targets":[],"gain":3,"contribution":null})");
    CHECK(json["threats"]["white"]["motifs"][0]["targets"].size() == 2);
    CHECK(json["candidates"][0]["motifs"][0]["tag"] == "promotion");
    CHECK(json["candidates"][0]["creates_threat"]["motifs"][0]["tag"] == "discovered_attack");
    CHECK(json["asked_move"]["motifs"][0]["tag"] == "skewer");
    CHECK(json["asked_move"]["refutation"]["motifs"][0]["gain"] == 3);
}

TEST_CASE("report scores and optional values are serialized", "[unit]") {
    using namespace chesswhy;

    const auto normal = to_json(test_examples::normal_position());
    CHECK(normal["position"]["eval_white"]["es"].get<double>() == Catch::Approx(0.617));
    CHECK(normal["candidates"][0]["eval"]["es"].get<double>() == Catch::Approx(0.617));
    CHECK(normal["asked_move"]["loss"].get<double>() == Catch::Approx(0.35));
    CHECK(normal["asked_move"]["eval"]["mate"].is_null());
    CHECK(normal["candidates"][0]["parries_threat"].is_null());
    CHECK(normal["asked_move"]["refutation"]["consequence"]["mate_in"] == 1);

    const auto terminal = to_json(test_examples::terminal_checkmate());
    CHECK(terminal["input"]["pgn_game"].is_null());
    CHECK(terminal["input"]["pgn_ply"].is_null());
    CHECK(terminal["input"]["move"].is_null());
    CHECK(terminal["position"]["terminal"] == "checkmate");
    CHECK(terminal["position"]["eval_white"].is_null());
    CHECK(terminal["threats"].is_null());
    CHECK(terminal["candidates"].empty());
    CHECK(terminal["engine"]["nodes_searched"] == 0);
    CHECK(terminal["asked_move"].is_null());
    CHECK(terminal["opening"]["current"].is_null());
    CHECK(terminal["opening"]["last_named"].is_null());

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
