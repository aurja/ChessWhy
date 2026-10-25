#include "chesswhy/cli.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chess.hpp>
#include <nlohmann/json.hpp>

#include "progress.hpp"

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

[[nodiscard]] std::vector<std::string> fixture_args(const Json& json_args) {
    std::vector<std::string> args;
    args.reserve(json_args.size());
    for (const auto& value : json_args) {
        args.push_back(value.get<std::string>());
    }
    for (std::size_t index = 1; index < args.size(); ++index) {
        if (args[index - 1] == "--pgn") {
            args[index] =
                (std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures" / args[index])
                    .string();
        }
    }
    return args;
}

[[nodiscard]] chesswhy::cli::Options parse_options(const Json& json_args) {
    const auto args = fixture_args(json_args);
    std::vector<std::string_view> arg_views;
    arg_views.reserve(args.size());
    for (const auto& arg : args) {
        arg_views.emplace_back(arg);
    }
    return chesswhy::cli::parse_args(arg_views);
}

[[nodiscard]] std::string render_output(const chesswhy::cli::Options& options,
                                        const chesswhy::Report& report, bool json,
                                        bool full_report) {
    auto output_options = options;
    output_options.json = json;
    output_options.full_report = full_report;
    std::ostringstream output;
    chesswhy::cli::write_output(output_options, report, output);
    return output.str();
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

void check_motifs_include(const Json& expectations, const Json& report, std::string_view id,
                          std::vector<std::string>& failures) {
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
                        (motif.is_object() &&
                         motif.value("contribution", Json(nullptr)) == instance.at("contribution"));
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

void check_expectations(const Json& fixture, std::string_view id, const Json& report,
                        std::vector<std::string>& failures) {
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

    if (fixture.contains("playable_includes")) {
        const Json* candidates = find_pointer(report, "/candidates");
        for (const auto& san : fixture.at("playable_includes")) {
            bool found = false;
            if (candidates != nullptr && candidates->is_array()) {
                found = std::ranges::any_of(*candidates, [&](const Json& candidate) {
                    return candidate.value("san", std::string{}) == san.get<std::string>() &&
                           (candidate.value("verdict", std::string{}) == "best" ||
                            candidate.value("verdict", std::string{}) == "okay");
                });
            }
            const Json expected = {{"san", san}, {"verdict", "best or okay"}};
            check_value(found, id, "playable_includes:" + san.get<std::string>(), expected,
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
    if (fixture.contains("candidates_count") || fixture.contains("candidates_detailed")) {
        const Json* candidates = find_pointer(report, "/candidates");
        const std::size_t count =
            candidates != nullptr && candidates->is_array() ? candidates->size() : 0U;
        if (fixture.contains("candidates_count")) {
            check_value(candidates != nullptr && candidates->is_array() &&
                            count == fixture.at("candidates_count").get<std::size_t>(),
                        id, "candidates_count", fixture.at("candidates_count"), candidates,
                        failures);
        }
        if (fixture.contains("candidates_detailed")) {
            const bool all_detailed = candidates != nullptr && candidates->is_array() &&
                                      std::ranges::all_of(*candidates, [](const Json& candidate) {
                                          return candidate.value("detailed", false);
                                      });
            check_value(all_detailed == fixture.at("candidates_detailed").get<bool>(), id,
                        "candidates_detailed", fixture.at("candidates_detailed"), candidates,
                        failures);
        }
    }
    if (fixture.contains("motifs_include")) {
        check_motifs_include(fixture.at("motifs_include"), report, id, failures);
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
                        return std::ranges::all_of(instance.items(), [&](const auto& field) {
                            return feature.contains(field.key()) &&
                                   feature.at(field.key()) == field.value();
                        });
                    });
                std::string expectation = "positional_include:";
                expectation += pointer;
                check_value(found, id, expectation, instance, actual, failures);
            }
        }
    }
}

void check_move_expectations(const Json& fixture, const Json& document, std::string_view id,
                             std::vector<std::string>& failures) {
    if (fixture.contains("moves_equals")) {
        for (auto expected = fixture.at("moves_equals").begin();
             expected != fixture.at("moves_equals").end(); ++expected) {
            const std::string& pointer = expected.key();
            const Json* actual = find_pointer(document, pointer);
            check_value(actual != nullptr && *actual == expected.value(), id, pointer,
                        expected.value(), actual, failures);
        }
    }
    if (fixture.contains("moves_count")) {
        const Json* moves = find_pointer(document, "/moves");
        check_value(moves != nullptr && moves->is_array() &&
                        moves->size() == fixture.at("moves_count").get<std::size_t>(),
                    id, "moves_count", fixture.at("moves_count"), moves, failures);
    }
    if (fixture.contains("moves_san")) {
        Json actual = Json::array();
        const Json* moves = find_pointer(document, "/moves");
        if (moves != nullptr && moves->is_array()) {
            for (const auto& move : *moves) {
                actual.push_back(move.value("san", std::string{}));
            }
        }
        check_value(moves != nullptr && actual == fixture.at("moves_san"), id, "moves_san",
                    fixture.at("moves_san"), moves == nullptr ? nullptr : &actual, failures);
    }
    if (fixture.contains("moves_motifs_include")) {
        check_motifs_include(fixture.at("moves_motifs_include"), document, id, failures);
    }
}

int run_timing() {
    try {
        const Json fixture_file = read_fixtures();
        const auto& fixtures = fixture_file.at("engine_fixtures");
        std::vector<std::string> failures;
        chesswhy::test::Progress progress("cli_timing", fixtures.size());
        std::size_t index = 0;
        for (const Json& fixture : fixtures) {
            ++index;
            const std::string fixture_id = fixture.at("id").get<std::string>();
            progress.step_started(index, "run", fixture_id);
            const auto options = parse_options(fixture.at("args"));
            const auto start = Clock::now();
            try {
                const auto report = chesswhy::cli::build_report(options);
                const std::string full_output = render_output(options, report, true, true);
                static_cast<void>(full_output);
            } catch (const std::exception& error) {
                failures.push_back("fixture " + fixture_id +
                                   " full-JSON invocation failed: " + error.what());
            }
            const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            std::cout << "fixture " << fixture_id << " wall time: " << elapsed << " s\n";
            if (elapsed > 25.0) {
                failures.push_back("fixture " + fixture_id +
                                   " wall time: expected <= 25 s, actual " +
                                   std::to_string(elapsed) + " s");
            }
            progress.step_finished(index, "run", fixture_id);
        }
        for (const auto& failure : failures) {
            std::cerr << failure << '\n';
        }
        return failures.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "cli_timing failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--timing") {
        return run_timing();
    }
    if (argc != 2) {
        std::cerr << "usage: cli_fixture_test FIXTURE_ID | --timing\n";
        return 2;
    }
    const std::string fixture_id = argv[1];
    try {
        const Json fixture_file = read_fixtures();
        const auto& fixtures = fixture_file.at("engine_fixtures");
        const auto found = std::ranges::find_if(fixtures, [&](const Json& fixture) {
            return fixture.at("id").get<std::string>() == fixture_id;
        });
        if (found == fixtures.end()) {
            throw std::runtime_error("unknown engine fixture " + fixture_id);
        }

        const std::filesystem::path report_directory(CHESSWHY_CLI_REPORT_DIR);
        std::filesystem::create_directories(report_directory);
        std::vector<std::string> failures;
        const std::size_t run_count =
            1U + (found->contains("runs") ? found->at("runs").size() : 0U);
        chesswhy::test::Progress progress(fixture_id, run_count);
        const auto run_and_check = [&](const Json& run, std::string_view run_name,
                                       bool default_run) {
            const std::string label =
                default_run ? fixture_id : fixture_id + ":" + std::string(run_name);
            const std::string filename =
                default_run ? fixture_id : fixture_id + "-" + std::string(run_name);
            const auto options = parse_options(run.at("args"));
            const auto start = Clock::now();
            const auto report = chesswhy::cli::build_report(options);
            const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            const std::string full_output = render_output(options, report, true, true);
            const Json full_report = Json::parse(full_output);
            const std::string moves_output = render_output(options, report, true, false);
            const Json moves_document = Json::parse(moves_output);
            const std::string text_output = render_output(options, report, false, false);
            const std::string full_text_output = render_output(options, report, false, true);

            const auto save_report = [&](std::string_view suffix, const std::string& output) {
                const auto path = report_directory / (filename + std::string(suffix));
                std::ofstream file(path, std::ios::binary);
                if (!file) {
                    throw std::runtime_error("unable to write " + path.string());
                }
                file << output;
                file.close();
                if (!file) {
                    throw std::runtime_error("unable to finish writing " + path.string());
                }
            };
            save_report(".full.json", full_output);
            save_report(".moves.json", moves_output);

            check_expectations(run, label, full_report, failures);
            check_move_expectations(run, moves_document, label, failures);
            if (run.contains("text_contains")) {
                const std::string expected = run.at("text_contains").get<std::string>();
                const Json actual = text_output;
                check_value(text_output.find(expected) != std::string::npos, label, "text_contains",
                            Json(expected), &actual, failures);
            }
            if (run.contains("full_text_contains")) {
                const std::string expected = run.at("full_text_contains").get<std::string>();
                const Json actual = full_text_output;
                check_value(full_text_output.find(expected) != std::string::npos, label,
                            "full_text_contains", Json(expected), &actual, failures);
            }
            if (default_run) {
                std::cout << "fixture " << fixture_id << " wall time: " << elapsed << " s\n";
            }
        };

        std::size_t run_index = 1;
        progress.step_started(run_index, "run", "default");
        run_and_check(*found, "default", true);
        progress.step_finished(run_index, "run", "default");
        if (found->contains("runs")) {
            for (const auto& run : found->at("runs")) {
                ++run_index;
                const std::string run_name = run.at("name").get<std::string>();
                progress.step_started(run_index, "run", run_name);
                run_and_check(run, run_name, false);
                progress.step_finished(run_index, "run", run_name);
            }
        }
        for (const auto& failure : failures) {
            std::cerr << failure << '\n';
        }
        return failures.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "fixture " << fixture_id << " failed: " << error.what() << '\n';
        return 1;
    }
}
