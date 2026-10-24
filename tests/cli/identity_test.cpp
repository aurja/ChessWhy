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
constexpr std::size_t kRerunCount = (kFixtureIds.size() * 2U) + 2U;

[[nodiscard]] Json fixture_file() {
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
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == "--pgn") {
            args[index + 1] =
                (std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures" / args[index + 1])
                    .string();
        }
    }
    return args;
}

[[nodiscard]] chesswhy::cli::Options parse_options(const std::vector<std::string>& args) {
    std::vector<std::string_view> arg_views;
    arg_views.reserve(args.size());
    for (const auto& arg : args) {
        arg_views.emplace_back(arg);
    }
    return chesswhy::cli::parse_args(arg_views);
}

[[nodiscard]] chesswhy::cli::Options fixture_options(const Json& json_args) {
    return parse_options(fixture_args(json_args));
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

[[nodiscard]] std::array<std::string, 4> render_outputs(const chesswhy::cli::Options& options,
                                                        const chesswhy::Report& report) {
    return {render_output(options, report, true, true), render_output(options, report, true, false),
            render_output(options, report, false, false),
            render_output(options, report, false, true)};
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
        const auto rerun = [&](const chesswhy::cli::Options& options, std::string_view action,
                               std::string_view detail) {
            const std::size_t index = ++rerun_index;
            progress.step_started(index, action, detail);
            auto report = chesswhy::cli::build_report(options);
            progress.step_finished(index, action, detail);
            return report;
        };
        constexpr std::array<std::string_view, 4> output_names{"full JSON", "moves JSON", "text",
                                                               "full text"};
        for (const std::string_view id : kFixtureIds) {
            const auto& engine_fixtures = fixtures.at("engine_fixtures");
            const auto fixture = std::ranges::find_if(engine_fixtures, [id](const Json& value) {
                return value.at("id").get<std::string>() == id;
            });
            if (fixture == engine_fixtures.end()) {
                throw std::runtime_error("missing fixture " + std::string(id));
            }
            const auto options = fixture_options(fixture->at("args"));
            const auto first_report = rerun(options, id, "first report");
            const auto second_report = rerun(options, id, "second report");
            const auto first_outputs = render_outputs(options, first_report);
            const auto second_outputs = render_outputs(options, second_report);
            for (std::size_t index = 0; index < output_names.size(); ++index) {
                require_identical(std::string(id) + " " + std::string(output_names[index]),
                                  first_outputs[index], second_outputs[index]);
            }
        }
        constexpr std::string_view smoke_fen =
            "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3";
        const std::vector<std::string> smoke_args{"--fen", std::string(smoke_fen), "--moves",
                                                  "Nf6"};
        const auto smoke_options = parse_options(smoke_args);
        const auto first_report = rerun(smoke_options, "smoke", "text first");
        const auto second_report = rerun(smoke_options, "smoke", "text second");
        const std::string first = render_output(smoke_options, first_report, false, false);
        const std::string second = render_output(smoke_options, second_report, false, false);
        require_identical("smoke text", first, second);
    } catch (const std::exception& error) {
        std::cerr << "CLI rerun identity failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
