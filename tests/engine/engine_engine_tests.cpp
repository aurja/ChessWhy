#include "chesswhy/engine.hpp"

#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

using chesswhy::engine::Engine;
using chesswhy::engine::EngineError;
using chesswhy::engine::Line;
using chesswhy::engine::Options;
using chesswhy::engine::SearchRequest;
using chesswhy::engine::SearchResult;

bool same_line(const Line& left, const Line& right) {
    return left.multipv == right.multipv && left.depth == right.depth && left.cp == right.cp &&
           left.mate == right.mate && left.win == right.win && left.draw == right.draw &&
           left.loss == right.loss && left.pv_uci == right.pv_uci;
}

bool same_lines(const SearchResult& left, const SearchResult& right) {
    if (left.lines.size() != right.lines.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.lines.size(); ++index) {
        if (!same_line(left.lines[index], right.lines[index])) {
            return false;
        }
    }
    return true;
}

void check_wdl(const SearchResult& result) {
    for (const auto& line : result.lines) {
        CHECK(line.win + line.draw + line.loss == 1000);
    }
}

} // namespace

TEST_CASE("Stockfish fixed-depth searches are reproducible", "[engine]") {
    const std::filesystem::path binary =
        std::filesystem::path(CHESSWHY_ASSETS_DIR) / "engines" / "stockfish";
    REQUIRE(std::filesystem::exists(binary));
    REQUIRE(std::filesystem::is_regular_file(binary));
    REQUIRE(::access(binary.c_str(), X_OK) == 0);

    Engine engine(Options{.binary = binary, .threads = 1, .hash_mb = 64});
    CHECK(engine.name().starts_with("Stockfish"));

    constexpr auto start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    CHECK_THROWS_AS(engine.search(SearchRequest{.fen = start_fen,
                                                .depth = 0,
                                                .multipv = 1,
                                                .searchmoves_uci = {}}),
                    EngineError);
    CHECK_THROWS_AS(engine.search(SearchRequest{.fen = start_fen,
                                                .depth = 1,
                                                .multipv = 0,
                                                .searchmoves_uci = {}}),
                    EngineError);
    const SearchRequest repeat_request{
        .fen = start_fen, .depth = 5, .multipv = 1, .searchmoves_uci = {}};
    const auto first = engine.search(repeat_request);
    REQUIRE_FALSE(first.lines.empty());
    CHECK(first.lines.front().depth >= repeat_request.depth);
    check_wdl(first);

    const SearchRequest unrelated_request{
        .fen = "rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
        .depth = 4,
        .multipv = 1,
        .searchmoves_uci = {}};
    const auto unrelated = engine.search(unrelated_request);
    REQUIRE_FALSE(unrelated.lines.empty());
    CHECK(unrelated.lines.front().depth >= unrelated_request.depth);

    const auto repeated = engine.search(repeat_request);
    CHECK(same_lines(first, repeated));
    check_wdl(repeated);

    const SearchRequest restricted_request{
        .fen = start_fen, .depth = 4, .multipv = 1, .searchmoves_uci = {"e2e4"}};
    const auto restricted = engine.search(restricted_request);
    REQUIRE_FALSE(restricted.lines.empty());
    REQUIRE_FALSE(restricted.lines.front().pv_uci.empty());
    CHECK(restricted.lines.front().pv_uci.front() == "e2e4");
    check_wdl(restricted);

    const SearchRequest one_legal_move{
        .fen = "rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
        .depth = 4,
        .multipv = 5,
        .searchmoves_uci = {}};
    const auto single_line = engine.search(one_legal_move);
    REQUIRE(single_line.lines.size() == 1);
    CHECK(single_line.lines.front().depth >= one_legal_move.depth);
    CHECK(single_line.lines.front().multipv == 1);
    check_wdl(single_line);
}

TEST_CASE("missing engine binary reports an EngineError with its path", "[engine]") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("chesswhy-missing-engine-" + std::to_string(::getpid()));
    REQUIRE_FALSE(std::filesystem::exists(path));

    bool threw = false;
    try {
        const Engine missing(Options{.binary = path, .threads = 1, .hash_mb = 64});
    } catch (const EngineError& error) {
        threw = true;
        const std::string message = error.what();
        CHECK(message.find("engine") != std::string::npos);
        CHECK(message.find(path.string()) != std::string::npos);
    }
    CHECK(threw);
}
