#include "chesswhy/games.hpp"
#include "chesswhy/openings.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace {

using chesswhy::games::BuildOptions;
using chesswhy::games::GamesError;
using chesswhy::games::Table;

struct BuiltTable {
    std::string contents;
    std::string summary;
};

BuiltTable build(std::string_view pgn, BuildOptions options) {
    std::istringstream input{std::string(pgn)};
    std::ostringstream output;
    std::ostringstream log;
    chesswhy::games::build_table(input, output, log, options);
    return {.contents = output.str(), .summary = log.str()};
}

std::string eligible_game(std::string_view result, std::string_view moves,
                          std::string_view extra_headers = {}) {
    std::string game = "[WhiteElo \"2300\"]\n[BlackElo \"2400\"]\n[Result \"";
    game += result;
    game += "\"]\n";
    game += extra_headers;
    game += "\n";
    game += moves;
    game += "\n";
    return game;
}

std::string
valid_table(std::string_view row = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t"
                                   "2\t2\t0\t0\td2d4:1,e2e4:1\n") {
    std::string table =
        "# source lichess_broadcast\n"
        "# platform over the board (may include online events relayed as broadcasts)\n"
        "# players both rated 2200 or more\n"
        "# months test\n"
        "# credit Lichess broadcast database, database.lichess.org, CC BY-SA 4.0\n"
        "# max_ply 1\n"
        "# min_games 2\n"
        "# games_read 2\n"
        "# excluded variant=0 setup=0 rating=0 result=0 illegal=0\n"
        "# games_kept 2\n"
        "# positions 1\n";
    table += row;
    return table;
}

class TemporaryTable {
public:
    explicit TemporaryTable(std::string_view contents) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("chesswhy-games-table-" + std::to_string(stamp) + ".tsv");
        std::ofstream output(path_, std::ios::binary);
        if (!output) {
            throw std::runtime_error("cannot create temporary games table");
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) {
            throw std::runtime_error("cannot write temporary games table");
        }
    }

    ~TemporaryTable() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    TemporaryTable(const TemporaryTable&) = delete;
    TemporaryTable& operator=(const TemporaryTable&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("games table counts first-failing exclusions in contract order", "[games][unit]") {
    std::string pgn;
    pgn += eligible_game("1-0", "1. e4 *", "[Variant \"Crazyhouse\"]\n");
    pgn += eligible_game(
        "1-0", "1. e4 *",
        "[SetUp \"1\"]\n[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n");
    pgn += "[WhiteElo \"2200.5\"]\n[BlackElo \"2300\"]\n[Result \"1-0\"]\n\n1. e4 *\n";
    pgn += eligible_game("*", "1. e4 *");
    pgn += eligible_game("1-0", "1. Ke3 *");
    pgn += eligible_game("1-0", "1. e4 e5 *");

    const auto result = build(pgn, {.max_ply = 2, .min_games = 1, .months = "test"});
    CHECK(result.contents.starts_with(
        "# source lichess_broadcast\n"
        "# platform over the board (may include online events relayed as broadcasts)\n"
        "# players both rated 2200 or more\n"
        "# months test\n"
        "# credit Lichess broadcast database, database.lichess.org, CC BY-SA 4.0\n"
        "# max_ply 2\n"
        "# min_games 1\n"));
    CHECK(result.contents.find("# months test\n") != std::string::npos);
    CHECK(result.contents.find("# games_read 6\n") != std::string::npos);
    CHECK(result.contents.find("# excluded variant=1 setup=1 rating=1 result=1 illegal=1\n") !=
          std::string::npos);
    CHECK(result.contents.find("# games_kept 1\n") != std::string::npos);
    CHECK(result.contents.find("# positions 2\n") != std::string::npos);
    CHECK(result.summary.find("games_read 6\n") != std::string::npos);
    CHECK(result.summary.find("excluded variant=1 setup=1 rating=1 result=1 illegal=1\n") !=
          std::string::npos);
}

TEST_CASE("games table respects ply limit and excludes N-minus-one positions", "[games][unit]") {
    std::string pgn;
    pgn += eligible_game("1-0", "1. e4 *");
    pgn += eligible_game("0-1", "1. e4 e5 2. Ke7 *");

    const auto result = build(pgn, {.max_ply = 2, .min_games = 2, .months = "test"});
    CHECK(result.contents.find("# games_read 2\n") != std::string::npos);
    CHECK(result.contents.find("# games_kept 2\n") != std::string::npos);
    CHECK(result.contents.find("# excluded variant=0 setup=0 rating=0 result=0 illegal=0\n") !=
          std::string::npos);
    CHECK(result.contents.find("# positions 1\n") != std::string::npos);
    chess::Board after_e4;
    after_e4.makeMove(chess::uci::uciToMove(after_e4, "e2e4"));
    const auto post_e4_row = chesswhy::openings::position_key(after_e4) + '\t';
    CHECK(result.contents.find(post_e4_row) == std::string::npos);
    CHECK(result.contents.find(
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t2\t1\t0\t1\te2e4:2\n") !=
          std::string::npos);
}

TEST_CASE("repeated positions contribute only their first occurrence per game", "[games][unit]") {
    const auto result = build(eligible_game("1-0", "1. Nf3 Nf6 2. Ng1 Ng8 3. e4 *"),
                              {.max_ply = 5, .min_games = 1, .months = "test"});
    const auto start =
        result.contents.find("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t");
    REQUIRE(start != std::string::npos);
    const auto row_end = result.contents.find('\n', start);
    const auto row = result.contents.substr(start, row_end - start);
    CHECK(row.find("\t1\t1\t0\t0\tg1f3:1") != std::string::npos);
    CHECK(row.find("e2e4") == std::string::npos);
}

TEST_CASE("minimum-count boundary, move tie ordering, and table lookup round trip",
          "[games][unit]") {
    std::string pgn;
    pgn += eligible_game("1-0", "1. e4 *");
    pgn += eligible_game("1/2-1/2", "1. d4 *");
    const auto result = build(pgn, {.max_ply = 1, .min_games = 2, .months = "2020-07"});
    const std::string expected_row =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t2\t1\t1\t0\td2d4:1,e2e4:1\n";
    CHECK(result.contents.ends_with(expected_row));

    TemporaryTable file(result.contents);
    const auto table = Table::load(file.path());
    const auto found = table.find(chess::Board{});
    CHECK(found.found);
    CHECK(found.id == "lichess_broadcast");
    CHECK(found.months == "2020-07");
    CHECK(found.max_ply == 1);
    CHECK(found.min_games == 2);
    REQUIRE(found.games == 2);
    CHECK(found.white == 1);
    CHECK(found.draws == 1);
    CHECK(found.black == 0);
    REQUIRE(found.moves.size() == 2);
    CHECK(found.moves[0].uci == "d2d4");
    CHECK(found.moves[0].san == "d4");
    CHECK(found.moves[0].games == 1);
    CHECK(found.moves[1].uci == "e2e4");
    CHECK(found.moves[1].san == "e4");

    chess::Board after_e4;
    after_e4.makeMove(chess::uci::uciToMove(after_e4, "e2e4"));
    const auto miss = table.find(after_e4);
    CHECK_FALSE(miss.found);
    CHECK_FALSE(miss.games.has_value());
    CHECK(miss.moves.empty());
}

TEST_CASE("games table loader rejects malformed headers, rows, and ordering", "[games][unit]") {
    const auto expect_bad = [](const std::string& contents) {
        TemporaryTable file(contents);
        CHECK_THROWS_AS(Table::load(file.path()), GamesError);
    };

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto missing = std::filesystem::temp_directory_path() /
                         ("chesswhy-missing-games-" + std::to_string(stamp) + ".tsv");
    CHECK_THROWS_AS(Table::load(missing), GamesError);

    expect_bad("not a games table\n");
    auto malformed_header = valid_table();
    malformed_header.replace(0, std::string("# source lichess_broadcast").size(),
                             "# source something_else");
    expect_bad(malformed_header);
    expect_bad(valid_table("malformed row\n"));
    expect_bad(
        valid_table("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t2\t2\t0\t0\ta1a8:2\n"));

    std::string duplicate_rows = valid_table();
    duplicate_rows.replace(duplicate_rows.find("# positions 1"),
                           std::string("# positions 1").size(), "# positions 2");
    duplicate_rows +=
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -\t2\t2\t0\t0\td2d4:1,e2e4:1\n";
    expect_bad(duplicate_rows);

    std::string unsorted_rows = valid_table();
    unsorted_rows.replace(unsorted_rows.find("# positions 1"), std::string("# positions 1").size(),
                          "# positions 2");
    unsorted_rows += "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq -\t2\t0\t1\t1\te7e5:2\n";
    expect_bad(unsorted_rows);
}
