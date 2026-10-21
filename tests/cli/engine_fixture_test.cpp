#include "chesswhy/cli.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chess.hpp>
#include <nlohmann/json.hpp>

#ifndef CHESSWHY_SOURCE_DIR
#error "CHESSWHY_SOURCE_DIR must be provided by chesswhy_paths"
#endif
#ifndef CHESSWHY_CLI_REPORT_DIR
#error "CHESSWHY_CLI_REPORT_DIR must be configured"
#endif

namespace {

using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

[[nodiscard]] Json read_fixtures() {
    const auto path = std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures/positions.json";
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to read " + path.string());
    }
    return Json::parse(input);
}

[[nodiscard]] std::vector<std::string> fixture_args(const Json& json_args,
                                                    bool json_output = true) {
    std::vector<std::string> args;
    args.reserve(json_args.size() + (json_output ? 1U : 0U));
    for (const auto& value : json_args) {
        args.push_back(value.get<std::string>());
    }
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == "--pgn") {
            args[index + 1] =
                (std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures" / args[index + 1])
                    .string();
        }
    }
    if (json_output) {
        args.emplace_back("--json");
    }
    return args;
}

[[nodiscard]] Json run_fixture(const Json& fixture, std::string& output, std::string& error) {
    const auto args = fixture_args(fixture.at("args"));
    std::vector<std::string_view> arg_views;
    arg_views.reserve(args.size());
    for (const auto& arg : args) {
        arg_views.emplace_back(arg);
    }

    const auto options = chesswhy::cli::parse_args(arg_views);
    std::ostringstream out;
    std::ostringstream err;
    const int result = chesswhy::cli::run(options, out, err);
    output = out.str();
    error = err.str();
    if (result != chesswhy::cli::kExitOk) {
        throw std::runtime_error("cli::run returned " + std::to_string(result) + ": " + error);
    }
    if (!error.empty()) {
        throw std::runtime_error("cli::run wrote an unexpected diagnostic: " + error);
    }
    return Json::parse(output);
}

[[nodiscard]] std::string run_fixture_text(const Json& fixture) {
    const auto args = fixture_args(fixture.at("args"), false);
    std::vector<std::string_view> arg_views;
    arg_views.reserve(args.size());
    for (const auto& arg : args) {
        arg_views.emplace_back(arg);
    }

    const auto options = chesswhy::cli::parse_args(arg_views);
    std::ostringstream out;
    std::ostringstream err;
    const int result = chesswhy::cli::run(options, out, err);
    if (result != chesswhy::cli::kExitOk || !err.str().empty()) {
        throw std::runtime_error("text cli::run returned " + std::to_string(result) + ": " +
                                 err.str());
    }
    return out.str();
}

void check_value(bool condition, std::string_view fixture_id, std::string_view key,
                 const Json& expected, const Json* actual, std::vector<std::string>& failures) {
    if (condition) {
        return;
    }
    failures.push_back("fixture " + std::string(fixture_id) + " expectation " + std::string(key) +
                       ": expected " + expected.dump() + ", actual " +
                       (actual == nullptr ? "<missing>" : actual->dump()));
}

[[nodiscard]] const Json* find_pointer(const Json& value, const std::string& pointer) {
    try {
        return &value.at(Json::json_pointer(pointer));
    } catch (const Json::exception&) {
        return nullptr;
    }
}

void check_expectations(const Json& fixture, const Json& report,
                        std::vector<std::string>& failures) {
    const std::string id = fixture.at("id").get<std::string>();
    const auto check_mapped = [&](std::string_view category, const auto& check) {
        if (!fixture.contains(category)) {
            return;
        }
        for (auto expected = fixture.at(category).begin(); expected != fixture.at(category).end();
             ++expected) {
            const std::string& pointer = expected.key();
            const Json* actual = find_pointer(report, pointer);
            check(pointer, expected.value(), actual);
        }
    };

    check_mapped("equals",
                 [&](const std::string& pointer, const Json& expected, const Json* actual) {
                     check_value(actual != nullptr && *actual == expected, id, pointer, expected,
                                 actual, failures);
                 });
    check_mapped("min", [&](const std::string& pointer, const Json& expected, const Json* actual) {
        const bool matches = actual != nullptr && actual->is_number() && expected.is_number() &&
                             actual->get<double>() >= expected.get<double>();
        check_value(matches, id, pointer, expected, actual, failures);
    });
    check_mapped("line_prefix",
                 [&](const std::string& pointer, const Json& expected, const Json* actual) {
                     bool matches = actual != nullptr && actual->is_array() &&
                                    expected.is_array() && actual->size() >= expected.size();
                     if (matches) {
                         for (std::size_t index = 0; index < expected.size(); ++index) {
                             if ((*actual)[index] != expected[index]) {
                                 matches = false;
                                 break;
                             }
                         }
                     }
                     check_value(matches, id, pointer, expected, actual, failures);
                 });
    check_mapped("verdict_in",
                 [&](const std::string& pointer, const Json& expected, const Json* actual) {
                     const bool matches = actual != nullptr && expected.is_array() &&
                                          std::ranges::find(expected, *actual) != expected.end();
                     check_value(matches, id, pointer, expected, actual, failures);
                 });

    if (fixture.contains("fine_includes")) {
        const Json* candidates = find_pointer(report, "/candidates");
        for (const auto& san : fixture.at("fine_includes")) {
            bool found = false;
            if (candidates != nullptr && candidates->is_array()) {
                found = std::ranges::any_of(*candidates, [&](const Json& candidate) {
                    return candidate.value("san", std::string{}) == san.get<std::string>() &&
                           candidate.value("verdict", std::string{}) == "fine";
                });
            }
            const Json expected = {{"san", san}, {"verdict", "fine"}};
            check_value(found, id, "fine_includes:" + san.get<std::string>(), expected,
                        found ? &expected : nullptr, failures);
        }
    }
    if (fixture.contains("candidates_san")) {
        Json actual = Json::array();
        const Json* candidates = find_pointer(report, "/candidates");
        if (candidates != nullptr && candidates->is_array()) {
            for (const auto& candidate : *candidates) {
                actual.push_back(candidate.value("san", std::string{}));
            }
        }
        check_value(candidates != nullptr && actual == fixture.at("candidates_san"), id,
                    "candidates_san", fixture.at("candidates_san"),
                    candidates == nullptr ? nullptr : &actual, failures);
    }
    if (fixture.contains("candidate")) {
        const Json* candidates = find_pointer(report, "/candidates");
        for (auto candidate_expected = fixture.at("candidate").begin();
             candidate_expected != fixture.at("candidate").end(); ++candidate_expected) {
            const std::string& san = candidate_expected.key();
            const Json* candidate = nullptr;
            if (candidates != nullptr && candidates->is_array()) {
                const auto found = std::ranges::find_if(*candidates, [&](const Json& value) {
                    return value.value("san", std::string{}) == san;
                });
                if (found != candidates->end()) {
                    candidate = &*found;
                }
            }
            if (candidate == nullptr) {
                std::string message = "fixture ";
                message += id;
                message += " expectation candidate:";
                message += san;
                message += ": expected candidate to exist, actual <missing>";
                failures.push_back(std::move(message));
                continue;
            }
            for (auto expected = candidate_expected.value().begin();
                 expected != candidate_expected.value().end(); ++expected) {
                const std::string& pointer = expected.key();
                const Json* actual = find_pointer(*candidate, pointer);
                std::string expectation = "candidate:";
                expectation += san;
                expectation += pointer;
                check_value(actual != nullptr && *actual == expected.value(), id, expectation,
                            expected.value(), actual, failures);
            }
        }
    }
    if (fixture.contains("motifs_include")) {
        const auto& expectations = fixture.at("motifs_include");
        for (auto expected_array = expectations.begin(); expected_array != expectations.end();
             ++expected_array) {
            const std::string& pointer = expected_array.key();
            const Json* actual = find_pointer(report, pointer);
            for (const auto& instance : expected_array.value()) {
                const auto tag = instance.at("tag").get<std::string>();
                const auto side = instance.at("side").get<std::string>();
                const auto actor = instance.at("actor").get<std::string>();
                const bool found =
                    actual != nullptr && actual->is_array() &&
                    std::ranges::any_of(*actual, [&](const Json& motif) {
                        const Json* actor_square = find_pointer(motif, "/actor/square");
                        const bool base_match = motif.is_object() && actor_square != nullptr &&
                                                *actor_square == actor &&
                                                motif.value("tag", std::string{}) == tag &&
                                                motif.value("side", std::string{}) == side;
                        const bool contribution_matches =
                            !instance.contains("contribution") ||
                            (motif.is_object() && motif.value("contribution", Json(nullptr)) ==
                                                      instance.at("contribution"));
                        return base_match && contribution_matches;
                    });
                std::string expectation = "motifs_include:";
                expectation += pointer;
                expectation += ':';
                expectation += tag;
                expectation += ':';
                expectation += side;
                expectation += ':';
                expectation += actor;
                check_value(found, id, expectation, instance, actual, failures);
            }
        }
    }
    if (fixture.contains("positional_include")) {
        const auto& expectations = fixture.at("positional_include");
        for (auto expected_array = expectations.begin(); expected_array != expectations.end();
             ++expected_array) {
            const std::string& pointer = expected_array.key();
            const Json* actual = find_pointer(report, pointer);
            for (const auto& instance : expected_array.value()) {
                const bool found =
                    actual != nullptr && actual->is_array() && instance.is_object() &&
                    std::ranges::any_of(*actual, [&](const Json& feature) {
                        if (!feature.is_object()) {
                            return false;
                        }
                        for (auto field = instance.begin(); field != instance.end(); ++field) {
                            if (!feature.contains(field.key()) ||
                                feature.at(field.key()) != field.value()) {
                                return false;
                            }
                        }
                        return true;
                    });
                std::string expectation = "positional_include:";
                expectation += pointer;
                check_value(found, id, expectation, instance, actual, failures);
            }
        }
    }
}

constexpr int kTacticMarginUnits = 260;

void check_invariant(bool condition, std::string_view fixture_id, std::string_view key,
                     std::string_view expected, std::string_view actual,
                     std::vector<std::string>& failures) {
    if (!condition) {
        failures.push_back("fixture " + std::string(fixture_id) + " invariant " + std::string(key) +
                           ": expected " + std::string(expected) + ", actual " +
                           std::string(actual));
    }
}

[[nodiscard]] const Json* member(const Json& object, std::string_view key) {
    if (!object.is_object() || !object.contains(key)) {
        return nullptr;
    }
    return &object.at(key);
}

[[nodiscard]] std::optional<int> score_units(const Json* eval) {
    if (eval == nullptr || !eval->is_object() || !eval->contains("es") ||
        !eval->at("es").is_number()) {
        return std::nullopt;
    }
    return static_cast<int>(std::llround(eval->at("es").get<double>() * 2000.0));
}

[[nodiscard]] std::size_t legal_move_count(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    return moves.size();
}

[[nodiscard]] std::optional<std::string> uci_for_san(const chess::Board& board,
                                                     std::string_view san) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const auto move : moves) {
        if (chess::uci::moveToSan(board, move) == san) {
            return chess::uci::moveToUci(move, board.chess960());
        }
    }
    return std::nullopt;
}

void check_motif_contributions(const Json* motifs, bool expected, std::string_view fixture_id,
                               std::string_view key, std::vector<std::string>& failures) {
    if (motifs == nullptr || !motifs->is_array()) {
        check_invariant(false, fixture_id, key, "motif array", "<missing or not an array>",
                        failures);
        return;
    }
    for (std::size_t index = 0; index < motifs->size(); ++index) {
        const Json& motif = (*motifs)[index];
        const Json* contribution = member(motif, "contribution");
        const bool present = contribution != nullptr && !contribution->is_null();
        check_invariant(present == expected, fixture_id,
                        std::string(key) + "[" + std::to_string(index) + "].contribution",
                        expected ? "non-null" : "null",
                        contribution == nullptr ? "<missing>" : contribution->dump(), failures);
    }
}

void check_empty_labels_when_not_tactic(const Json* tactic, bool is_tactic,
                                        std::string_view fixture_id, std::string_view key,
                                        std::vector<std::string>& failures) {
    if (tactic == nullptr || !tactic->is_object() || is_tactic) {
        return;
    }
    const Json* labels = member(*tactic, "labels");
    const bool empty = labels != nullptr && labels->is_array() && labels->empty();
    check_invariant(empty, fixture_id, std::string(key) + ".labels", "[]",
                    labels == nullptr ? "<missing>" : labels->dump(), failures);
}

[[nodiscard]] bool check_refutation(const Json& move, std::string_view fixture_id,
                                    std::string_view key, std::vector<std::string>& failures) {
    const Json* refutation = member(move, "refutation");
    if (refutation == nullptr || refutation->is_null()) {
        return false;
    }
    const Json* line = member(*refutation, "line");
    const bool has_ply = line != nullptr && line->is_array() && !line->empty();
    const Json* tactic = member(*refutation, "tactic");
    if (!has_ply) {
        check_invariant(tactic != nullptr && tactic->is_null(), fixture_id,
                        std::string(key) + ".refutation.tactic", "null",
                        tactic == nullptr ? "<missing>" : tactic->dump(), failures);
        check_motif_contributions(member(*refutation, "motifs"), false, fixture_id,
                                  std::string(key) + ".refutation.motifs", failures);
        return false;
    }

    const bool judged = tactic != nullptr && tactic->is_object();
    check_invariant(judged, fixture_id, std::string(key) + ".refutation.tactic", "judgement object",
                    tactic == nullptr ? "<missing>" : tactic->dump(), failures);
    if (!judged) {
        check_motif_contributions(member(*refutation, "motifs"), false, fixture_id,
                                  std::string(key) + ".refutation.motifs", failures);
        return false;
    }

    const chess::Board root(refutation->at("root_fen").get<std::string>());
    const bool forced_expected = legal_move_count(root) == 1;
    const bool forced_actual = tactic->value("forced", false);
    check_invariant(forced_actual == forced_expected, fixture_id,
                    std::string(key) + ".refutation.tactic.forced",
                    forced_expected ? "true" : "false", forced_actual ? "true" : "false", failures);
    const Json* alternative = member(*tactic, "alternative");
    const Json* margin = member(*tactic, "margin");
    bool is_tactic_expected = false;

    if (forced_expected) {
        check_invariant(alternative != nullptr && alternative->is_null(), fixture_id,
                        std::string(key) + ".refutation.tactic.alternative", "null",
                        alternative == nullptr ? "<missing>" : alternative->dump(), failures);
        check_invariant(margin != nullptr && margin->is_null(), fixture_id,
                        std::string(key) + ".refutation.tactic.margin", "null",
                        margin == nullptr ? "<missing>" : margin->dump(), failures);
    } else {
        const bool has_alternative = alternative != nullptr && alternative->is_object();
        check_invariant(has_alternative, fixture_id,
                        std::string(key) + ".refutation.tactic.alternative", "object",
                        alternative == nullptr ? "<missing>" : alternative->dump(), failures);
        if (has_alternative) {
            const Json* move_eval = member(move, "eval");
            const Json* alternative_eval = member(*alternative, "eval");
            const auto move_score = score_units(move_eval);
            const auto alternative_score = score_units(alternative_eval);
            const auto* actual_alt_uci = member(*alternative, "uci");
            const auto first_uci = uci_for_san(root, line->front().get<std::string>());
            if (first_uci.has_value() && actual_alt_uci != nullptr && actual_alt_uci->is_string()) {
                check_invariant(actual_alt_uci->get<std::string>() != *first_uci, fixture_id,
                                std::string(key) + ".refutation.tactic.alternative.uci",
                                "different from refutation first move", actual_alt_uci->dump(),
                                failures);
            } else {
                check_invariant(
                    false, fixture_id, std::string(key) + ".refutation.tactic.alternative.uci",
                    "resolvable alternative and refutation first move",
                    actual_alt_uci == nullptr ? "<missing>" : actual_alt_uci->dump(), failures);
            }
            if (move_score.has_value() && alternative_score.has_value()) {
                const int expected_margin = (2000 - *move_score) - *alternative_score;
                const bool margin_matches =
                    margin != nullptr && margin->is_number() &&
                    static_cast<int>(std::llround(margin->get<double>() * 2000.0)) ==
                        expected_margin;
                check_invariant(margin_matches, fixture_id,
                                std::string(key) + ".refutation.tactic.margin",
                                std::to_string(expected_margin) + " es_units",
                                margin == nullptr ? "<missing>" : margin->dump(), failures);
                is_tactic_expected = expected_margin >= kTacticMarginUnits;
            } else {
                check_invariant(false, fixture_id, std::string(key) + ".refutation.tactic.margin",
                                "derivable from engine evals", "<missing eval>", failures);
            }
        }
    }

    const bool is_tactic_actual = tactic->value("is_tactic", false);
    check_invariant(is_tactic_actual == is_tactic_expected, fixture_id,
                    std::string(key) + ".refutation.tactic.is_tactic",
                    is_tactic_expected ? "true" : "false", is_tactic_actual ? "true" : "false",
                    failures);
    check_empty_labels_when_not_tactic(tactic, is_tactic_expected, fixture_id,
                                       std::string(key) + ".refutation.tactic", failures);
    check_motif_contributions(member(*refutation, "motifs"), is_tactic_expected, fixture_id,
                              std::string(key) + ".refutation.motifs", failures);
    return is_tactic_expected;
}

void check_move_motifs(const Json& move, bool root_line, bool root_is_tactic,
                       std::string_view fixture_id, std::string_view key,
                       std::vector<std::string>& failures) {
    check_motif_contributions(member(move, "motifs"), root_line && root_is_tactic, fixture_id,
                              std::string(key) + ".motifs", failures);
    const Json* created = member(move, "creates_threat");
    if (created != nullptr && !created->is_null()) {
        check_motif_contributions(member(*created, "motifs"), false, fixture_id,
                                  std::string(key) + ".creates_threat.motifs", failures);
        const Json* tactic = member(*created, "tactic");
        check_invariant(tactic != nullptr && tactic->is_null(), fixture_id,
                        std::string(key) + ".creates_threat.tactic", "null",
                        tactic == nullptr ? "<missing>" : tactic->dump(), failures);
    }
    static_cast<void>(check_refutation(move, fixture_id, key, failures));
}

void check_tactic_invariants(const Json& report, std::string_view fixture_id,
                             std::vector<std::string>& failures) {
    const Json* terminal = find_pointer(report, "/position/terminal");
    const bool nonterminal = terminal == nullptr || terminal->is_null();
    const Json* candidates = member(report, "candidates");
    bool root_is_tactic = false;
    std::size_t root_moves = 0;

    if (nonterminal) {
        const Json* fen = find_pointer(report, "/position/fen");
        if (fen != nullptr && fen->is_string()) {
            root_moves = legal_move_count(chess::Board(fen->get<std::string>()));
        }
        const bool has_first =
            candidates != nullptr && candidates->is_array() && !candidates->empty();
        const Json* first = has_first ? &(*candidates)[0] : nullptr;
        const Json* tactic = first == nullptr ? nullptr : member(*first, "tactic");
        check_invariant(tactic != nullptr && tactic->is_object(), fixture_id,
                        "candidates[0].tactic", "judgement object",
                        tactic == nullptr ? "<missing>" : tactic->dump(), failures);

        if (tactic != nullptr && tactic->is_object()) {
            const bool forced_expected = root_moves == 1;
            const bool forced_actual = tactic->value("forced", false);
            check_invariant(forced_actual == forced_expected, fixture_id,
                            "candidates[0].tactic.forced", forced_expected ? "true" : "false",
                            forced_actual ? "true" : "false", failures);
            const Json* alternative = member(*tactic, "alternative");
            const Json* margin = member(*tactic, "margin");
            if (forced_expected) {
                check_invariant(alternative != nullptr && alternative->is_null(), fixture_id,
                                "candidates[0].tactic.alternative", "null",
                                alternative == nullptr ? "<missing>" : alternative->dump(),
                                failures);
                check_invariant(margin != nullptr && margin->is_null(), fixture_id,
                                "candidates[0].tactic.margin", "null",
                                margin == nullptr ? "<missing>" : margin->dump(), failures);
            } else if (candidates->size() >= 2) {
                const Json& second = (*candidates)[1];
                const bool has_alternative = alternative != nullptr && alternative->is_object();
                check_invariant(
                    has_alternative, fixture_id, "candidates[0].tactic.alternative", "object",
                    alternative == nullptr ? "<missing>" : alternative->dump(), failures);
                if (has_alternative) {
                    for (const std::string_view field : {"san", "uci", "eval"}) {
                        const Json* actual = member(*alternative, field);
                        const Json* expected = member(second, field);
                        const bool equal =
                            actual != nullptr && expected != nullptr && *actual == *expected;
                        check_invariant(equal, fixture_id,
                                        "candidates[0].tactic.alternative." + std::string(field),
                                        expected == nullptr ? "<missing>" : expected->dump(),
                                        actual == nullptr ? "<missing>" : actual->dump(), failures);
                    }
                }
                const auto first_score = score_units(member(*first, "eval"));
                const auto second_score = score_units(member(second, "eval"));
                if (first_score.has_value() && second_score.has_value()) {
                    const int expected_margin = *first_score - *second_score;
                    const bool margin_matches =
                        margin != nullptr && margin->is_number() &&
                        static_cast<int>(std::llround(margin->get<double>() * 2000.0)) ==
                            expected_margin;
                    check_invariant(margin_matches, fixture_id, "candidates[0].tactic.margin",
                                    std::to_string(expected_margin) + " es_units",
                                    margin == nullptr ? "<missing>" : margin->dump(), failures);
                    root_is_tactic = expected_margin >= kTacticMarginUnits;
                } else {
                    check_invariant(false, fixture_id, "candidates[0].tactic.margin",
                                    "derivable from candidate evals", "<missing eval>", failures);
                }
            } else {
                check_invariant(false, fixture_id, "candidates[1]",
                                "second candidate for a non-forced position", "<missing>",
                                failures);
            }
            const bool is_tactic_actual = tactic->value("is_tactic", false);
            check_invariant(is_tactic_actual == root_is_tactic, fixture_id,
                            "candidates[0].tactic.is_tactic", root_is_tactic ? "true" : "false",
                            is_tactic_actual ? "true" : "false", failures);
            check_empty_labels_when_not_tactic(tactic, root_is_tactic, fixture_id,
                                               "candidates[0].tactic", failures);
        }

        if (candidates != nullptr && candidates->is_array()) {
            for (std::size_t index = 0; index < candidates->size(); ++index) {
                const Json& candidate = (*candidates)[index];
                if (index > 0) {
                    const Json* candidate_tactic = member(candidate, "tactic");
                    check_invariant(
                        candidate_tactic != nullptr && candidate_tactic->is_null(), fixture_id,
                        "candidates[" + std::to_string(index) + "].tactic", "null",
                        candidate_tactic == nullptr ? "<missing>" : candidate_tactic->dump(),
                        failures);
                }
                check_move_motifs(candidate, index == 0, root_is_tactic, fixture_id,
                                  "candidates[" + std::to_string(index) + "]", failures);
            }
        }
    }

    check_motif_contributions(find_pointer(report, "/position/motifs"), root_is_tactic, fixture_id,
                              "position.motifs", failures);

    const Json* threats = member(report, "threats");
    if (threats != nullptr && threats->is_object()) {
        const Json* side = find_pointer(report, "/position/side_to_move");
        const std::string own_side =
            side != nullptr && side->is_string() ? side->get<std::string>() : std::string{};
        for (const std::string_view color : {"white", "black"}) {
            const Json* threat = member(*threats, color);
            if (threat == nullptr || threat->is_null()) {
                continue;
            }
            const bool root_threat = own_side == color;
            check_motif_contributions(member(*threat, "motifs"), root_threat && root_is_tactic,
                                      fixture_id, "threats." + std::string(color) + ".motifs",
                                      failures);
            const Json* tactic = member(*threat, "tactic");
            check_invariant(tactic != nullptr && tactic->is_null(), fixture_id,
                            "threats." + std::string(color) + ".tactic", "null",
                            tactic == nullptr ? "<missing>" : tactic->dump(), failures);
        }
    }

    const Json* asked = member(report, "asked_move");
    if (asked != nullptr && !asked->is_null()) {
        const Json* matching_candidate = nullptr;
        if (candidates != nullptr && candidates->is_array()) {
            const std::string asked_uci = asked->value("uci", std::string{});
            const auto found = std::ranges::find_if(*candidates, [&](const Json& candidate) {
                return candidate.value("uci", std::string{}) == asked_uci;
            });
            if (found != candidates->end()) {
                matching_candidate = &*found;
            }
        }
        const Json* asked_tactic = member(*asked, "tactic");
        const Json* candidate_tactic =
            matching_candidate == nullptr ? nullptr : member(*matching_candidate, "tactic");
        const bool tactic_copied =
            matching_candidate != nullptr && candidate_tactic != nullptr
                ? asked_tactic != nullptr && *asked_tactic == *candidate_tactic
                : asked_tactic != nullptr && asked_tactic->is_null();
        std::string expected_tactic;
        if (matching_candidate == nullptr) {
            expected_tactic = "null";
        } else if (candidate_tactic == nullptr) {
            expected_tactic = "<missing>";
        } else {
            expected_tactic = candidate_tactic->dump();
        }
        check_invariant(tactic_copied, fixture_id, "asked_move.tactic", expected_tactic,
                        asked_tactic == nullptr ? "<missing>" : asked_tactic->dump(), failures);
        const bool asked_is_root = matching_candidate != nullptr && candidates != nullptr &&
                                   candidates->is_array() && !candidates->empty() &&
                                   matching_candidate == &(*candidates)[0];
        check_move_motifs(*asked, asked_is_root, root_is_tactic, fixture_id, "asked_move",
                          failures);
    }
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: cli_fixture_test FIXTURE_ID CHECK_TIMING\n";
        return 2;
    }
    const std::string fixture_id = argv[1];
    const bool check_timing = std::string_view(argv[2]) == "1";

    try {
        const Json fixture_file = read_fixtures();
        const auto& fixtures = fixture_file.at("engine_fixtures");
        const auto found = std::ranges::find_if(fixtures, [&](const Json& fixture) {
            return fixture.at("id").get<std::string>() == fixture_id;
        });
        if (found == fixtures.end()) {
            throw std::runtime_error("unknown engine fixture " + fixture_id);
        }

        const auto start = Clock::now();
        std::string output;
        std::string error;
        const Json report = run_fixture(*found, output, error);
        const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();

        const std::filesystem::path report_directory(CHESSWHY_CLI_REPORT_DIR);
        std::filesystem::create_directories(report_directory);
        const auto report_path = report_directory / (fixture_id + ".json");
        std::ofstream report_file(report_path, std::ios::binary);
        if (!report_file) {
            throw std::runtime_error("unable to write " + report_path.string());
        }
        report_file << output;
        report_file.close();
        if (!report_file) {
            throw std::runtime_error("unable to finish writing " + report_path.string());
        }

        std::vector<std::string> failures;
        check_expectations(*found, report, failures);
        check_tactic_invariants(report, fixture_id, failures);
        if (found->contains("text_contains")) {
            const std::string expected_text = found->at("text_contains").get<std::string>();
            const std::string rendered_text = run_fixture_text(*found);
            const Json actual_text = rendered_text;
            check_value(rendered_text.find(expected_text) != std::string::npos, fixture_id,
                        "text_contains", Json(expected_text), &actual_text, failures);
        }
        if (check_timing && elapsed > 12.0) {
            failures.push_back("fixture " + fixture_id + " wall time: expected <= 12 s, actual " +
                               std::to_string(elapsed) + " s");
        }
        std::cout << "fixture " << fixture_id << " wall time: " << elapsed << " s\n";
        for (const auto& failure : failures) {
            std::cerr << failure << '\n';
        }
        return failures.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "fixture " << fixture_id << " failed: " << error.what() << '\n';
        return 1;
    }
}
