#include "chesswhy/analysis.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace {

using nlohmann::json;

[[nodiscard]] std::string side_name(chesswhy::Side side) {
    return side == chesswhy::Side::White ? "white" : "black";
}

[[nodiscard]] std::string terminal_name(chesswhy::Terminal terminal) {
    return terminal == chesswhy::Terminal::Checkmate ? "checkmate" : "stalemate";
}

[[nodiscard]] std::string consequence_name(chesswhy::ConsequenceClass cls) {
    switch (cls) {
    case chesswhy::ConsequenceClass::Mate:
        return "mate";
    case chesswhy::ConsequenceClass::Material:
        return "material";
    case chesswhy::ConsequenceClass::Promotion:
        return "promotion";
    case chesswhy::ConsequenceClass::Positional:
        return "positional";
    }
    return "positional";
}

[[nodiscard]] std::string verdict_name(chesswhy::Verdict verdict) {
    switch (verdict) {
    case chesswhy::Verdict::Fine:
        return "fine";
    case chesswhy::Verdict::Inaccuracy:
        return "inaccuracy";
    case chesswhy::Verdict::Mistake:
        return "mistake";
    case chesswhy::Verdict::Blunder:
        return "blunder";
    }
    return "fine";
}

[[nodiscard]] std::string urgency_name(chesswhy::UrgencyRule rule) {
    switch (rule) {
    case chesswhy::UrgencyRule::InCheck:
        return "in_check";
    case chesswhy::UrgencyRule::Quiet:
        return "quiet";
    case chesswhy::UrgencyRule::ConsequenceClass:
        return "consequence_class";
    case chesswhy::UrgencyRule::Counterattack:
        return "counterattack";
    case chesswhy::UrgencyRule::Parry:
        return "parry";
    }
    return "quiet";
}

[[nodiscard]] std::string parry_test_name(chesswhy::ParryTest test) {
    switch (test) {
    case chesswhy::ParryTest::Searched:
        return "searched";
    case chesswhy::ParryTest::ThreatMoveIllegal:
        return "threat_move_illegal";
    case chesswhy::ParryTest::DeferredByCheck:
        return "deferred_by_check";
    }
    return "searched";
}

[[nodiscard]] json consequence_json(const chesswhy::Consequence& consequence) {
    return {{"class", consequence_name(consequence.cls)},
            {"mate_in", consequence.mate_in ? json(*consequence.mate_in) : json(nullptr)},
            {"material", consequence.material},
            {"promotes", consequence.promotes},
            {"plies", consequence.plies}};
}

[[nodiscard]] json threat_json(const chesswhy::Threat& threat) {
    return {{"side", side_name(threat.side)},
            {"root_fen", threat.root_fen},
            {"line", threat.line_san},
            {"consequence", consequence_json(threat.consequence)}};
}

[[nodiscard]] json move_json(const chesswhy::MoveAssessment& move) {
    const auto parry = move.parries_threat
                           ? json{{"value", move.parries_threat->parries},
                                  {"test", parry_test_name(move.parries_threat->test)}}
                           : json(nullptr);
    return {
        {"san", move.san},
        {"uci", move.uci},
        {"eval",
         {{"es_units", move.eval.es_units},
          {"cp", move.eval.cp ? json(*move.eval.cp) : json(nullptr)},
          {"mate", move.eval.mate ? json(*move.eval.mate) : json(nullptr)}}},
        {"loss_units", move.loss_units},
        {"verdict", verdict_name(move.verdict)},
        {"in_multipv", move.in_multipv},
        {"gives_check", move.gives_check},
        {"line", move.line_san},
        {"parries_threat", parry},
        {"creates_threat", move.creates_threat ? threat_json(*move.creates_threat) : json(nullptr)},
        {"refutation", move.refutation ? threat_json(*move.refutation) : json(nullptr)}};
}

[[nodiscard]] json report_view(const chesswhy::Report& report) {
    json position_terminal = nullptr;
    if (report.position.terminal) {
        position_terminal = terminal_name(*report.position.terminal);
    }
    json position_eval = nullptr;
    if (report.position.eval_white) {
        const auto& eval = *report.position.eval_white;
        position_eval = {{"es_units", eval.es_units},
                         {"cp", eval.cp ? json(*eval.cp) : json(nullptr)},
                         {"mate", eval.mate ? json(*eval.mate) : json(nullptr)}};
    }
    json position{{"fen", report.position.fen},
                  {"side_to_move", side_name(report.position.side_to_move)},
                  {"in_check", report.position.in_check},
                  {"terminal", position_terminal},
                  {"eval_white", position_eval}};

    json threats = nullptr;
    if (report.threats) {
        json white = report.threats->white ? threat_json(*report.threats->white) : json(nullptr);
        json black = report.threats->black ? threat_json(*report.threats->black) : json(nullptr);
        json reliable = nullptr;
        if (report.threats->null_move_reliable) {
            reliable = {{"value", *report.threats->null_move_reliable}};
        }
        json urgent_side = report.threats->urgency.more_urgent
                               ? json(side_name(*report.threats->urgency.more_urgent))
                               : json(nullptr);
        threats = {
            {"white", white},
            {"black", black},
            {"null_move_reliable", reliable},
            {"urgency",
             {{"more_urgent", urgent_side}, {"rule", urgency_name(report.threats->urgency.rule)}}}};
    }

    json candidates = json::array();
    for (const auto& candidate : report.candidates) {
        candidates.push_back(move_json(candidate));
    }
    json asked = report.asked_move ? move_json(*report.asked_move) : json(nullptr);
    json opening_current = nullptr;
    if (report.opening.current) {
        opening_current = {{"eco", report.opening.current->eco},
                           {"name", report.opening.current->name}};
    }
    return {{"position", position},     {"threats", threats},
            {"candidates", candidates}, {"fine_truncated", report.fine_truncated},
            {"asked_move", asked},      {"opening", {{"current", opening_current}}}};
}

[[nodiscard]] json fixture_file() {
    const auto path = std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures/positions.json";
    std::ifstream input(path);
    REQUIRE(input.good());
    return json::parse(input);
}

void check_fixture_expectations(const json& fixture, const chesswhy::Report& report) {
    const auto view = report_view(report);
    if (fixture.contains("equals")) {
        for (auto it = fixture["equals"].begin(); it != fixture["equals"].end(); ++it) {
            INFO("fixture " << fixture["id"] << " equality " << it.key() << " actual "
                            << view.at(json::json_pointer(it.key())).dump());
            CHECK(view.at(json::json_pointer(it.key())) == it.value());
        }
    }
    if (fixture.contains("min")) {
        for (auto it = fixture["min"].begin(); it != fixture["min"].end(); ++it) {
            INFO("fixture " << fixture["id"] << " minimum " << it.key());
            CHECK(view.at(json::json_pointer(it.key())).get<int>() >= it.value().get<int>());
        }
    }
    if (fixture.contains("line_prefix")) {
        for (auto it = fixture["line_prefix"].begin(); it != fixture["line_prefix"].end(); ++it) {
            const auto actual = view.at(json::json_pointer(it.key()));
            REQUIRE(actual.is_array());
            REQUIRE(actual.size() >= it.value().size());
            for (std::size_t index = 0; index < it.value().size(); ++index) {
                INFO("fixture " << fixture["id"] << " line prefix " << it.key());
                CHECK(actual[index] == it.value()[index]);
            }
        }
    }
    if (fixture.contains("verdict_in")) {
        for (auto it = fixture["verdict_in"].begin(); it != fixture["verdict_in"].end(); ++it) {
            const auto actual = view.at(json::json_pointer(it.key()));
            CHECK(std::find(it.value().begin(), it.value().end(), actual) != it.value().end());
        }
    }
    if (fixture.contains("fine_includes")) {
        for (const auto& san : fixture["fine_includes"]) {
            std::string candidate_sans;
            for (const auto& candidate : report.candidates) {
                if (!candidate_sans.empty()) {
                    candidate_sans += ", ";
                }
                candidate_sans += candidate.san;
                candidate_sans += '=';
                candidate_sans += verdict_name(candidate.verdict);
            }
            INFO("fixture " << fixture["id"] << " fine include " << san << " candidates "
                            << candidate_sans);
            const auto found = std::ranges::find_if(
                report.candidates, [&san](const chesswhy::MoveAssessment& candidate) {
                    return candidate.san == san.get<std::string>() &&
                           candidate.verdict == chesswhy::Verdict::Fine;
                });
            CHECK(found != report.candidates.end());
        }
    }
    if (fixture.contains("candidates_san")) {
        json actual = json::array();
        for (const auto& candidate : report.candidates) {
            actual.push_back(candidate.san);
        }
        CHECK(actual == fixture["candidates_san"]);
    }
    if (fixture.contains("candidate")) {
        for (auto it = fixture["candidate"].begin(); it != fixture["candidate"].end(); ++it) {
            const auto found = std::ranges::find_if(
                report.candidates, [&it](const chesswhy::MoveAssessment& candidate) {
                    return candidate.san == it.key();
                });
            REQUIRE(found != report.candidates.end());
            const auto candidate_view = move_json(*found);
            for (auto expected = it.value().begin(); expected != it.value().end(); ++expected) {
                INFO("fixture " << fixture["id"] << " candidate " << it.key() << " "
                                << expected.key());
                CHECK(candidate_view.at(json::json_pointer(expected.key())) == expected.value());
            }
        }
    }
}

} // namespace

TEST_CASE("analysis engine fixtures match their pinned expectations", "[analysis][engine]") {
    const auto binary = std::filesystem::path(CHESSWHY_ASSETS_DIR) / "engines/stockfish";
    REQUIRE(std::filesystem::exists(binary));
    const auto fixtures = fixture_file();

    for (const auto* const id : {"F03", "F06", "F11"}) {
        const auto found = std::ranges::find_if(
            fixtures["engine_fixtures"].begin(), fixtures["engine_fixtures"].end(),
            [id](const json& fixture) { return fixture["id"] == id; });
        REQUIRE(found != fixtures["engine_fixtures"].end());
        const auto& fixture = *found;
        const auto& args = fixture["args"];
        std::string fen;
        std::optional<std::string> asked_san;
        for (std::size_t index = 0; index < args.size(); ++index) {
            if (args[index] == "--fen") {
                fen = args[index + 1].get<std::string>();
            } else if (args[index] == "--move") {
                asked_san = args[index + 1].get<std::string>();
            }
        }
        REQUIRE_FALSE(fen.empty());
        const chess::Board board(fen);
        std::optional<chess::Move> asked;
        if (asked_san) {
            asked = chess::uci::parseSan(board, *asked_san);
        }
        const chesswhy::analysis::Request request{
            .board = board, .asked = asked, .budget = chesswhy::Budget::Standard};
        chesswhy::Report report;
        chesswhy::engine::Engine engine({.binary = binary, .threads = 1, .hash_mb = 64});
        report.engine.name = engine.name();
        report.engine.threads = 1;
        report.engine.hash_mb = 64;
        report.engine.reproducible = true;

        chesswhy::analysis::analyse(engine, request, report);
        check_fixture_expectations(fixture, report);
        if (id == std::string_view("F03")) {
            CHECK(report.engine.nodes_searched > 0);
            CHECK_FALSE(report.position.terminal.has_value());
        }
    }
}
