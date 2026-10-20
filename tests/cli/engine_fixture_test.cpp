#include "chesswhy/cli.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] std::vector<std::string> fixture_args(const Json& json_args) {
    std::vector<std::string> args;
    args.reserve(json_args.size() + 1);
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
    args.emplace_back("--json");
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
                        return motif.is_object() && actor_square != nullptr &&
                               *actor_square == actor && motif.value("tag", std::string{}) == tag &&
                               motif.value("side", std::string{}) == side;
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
