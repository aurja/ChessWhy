#include "chesswhy/cli.hpp"
#include "chesswhy/input.hpp"
#include "chesswhy/openings.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#ifndef CHESSWHY_SOURCE_DIR
#error "CHESSWHY_SOURCE_DIR must be provided by chesswhy_paths"
#endif
#ifndef CHESSWHY_ASSETS_DIR
#error "CHESSWHY_ASSETS_DIR must be provided by chesswhy_paths"
#endif

namespace {

using Json = nlohmann::json;

[[nodiscard]] std::filesystem::path fixture_path(std::string_view filename) {
    return std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests/fixtures" / filename;
}

[[nodiscard]] Json positions() {
    std::ifstream input(std::filesystem::path(CHESSWHY_SOURCE_DIR) /
                        "tests/fixtures/positions.json");
    if (!input) {
        throw std::runtime_error("unable to read tests/fixtures/positions.json");
    }
    return Json::parse(input);
}

[[nodiscard]] std::vector<std::string> fixture_args(const Json& args) {
    std::vector<std::string> result;
    result.reserve(args.size());
    for (const auto& arg : args) {
        result.push_back(arg.get<std::string>());
    }
    for (std::size_t index = 0; index + 1 < result.size(); ++index) {
        if (result[index] == "--pgn") {
            result[index + 1] = fixture_path(result[index + 1]).string();
        }
    }
    return result;
}

[[nodiscard]] std::vector<std::string_view> views(const std::vector<std::string>& args) {
    std::vector<std::string_view> result;
    result.reserve(args.size());
    for (const auto& arg : args) {
        result.emplace_back(arg);
    }
    return result;
}

[[nodiscard]] Json opening_name_json(const std::optional<chesswhy::OpeningName>& name) {
    if (!name) {
        return nullptr;
    }
    return Json{{"eco", name->eco}, {"name", name->name}};
}

[[nodiscard]] Json last_named_json(const std::optional<chesswhy::NamedPly>& named) {
    if (!named) {
        return nullptr;
    }
    return Json{{"eco", named->opening.eco}, {"name", named->opening.name}, {"ply", named->ply}};
}

} // namespace

TEST_CASE("CLI error fixtures return the required status and one diagnostic", "[cli][unit]") {
    const Json fixture_file = positions();
    for (const auto& fixture : fixture_file.at("error_fixtures")) {
        INFO("fixture " << fixture.at("id").get<std::string>());
        const auto args = fixture_args(fixture.at("args"));
        const auto arg_views = views(args);
        std::ostringstream out;
        std::ostringstream err;
        int exit_code = -1;
        try {
            const auto options = chesswhy::cli::parse_args(arg_views);
            exit_code = chesswhy::cli::run(options, out, err);
        } catch (const chesswhy::input::InputError& error) {
            exit_code = chesswhy::cli::kExitInput;
            err << "chesswhy: error: " << error.what() << '\n';
        }

        CHECK(exit_code == fixture.at("exit").get<int>());
        CHECK(out.str().empty());
        const std::string diagnostic = err.str();
        REQUIRE(diagnostic.starts_with("chesswhy: error: "));
        REQUIRE_FALSE(diagnostic.empty());
        CHECK(diagnostic.back() == '\n');
        CHECK(diagnostic.find('\n') == diagnostic.size() - 1);
        for (const auto& fragment : fixture.at("contains")) {
            CHECK(diagnostic.find(fragment.get<std::string>()) != std::string::npos);
        }
    }
}

TEST_CASE("PGN opening fixtures describe current and last named positions", "[cli][unit]") {
    const Json fixture_file = positions();
    const auto table = chesswhy::openings::Table::load(std::filesystem::path(CHESSWHY_ASSETS_DIR) /
                                                       "data/openings");
    for (const auto& fixture : fixture_file.at("unit_fixtures")) {
        if (!fixture.contains("pgn")) {
            continue;
        }
        INFO("fixture " << fixture.at("id").get<std::string>());
        const int game = fixture.value("game", 1);
        std::optional<int> requested_ply;
        if (fixture.at("id") != "U03") {
            requested_ply = fixture.at("ply").get<int>();
        }
        const auto position = chesswhy::input::load_pgn(
            fixture_path(fixture.at("pgn").get<std::string>()), game, requested_ply);
        const auto opening = chesswhy::openings::describe(table, position.board, position.mainline);
        CHECK(position.ply == fixture.at("ply").get<int>());
        CHECK(position.board.getFen() == fixture.at("fen").get<std::string>());
        CHECK(opening_name_json(opening.current) == fixture.at("current"));
        CHECK(last_named_json(opening.last_named) == fixture.at("last_named"));
    }
}

TEST_CASE("FEN and SAN/UCI parse fixtures identify the same moves", "[cli][unit]") {
    const Json fixture_file = positions();
    for (const auto& fixture : fixture_file.at("parse_fixtures")) {
        INFO("move " << fixture.at("move").get<std::string>());
        const auto board = chesswhy::input::parse_fen(fixture.at("fen").get<std::string>());
        const auto san_move =
            chesswhy::input::parse_move(board, fixture.at("move").get<std::string>());
        const auto uci = fixture.at("uci").get<std::string>();
        CHECK(chess::uci::moveToUci(san_move) == uci);
        CHECK(chesswhy::input::parse_move(board, uci) == san_move);
    }
}

TEST_CASE("CLI argument parsing applies defaults and validates usage", "[cli][unit]") {
    constexpr std::string_view fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::vector<std::string_view> defaults_args{"--fen", fen};
    const auto defaults = chesswhy::cli::parse_args(defaults_args);
    CHECK(defaults.fen == fen);
    CHECK_FALSE(defaults.pgn.has_value());
    CHECK(defaults.game == 1);
    CHECK_FALSE(defaults.ply.has_value());
    CHECK_FALSE(defaults.move.has_value());
    CHECK(defaults.level == chesswhy::Level::Club);
    CHECK(defaults.budget == chesswhy::Budget::Standard);
    CHECK_FALSE(defaults.json);
    CHECK_FALSE(defaults.no_llm);
    CHECK(defaults.threads == 1);
    CHECK(defaults.engine == std::filesystem::path(CHESSWHY_ASSETS_DIR) / "engines/stockfish");
    CHECK(defaults.openings == std::filesystem::path(CHESSWHY_ASSETS_DIR) / "data/openings");

    const std::vector<std::string_view> help_args{"--help"};
    const auto help = chesswhy::cli::parse_args(help_args);
    CHECK(help.help);
    std::ostringstream help_out;
    std::ostringstream help_err;
    CHECK(chesswhy::cli::run(help, help_out, help_err) == chesswhy::cli::kExitOk);
    CHECK(help_err.str().empty());
    CHECK(help_out.str().find("Usage: chesswhy") != std::string::npos);

    const auto expect_error = [](const std::vector<std::string_view>& args,
                                 std::string_view fragment) {
        try {
            (void)chesswhy::cli::parse_args(args);
            FAIL("parse_args accepted invalid arguments");
        } catch (const chesswhy::input::InputError& error) {
            CHECK(std::string_view(error.what()).find(fragment) != std::string_view::npos);
        }
    };
    expect_error({"--fen", fen, "--budget", "insane"}, "--budget");
    expect_error({"--fen", fen, "--game", "2"}, "--game");
    expect_error({"--fen", fen, "--game", "1"}, "--game");
    expect_error({"--fen", fen, "--threads", "0"}, "--threads");
    expect_error({"--fen"}, "--fen");
    expect_error({"--fen", fen, "--mystery"}, "--mystery");
    expect_error({}, "exactly one of --fen or --pgn");
}
