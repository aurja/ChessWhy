#include "chesswhy/engine.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using chesswhy::engine::EngineError;
using chesswhy::engine::Line;
using chesswhy::engine::MultiPvCollector;
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

TEST_CASE("MultiPV collector reconciles a bound final F06 batch", "[unit]") {
    MultiPvCollector collector;
    collector.add("info depth 15 seldepth 24 multipv 1 score cp 81 nodes 542000 nps 680000 "
                  "time 797 hashfull 21 wdl 680 250 70 pv e7b7 e2e1");
    collector.add("info depth 15 seldepth 24 multipv 2 score cp 24 nodes 542000 nps 680000 "
                  "time 797 hashfull 21 wdl 520 400 80 pv h1g1 e2e1");
    collector.add("info depth 15 seldepth 24 multipv 3 score cp 12 nodes 542000 nps 680000 "
                  "time 797 hashfull 21 wdl 500 400 100 pv e7c7 e2e1");
    collector.add("info depth 15 seldepth 24 multipv 4 score cp -18 nodes 542000 nps 680000 "
                  "time 797 hashfull 21 wdl 450 400 150 pv h2h3 e2e1");
    collector.add("info depth 15 seldepth 24 multipv 5 score cp -25 nodes 542000 nps 680000 "
                  "time 797 hashfull 21 wdl 430 410 160 pv b2b3 e2e1");

    collector.add("info depth 16 seldepth 25 multipv 1 score cp 83 nodes 1000000 nps 690000 "
                  "time 1449 hashfull 28 wdl 685 245 70 pv e7b7 e2e1");
    collector.add("info depth 16 seldepth 25 multipv 2 score cp 26 nodes 1000000 nps 690000 "
                  "time 1449 hashfull 28 wdl 525 395 80 pv h1g1 e2e1");
    collector.add("info depth 16 seldepth 25 multipv 3 score cp 15 nodes 1000000 nps 690000 "
                  "time 1449 hashfull 28 wdl 505 395 100 pv b2b3 e2e1");
    collector.add("info depth 16 seldepth 25 multipv 4 score cp -16 nodes 1000000 nps 690000 "
                  "time 1449 hashfull 28 wdl 455 395 150 pv e7c7 e2e1");
    collector.add("info depth 16 seldepth 25 multipv 5 score cp -12 upperbound nodes 1000000 "
                  "nps 690000 time 1449 hashfull 28 wdl 470 390 140 pv h2h3 e2e1");

    const auto lines = std::move(collector).finish();
    REQUIRE(lines.size() == 5);
    const std::vector<std::string> expected_moves{"e7b7", "h1g1", "b2b3", "e7c7", "h2h3"};
    for (std::size_t index = 0; index < lines.size(); ++index) {
        REQUIRE_FALSE(lines[index].pv_uci.empty());
        CHECK(lines[index].pv_uci.front() == expected_moves[index]);
        CHECK(lines[index].multipv == static_cast<int>(index + 1));
    }
    CHECK(lines[0].depth == 16);
    CHECK(lines[0].cp == 83);
    CHECK(lines[4].depth == 15);
    CHECK(lines[4].cp == -18);
}

TEST_CASE("MultiPV bound move falls back to an unused exact slot line", "[unit]") {
    MultiPvCollector collector;
    collector.add("info depth 8 seldepth 14 multipv 1 score cp 20 nodes 12000 nps 600000 "
                  "time 20 wdl 520 400 80 pv e2e4");
    collector.add("info depth 8 seldepth 14 multipv 2 score cp 10 nodes 12000 nps 600000 "
                  "time 20 wdl 500 400 100 pv d2d4");
    collector.add("info depth 9 seldepth 15 multipv 1 score cp 25 nodes 22000 nps 610000 "
                  "time 36 wdl 530 390 80 pv e2e4");
    collector.add("info depth 9 seldepth 15 multipv 2 score cp 12 lowerbound nodes 22000 "
                  "nps 610000 time 36 wdl 505 395 100 pv c2c4");

    const auto lines = std::move(collector).finish();
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].pv_uci.front() == "e2e4");
    CHECK(lines[1].pv_uci.front() == "d2d4");
    CHECK(lines[1].depth == 8);
    CHECK(lines[1].multipv == 2);
}

TEST_CASE("MultiPV bound move omits a fallback already selected", "[unit]") {
    MultiPvCollector collector;
    collector.add("info depth 8 seldepth 14 multipv 1 score cp 20 nodes 12000 nps 600000 "
                  "time 20 wdl 520 400 80 pv e2e4");
    collector.add("info depth 8 seldepth 14 multipv 2 score cp 10 nodes 12000 nps 600000 "
                  "time 20 wdl 500 400 100 pv d2d4");
    collector.add("info depth 9 seldepth 15 multipv 1 score cp 12 nodes 22000 nps 610000 "
                  "time 36 wdl 505 395 100 pv d2d4");
    collector.add("info depth 9 seldepth 15 multipv 2 score cp 12 upperbound nodes 22000 "
                  "nps 610000 time 36 wdl 505 395 100 pv c2c4");

    const auto lines = std::move(collector).finish();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].pv_uci.front() == "d2d4");
    CHECK(lines[0].depth == 9);
    CHECK(lines[0].multipv == 1);
}

TEST_CASE("MultiPV collector accepts the implicit single slot", "[unit]") {
    MultiPvCollector collector;
    collector.add("info depth 8 seldepth 14 score cp 20 nodes 12000 nps 600000 time 20 "
                  "wdl 520 400 80 pv e2e4");

    const auto lines = std::move(collector).finish();
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].pv_uci.front() == "e2e4");
    CHECK(lines[0].multipv == 1);
}

TEST_CASE("MultiPV collector rejects a search with no exact lines", "[unit]") {
    MultiPvCollector collector;
    collector.add("info depth 8 seldepth 14 multipv 1 score cp 20 lowerbound nodes 12000 "
                  "nps 600000 time 20 wdl 520 400 80 pv e2e4");

    REQUIRE_THROWS_AS(std::move(collector).finish(), EngineError);
}
