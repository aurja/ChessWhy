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

void require_error(const std::vector<std::string_view>& args) {
    const auto result = invoke(args);
    CHECK(result.status == 2);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("chesswhy-pgn: error: "));
    CHECK(result.err.ends_with('\n'));
    CHECK(result.err.find('\n') == result.err.size() - 1);
}

} // namespace

TEST_CASE("pgntool move TSV matches golden output", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto result = invoke({fixture});
    REQUIRE(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out == read_text(golden_path("moves.tsv")));
}

TEST_CASE("pgntool game TSV matches golden output", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto result = invoke({"--games", fixture});
    REQUIRE(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out == read_text(golden_path("games.tsv")));
}

TEST_CASE("pgntool selects a single game when requested", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    const auto result = invoke({fixture, "--game", "2"});
    REQUIRE(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out.starts_with(
        "game\tply\tmove_number\tside\tsan\tuci\tnags\tfen_before\tfen_after\n"));
    CHECK(result.out.find("\n2\t1\t1\tblack\tKf7\t") != std::string::npos);
    CHECK(result.out.find("\n1\t") == std::string::npos);
    CHECK(result.out.find("\n3\t") == std::string::npos);
}

TEST_CASE("pgntool reports argument and input errors without stdout", "[pgntool]") {
    const auto fixture = fixture_path("annotated.pgn").string();
    require_error({"tests/fixtures/no-such-pgn-file.pgn"});
    require_error({fixture, "--game", "0"});
    require_error({fixture, "--game", "6"});
    require_error({});
    require_error({fixture, "--game"});
}

TEST_CASE("pgntool help prints usage", "[pgntool]") {
    const auto result = invoke({"--help"});
    CHECK(result.status == 0);
    CHECK(result.err.empty());
    CHECK(result.out.starts_with("usage: chesswhy-pgn FILE [--game N]\n"));
}
