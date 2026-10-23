#include "chesswhy/cli.hpp"
#include "chesswhy/input.hpp"
#include "chesswhy/move_report.hpp"
#include "chesswhy/openings.hpp"
#include "chesswhy/render.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "output.hpp"
#include "selection.hpp"

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
        if (fixture.contains("message")) {
            CHECK(diagnostic == fixture.at("message").get<std::string>() + '\n');
        } else {
            for (const auto& fragment : fixture.at("contains")) {
                CHECK(diagnostic.find(fragment.get<std::string>()) != std::string::npos);
            }
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
    CHECK_FALSE(defaults.moves.has_value());
    CHECK(defaults.level == chesswhy::Level::Club);
    CHECK(defaults.budget == chesswhy::Budget::Standard);
    CHECK_FALSE(defaults.json);
    CHECK_FALSE(defaults.full_report);
    CHECK_FALSE(defaults.no_llm);
    CHECK(defaults.threads == 1);
    CHECK(defaults.engine == std::filesystem::path(CHESSWHY_ASSETS_DIR) / "engines/stockfish");
    CHECK(defaults.openings == std::filesystem::path(CHESSWHY_ASSETS_DIR) / "data/openings");
    CHECK(defaults.games ==
          std::filesystem::path(CHESSWHY_ASSETS_DIR) / "data/games/lichess_broadcast_2200.tsv");

    const std::vector<std::string_view> games_args{"--fen", fen, "--games", "/tmp/games.tsv"};
    const auto custom_games = chesswhy::cli::parse_args(games_args);
    CHECK(custom_games.games == std::filesystem::path("/tmp/games.tsv"));
    constexpr std::string_view moves = "Nf3,best,top3,e4";
    const std::vector<std::string_view> moves_args{"--fen", fen,      "--moves",
                                                   moves,   "--json", "--full-report"};
    const auto selected_moves = chesswhy::cli::parse_args(moves_args);
    CHECK(selected_moves.moves == moves);
    CHECK(selected_moves.json);
    CHECK(selected_moves.full_report);

    const std::vector<std::string_view> help_args{"--help"};
    const auto help = chesswhy::cli::parse_args(help_args);
    CHECK(help.help);
    std::ostringstream help_out;
    std::ostringstream help_err;
    CHECK(chesswhy::cli::run(help, help_out, help_err) == chesswhy::cli::kExitOk);
    CHECK(help_err.str().empty());
    CHECK(help_out.str().find("Usage: chesswhy") != std::string::npos);
    CHECK(help_out.str().find("--moves LIST") != std::string::npos);
    CHECK(help_out.str().find("--full-report") != std::string::npos);
    CHECK(help_out.str().find("comma-separated, no spaces") != std::string::npos);
    CHECK(help_out.str().find("topN") != std::string::npos);
    CHECK(help_out.str().find("every legal move") != std::string::npos);
    CHECK(help_out.str().find("several times the default time") != std::string::npos);

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
    expect_error({"--fen", fen, "--move", "Nf6"}, "unknown flag --move");
    expect_error({"--fen", fen, "--moves", "Nf3", "--moves", "e4"}, "duplicate flag --moves");
    const std::vector<std::string_view> missing_games_args{"--fen", fen, "--games",
                                                           "/nonexistent/games.tsv"};
    const auto missing_games = chesswhy::cli::parse_args(missing_games_args);
    std::ostringstream missing_games_out;
    std::ostringstream missing_games_err;
    CHECK(chesswhy::cli::run(missing_games, missing_games_out, missing_games_err) ==
          chesswhy::cli::kExitEngine);
    CHECK(missing_games_out.str().empty());
    CHECK(missing_games_err.str().find("/nonexistent/games.tsv") != std::string::npos);
    expect_error({"--fen", fen, "--games", "/first.tsv", "--games", "/second.tsv"},
                 "duplicate flag --games");
}

TEST_CASE("CLI move selection parses keywords, topN and moves", "[cli][unit]") {
    using Keyword = chesswhy::Selection::Keyword;
    const auto board =
        chesswhy::input::parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    const auto defaults = chesswhy::cli::detail::resolve_moves(board, std::nullopt);
    CHECK(defaults.selection.keywords == std::vector<Keyword>{Keyword::Best});
    CHECK_FALSE(defaults.selection.top.has_value());
    CHECK(defaults.selection.moves.empty());
    CHECK(defaults.asked.empty());

    const std::vector<std::pair<std::string_view, Keyword>> keyword_cases{
        {"best", Keyword::Best},
        {"playable", Keyword::Playable},
        {"inaccuracies", Keyword::Inaccuracies},
        {"mistakes", Keyword::Mistakes},
        {"blunders", Keyword::Blunders},
        {"unplayable", Keyword::Unplayable},
        {"all", Keyword::All},
    };
    for (const auto& [item, keyword] : keyword_cases) {
        INFO("selection item " << item);
        const auto parsed = chesswhy::cli::detail::resolve_moves(board, item);
        CHECK(parsed.selection.keywords == std::vector<Keyword>{keyword});
        CHECK_FALSE(parsed.selection.top.has_value());
        CHECK(parsed.selection.moves.empty());
        CHECK(parsed.asked.empty());
    }

    const auto top = chesswhy::cli::detail::resolve_moves(board, "top3");
    CHECK(top.selection.keywords.empty());
    CHECK(top.selection.top == 3);
    CHECK(top.selection.moves.empty());
    CHECK(top.asked.empty());

    for (const auto& [san, uci] : std::vector<std::pair<std::string_view, std::string_view>>{
             {"Nf3", "g1f3"}, {"g1f3", "g1f3"}, {"e4", "e2e4"}, {"e2e4", "e2e4"}}) {
        INFO("asked move " << san);
        const auto parsed = chesswhy::cli::detail::resolve_moves(board, san);
        CHECK(parsed.selection.keywords.empty());
        CHECK_FALSE(parsed.selection.top.has_value());
        CHECK(parsed.selection.moves == std::vector<std::string>{std::string(uci)});
        CHECK(parsed.asked == std::vector<chess::Move>{chesswhy::input::parse_move(board, san)});
    }

    const auto combined = chesswhy::cli::detail::resolve_moves(
        board, "all,blunders,best,mistakes,unplayable,playable,inaccuracies,"
               "top2,Nf3,g1f3,e4,e2e4,top5");
    CHECK(combined.selection.keywords == std::vector<Keyword>{Keyword::Best, Keyword::Playable,
                                                              Keyword::Inaccuracies,
                                                              Keyword::Mistakes, Keyword::Blunders,
                                                              Keyword::Unplayable, Keyword::All});
    CHECK(combined.selection.top == 5);
    CHECK(combined.selection.moves == std::vector<std::string>{"g1f3", "e2e4"});
    CHECK(combined.asked == std::vector<chess::Move>{chesswhy::input::parse_move(board, "Nf3"),
                                                     chesswhy::input::parse_move(board, "e4")});

    const auto expect_selection_error = [board](std::string_view value,
                                                std::string_view diagnostic) {
        try {
            (void)chesswhy::cli::detail::resolve_moves(board, value);
            FAIL("move selection accepted invalid input");
        } catch (const chesswhy::input::InputError& error) {
            CHECK(std::string_view(error.what()).find(diagnostic) != std::string_view::npos);
        }
    };
    expect_selection_error("Nf3,,e4", "empty item in --moves");
    expect_selection_error("top0", "invalid value for --moves: top0");
    expect_selection_error("top", "invalid value for --moves: top");
    expect_selection_error("foo", "cannot parse move foo");
    constexpr std::string_view fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::vector<std::pair<std::string_view, std::string_view>> invalid_selections{
        {"Nf3,,e4", "empty item in --moves"},
        {"top0", "invalid value for --moves: top0"},
        {"top", "invalid value for --moves: top"},
        {"foo", "cannot parse move foo"},
    };
    for (const auto& [value, diagnostic] : invalid_selections) {
        INFO("invalid selection " << value);
        const std::vector<std::string_view> args{"--fen", fen, "--moves", value};
        const auto options = chesswhy::cli::parse_args(args);
        std::ostringstream output;
        std::ostringstream error;
        CHECK(chesswhy::cli::run(options, output, error) == chesswhy::cli::kExitInput);
        CHECK(output.str().empty());
        CHECK(error.str().starts_with("chesswhy: error: "));
        CHECK(error.str().find(diagnostic) != std::string::npos);
    }
}

TEST_CASE("CLI output flags select move/full content and text/JSON format", "[cli][unit]") {
    chesswhy::Report fake_report;
    fake_report.input.moves = "Nf3,best,top2";
    const auto move_document = chesswhy::move_reports(fake_report);

    const auto render_output = [&fake_report](bool json, bool full_report) {
        chesswhy::cli::Options options;
        options.json = json;
        options.full_report = full_report;
        std::ostringstream output;
        chesswhy::cli::detail::write_output(options, fake_report, output);
        return output.str();
    };

    const std::string move_text = render_output(false, false);
    const std::string move_json = render_output(true, false);
    const std::string full_text = render_output(false, true);
    const std::string full_json = render_output(true, true);
    CHECK(move_text == chesswhy::render::text(move_document));
    CHECK(move_json == chesswhy::to_json(move_document).dump(2) + "\n");
    CHECK(full_text == chesswhy::render::text(fake_report));
    CHECK(full_json == chesswhy::to_json(fake_report).dump(2) + "\n");
    CHECK(Json::parse(move_json).at("document") == "moves");
    CHECK(Json::parse(full_json).at("document") == "full");
    CHECK(Json::parse(full_json).at("input").at("moves") == "Nf3,best,top2");
}
