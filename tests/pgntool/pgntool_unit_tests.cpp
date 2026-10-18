#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "pgntool.hpp"

namespace {

std::filesystem::path fixture_path(std::string_view name) {
    return std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / name;
}

std::filesystem::path golden_path(std::string_view name) {
    return std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "pgntool" / "golden" / name;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct RunResult {
    int status;
    std::string out;
    std::string err;
};

RunResult invoke(const std::vector<std::string_view>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int status = chesswhy::pgntool::run(args, out, err);
    return {.status = status, .out = out.str(), .err = err.str()};
}

void require_error(const std::vector<std::string_view>& args, std::string_view named_token) {
    const auto result = invoke(args);
    CHECK(result.status == 2);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("chesswhy-pgn: error: "));
    CHECK(result.err.ends_with('\n'));
    CHECK(result.err.find('\n') == result.err.size() - 1);
    CHECK(result.err.find(named_token) != std::string::npos);
}

} // namespace

TEST_CASE("pgntool move TSV matches golden output", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto result = invoke({fixture});
    REQUIRE(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out == read_text(golden_path("moves.tsv")));
}

TEST_CASE("pgntool game TSV matches golden output regardless of flag order", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto file_first = invoke({"--games", fixture});
    const auto flag_last = invoke({fixture, "--games"});
    REQUIRE(file_first.status == 0);
    CHECK(file_first.err.empty());
    CHECK(file_first.out == read_text(golden_path("games.tsv")));
    CHECK(flag_last.status == 0);
    CHECK(flag_last.err.empty());
    CHECK(flag_last.out == file_first.out);
}

TEST_CASE("pgntool selects a single game regardless of flag order", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto file_first = invoke({fixture, "--game", "2"});
    const auto flag_first = invoke({"--game", "2", fixture});
    REQUIRE(file_first.status == 0);
    CHECK(file_first.err.empty());
    CHECK(file_first.out == flag_first.out);
    CHECK(flag_first.status == 0);
    CHECK(flag_first.err.empty());
    CHECK(file_first.out.starts_with(
        "game\tply\tmove_number\tside\tsan\tuci\tnags\tfen_before\tfen_after\n"));
    CHECK(file_first.out.find("\n2\t1\t1\tblack\tKf7\t") != std::string::npos);
    CHECK(file_first.out.find("\n1\t") == std::string::npos);
    CHECK(file_first.out.find("\n3\t") == std::string::npos);
}

TEST_CASE("pgntool reports argument and input errors without stdout", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    require_error({"tests/fixtures/no-such-pgn-file.pgn"}, "no-such-pgn-file.pgn");
    require_error({fixture, "--game", "0"}, "0");
    require_error({fixture, "--game", "6"}, "6");
    require_error({}, "FILE");
    require_error({fixture, fixture}, fixture);
    require_error({fixture, "--game"}, "--game");
    require_error({fixture, "--game", "not-a-number"}, "not-a-number");
    require_error({fixture, "--game", "2", "--game", "3"}, "--game");
    require_error({"--games", fixture, "--game", "2"}, "--games");
    require_error({fixture, "--game", "2", "--games"}, "--game");
    require_error({"--games", "--games", fixture}, "--games given twice");
    require_error({fixture, "--unknown"}, "--unknown");
}

TEST_CASE("pgntool help takes precedence anywhere in the arguments", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto result = invoke({fixture, "--help"});
    CHECK(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out.starts_with("usage: chesswhy-pgn [--game N] FILE\n"));
    const auto help_with_unknown_flag = invoke({"--unknown", "--help"});
    CHECK(help_with_unknown_flag.status == 0);
    CHECK(help_with_unknown_flag.err.empty());
    CHECK(help_with_unknown_flag.out == result.out);
}
