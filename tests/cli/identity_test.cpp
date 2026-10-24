#include "chesswhy/cli.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "progress.hpp"

#ifndef CHESSWHY_SOURCE_DIR
#error "CHESSWHY_SOURCE_DIR must be provided by chesswhy_paths"
#endif

namespace {

using Json = nlohmann::json;
constexpr std::array<std::string_view, 6> kFixtureIds{"F01", "F03", "F12", "F25", "F29", "F30"};
constexpr std::size_t kRerunCount = kFixtureIds.size() * 4U + 2U;

[[nodiscard]] Json fixture_file() {
    const auto path = std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures/positions.json";
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to read " + path.string());
    }
    return Json::parse(input);
}

[[nodiscard]] std::vector<std::string> fixture_args(const Json& json_args, bool json_output,
                                                    bool full_report = false) {
    std::vector<std::string> args;
    args.reserve(json_args.size() + (json_output ? 1U : 0U) + (full_report ? 1U : 0U));
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
    if (full_report) {
        args.emplace_back("--full-report");
    }
    return args;
}

[[nodiscard]] std::string run(const std::vector<std::string>& args) {
    std::vector<std::string_view> arg_views;
    arg_views.reserve(args.size());
    for (const auto& arg : args) {
        arg_views.emplace_back(arg);
    }
    const auto options = chesswhy::cli::parse_args(arg_views);
    std::ostringstream out;
    std::ostringstream err;
    const int status = chesswhy::cli::run(options, out, err);
    if (status != chesswhy::cli::kExitOk || !err.str().empty()) {
        throw std::runtime_error("CLI run failed with status " + std::to_string(status) + ": " +
                                 err.str());
    }
    return out.str();
}

void require_identical(std::string_view name, const std::string& first, const std::string& second) {
    if (first != second) {
        throw std::runtime_error(std::string(name) + " output differed across fresh runs");
    }
    std::cout << name << ": byte-identical\n";
}

} // namespace

int main() {
    try {
        const Json fixtures = fixture_file();
        chesswhy::test::Progress progress("cli_rerun_identity", kRerunCount);
        std::size_t rerun_index = 0;
        const auto rerun = [&](const std::vector<std::string>& args, std::string_view action,
                               std::string_view detail) {
            const std::size_t index = ++rerun_index;
            progress.step_started(index, action, detail);
            const std::string result = run(args);
            progress.step_finished(index, action, detail);
            return result;
        };
        for (const std::string_view id : kFixtureIds) {
            const auto& engine_fixtures = fixtures.at("engine_fixtures");
            const auto fixture = std::ranges::find_if(engine_fixtures, [id](const Json& value) {
                return value.at("id").get<std::string>() == id;
            });
            if (fixture == engine_fixtures.end()) {
                throw std::runtime_error("missing fixture " + std::string(id));
            }
            const auto full_args = fixture_args(fixture->at("args"), true, true);
            const std::string full_first = rerun(full_args, id, "full JSON first");
            const std::string full_second = rerun(full_args, id, "full JSON second");
            require_identical(std::string(id) + " full JSON", full_first, full_second);

            const auto moves_args = fixture_args(fixture->at("args"), true);
            const std::string moves_first = rerun(moves_args, id, "moves JSON first");
            const std::string moves_second = rerun(moves_args, id, "moves JSON second");
            require_identical(std::string(id) + " moves JSON", moves_first, moves_second);
        }
        constexpr std::string_view smoke_fen =
            "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3";
        const std::vector<std::string> smoke_args{"--fen", std::string(smoke_fen), "--moves",
                                                  "Nf6"};
        const std::string first = rerun(smoke_args, "smoke", "text first");
        const std::string second = rerun(smoke_args, "smoke", "text second");
        require_identical("smoke text", first, second);
    } catch (const std::exception& error) {
        std::cerr << "CLI rerun identity failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
