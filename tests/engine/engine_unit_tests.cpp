#include "chesswhy/engine.hpp"

#include <catch2/catch_test_macros.hpp>

using chesswhy::engine::Line;
using chesswhy::engine::parse_info_line;

TEST_CASE("parse Stockfish info lines with centipawn scores", "[unit]") {
    const auto line = parse_info_line(
        "info depth 14 seldepth 20 multipv 2 score cp -31 nodes 1000 wdl 250 500 250 "
        "pv e2e4 e7e5 g1f3");

    REQUIRE(line.has_value());
    const Line parsed = line.value_or(Line{});
    CHECK(parsed.depth == 14);
    CHECK(parsed.multipv == 2);
    CHECK(parsed.cp == -31);
    CHECK_FALSE(parsed.mate.has_value());
    CHECK(parsed.win == 250);
    CHECK(parsed.draw == 500);
    CHECK(parsed.loss == 250);
    CHECK(parsed.pv_uci == std::vector<std::string>{"e2e4", "e7e5", "g1f3"});
}

TEST_CASE("parse positive and negative mate scores", "[unit]") {
    const auto positive = parse_info_line("info depth 22 score mate 3 wdl 900 80 20 pv e2e8");
    REQUIRE(positive.has_value());
    const Line positive_line = positive.value_or(Line{});
    CHECK(positive_line.mate == 3);
    CHECK_FALSE(positive_line.cp.has_value());

    const auto negative = parse_info_line("info depth 19 score mate -5 wdl 20 80 900 pv e2e1");
    REQUIRE(negative.has_value());
    const Line negative_line = negative.value_or(Line{});
    CHECK(negative_line.mate == -5);
    CHECK_FALSE(negative_line.cp.has_value());
}

TEST_CASE("discard incomplete or bound Stockfish info lines", "[unit]") {
    CHECK_FALSE(parse_info_line("info depth 10 score cp 25 lowerbound wdl 400 300 300 pv e2e4")
                    .has_value());
    CHECK_FALSE(parse_info_line("info depth 10 score mate 2 wdl 800 100 100 upperbound pv e2e4")
                    .has_value());
    CHECK_FALSE(parse_info_line("info depth 10 score cp 25 pv e2e4").has_value());
    CHECK_FALSE(parse_info_line("info depth 10 score cp 25 wdl 400 300 300").has_value());
    CHECK_FALSE(parse_info_line("info depth 10 currmove e2e4 currmovenumber 1").has_value());
    CHECK_FALSE(parse_info_line("info string searching the hash table").has_value());
}

TEST_CASE("default multipv and reject malformed numeric fields", "[unit]") {
    const auto line = parse_info_line("info depth 8 score cp 0 wdl 350 300 350 pv d2d4");
    REQUIRE(line.has_value());
    const Line parsed = line.value_or(Line{});
    CHECK(parsed.multipv == 1);

    CHECK_FALSE(parse_info_line("info depth nope score cp 0 wdl 350 300 350 pv d2d4").has_value());
    CHECK_FALSE(
        parse_info_line("info depth 8 multipv x score cp 0 wdl 350 300 350 pv d2d4").has_value());
    CHECK_FALSE(parse_info_line("info depth 8 score cp bad wdl 350 300 350 pv d2d4").has_value());
    CHECK_FALSE(parse_info_line("info depth 8 score cp 0 wdl 350 300 x pv d2d4").has_value());
    CHECK_FALSE(parse_info_line("info depth 8 score cp 0 nodes invalid wdl 350 300 350 pv d2d4")
                    .has_value());
    CHECK_FALSE(parse_info_line("currmove d2d4").has_value());
}
