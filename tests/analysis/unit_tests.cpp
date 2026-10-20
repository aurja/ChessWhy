#include "chesswhy/analysis.hpp"

#include <array>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis node budgets match the frozen search table", "[analysis][unit]") {
    const auto quick = chesswhy::analysis::node_budget(chesswhy::Budget::Quick);
    CHECK(quick.a == 250'000);
    CHECK(quick.b == 50'000);
    CHECK(quick.c == 50'000);
    CHECK(quick.d == 25'000);
    CHECK(quick.e == 25'000);
    const auto standard = chesswhy::analysis::node_budget(chesswhy::Budget::Standard);
    CHECK(standard.a == 1'000'000);
    CHECK(standard.b == 200'000);
    CHECK(standard.c == 200'000);
    CHECK(standard.d == 100'000);
    CHECK(standard.e == 100'000);
    const auto deep = chesswhy::analysis::node_budget(chesswhy::Budget::Deep);
    CHECK(deep.a == 4'000'000);
    CHECK(deep.b == 800'000);
    CHECK(deep.c == 800'000);
    CHECK(deep.d == 400'000);
    CHECK(deep.e == 400'000);
}

TEST_CASE("expected score keeps WDL units exact", "[analysis][unit]") {
    chesswhy::engine::Line line;
    line.win = 437;
    line.draw = 281;
    line.loss = 282;
    CHECK(chesswhy::analysis::es_units(line) == 1'155);
}

TEST_CASE("verdict boundaries are inclusive at fine, mistake and blunder", "[analysis][unit]") {
    using chesswhy::Verdict;
    using chesswhy::analysis::verdict_for_loss;

    CHECK(verdict_for_loss(0) == Verdict::Fine);
    CHECK(verdict_for_loss(60) == Verdict::Fine);
    CHECK(verdict_for_loss(61) == Verdict::Inaccuracy);
    CHECK(verdict_for_loss(259) == Verdict::Inaccuracy);
    CHECK(verdict_for_loss(260) == Verdict::Mistake);
    CHECK(verdict_for_loss(539) == Verdict::Mistake);
    CHECK(verdict_for_loss(540) == Verdict::Blunder);
}

TEST_CASE("null move reliability depends on side-to-move non-pawn material", "[analysis][unit]") {
    const chess::Board zero_or_one("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
    const chess::Board one_piece("4k3/8/8/8/8/8/4P3/3NK3 w - - 0 1");
    const chess::Board two_pieces("4k3/8/8/8/8/8/4P3/2BNK3 w - - 0 1");
    const chess::Board black_side("4k3/8/8/8/8/8/4P3/2BNK3 b - - 0 1");

    CHECK(chesswhy::analysis::null_move_prone(zero_or_one));
    CHECK(chesswhy::analysis::null_move_prone(one_piece));
    CHECK_FALSE(chesswhy::analysis::null_move_prone(two_pieces));
    CHECK(chesswhy::analysis::null_move_prone(black_side));
}

TEST_CASE("walked plies follows the six-ply horizon and capture extension", "[analysis][unit]") {
    const chess::Board root("7k/8/8/6br/7p/5N2/R7/K3Q3 w - - 0 1");
    const std::array pv{std::string("a2a3"), std::string("h8g8"), std::string("a1b1"),
                        std::string("g8h8"), std::string("a3a4"), std::string("h8g8"),
                        std::string("a4h4"), std::string("h5h4"), std::string("f3h4"),
                        std::string("g5h4"), std::string("e1h4")};
    CHECK(chesswhy::analysis::walked_plies(root, pv) == chesswhy::analysis::kLinePlies);

    const std::array quiet_seventh{std::string("a2a3"), std::string("h8g8"), std::string("a1b1"),
                                   std::string("g8h8"), std::string("a3a4"), std::string("h8g8"),
                                   std::string("e1e2")};
    CHECK(chesswhy::analysis::walked_plies(root, quiet_seventh) ==
          chesswhy::analysis::kHorizonPlies);
}

TEST_CASE("walked plies stops at illegal moves and the end of a short PV", "[analysis][unit]") {
    const chess::Board root("k7/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const std::array illegal_pv{std::string("e2e3"), std::string("a8a7"), std::string("a1a8")};
    CHECK(chesswhy::analysis::walked_plies(root, illegal_pv) == 2);

    const std::array short_pv{std::string("e2e3")};
    CHECK(chesswhy::analysis::walked_plies(root, short_pv) == 1);
}

TEST_CASE("quiet consequence is positional and a knight capture is material", "[analysis][unit]") {
    const chesswhy::engine::Line line;
    const chess::Board quiet_root("k7/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const std::array quiet_pv{std::string("e2e3")};
    const auto quiet = chesswhy::analysis::consequence(quiet_root, quiet_pv, line);
    CHECK(quiet.cls == chesswhy::ConsequenceClass::Positional);
    CHECK(quiet.material == 0);
    CHECK(quiet.plies == 1);

    const chess::Board capture_root("4k3/8/8/4n3/3B4/8/8/4K3 w - - 0 1");
    const std::array capture_pv{std::string("d4e5")};
    const auto capture = chesswhy::analysis::consequence(capture_root, capture_pv, line);
    CHECK(capture.cls == chesswhy::ConsequenceClass::Material);
    CHECK(capture.material == 3);
    CHECK(chesswhy::analysis::consequence_rank(capture) == 3);
}

TEST_CASE("consequence extends captures after six plies and stops at ten", "[analysis][unit]") {
    const chess::Board root("7k/8/8/6br/7p/5N2/R7/K3Q3 w - - 0 1");
    const std::array pv{std::string("a2a3"), std::string("h8g8"), std::string("a1b1"),
                        std::string("g8h8"), std::string("a3a4"), std::string("h8g8"),
                        std::string("a4h4"), std::string("h5h4"), std::string("f3h4"),
                        std::string("g5h4"), std::string("e1h4")};
    const chesswhy::engine::Line line;
    const auto result = chesswhy::analysis::consequence(root, pv, line);
    CHECK(result.plies == 10);
    CHECK(result.material == -2);
    CHECK(result.cls == chesswhy::ConsequenceClass::Positional);
}

TEST_CASE("consequence nets captures made by both sides", "[analysis][unit]") {
    const chess::Board root("k3r3/8/8/4n3/3B4/8/8/7K w - - 0 1");
    const std::array pv{std::string("d4e5"), std::string("e8e5")};
    const chesswhy::engine::Line line;
    const auto result = chesswhy::analysis::consequence(root, pv, line);
    CHECK(result.material == 0);
    CHECK(result.cls == chesswhy::ConsequenceClass::Positional);
}

TEST_CASE("promotion counts piece change but remains promotion class below one pawn",
          "[analysis][unit]") {
    const chess::Board root("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    const std::array pv{std::string("a7a8q"), std::string("b8a8")};
    const chesswhy::engine::Line line;
    const auto result = chesswhy::analysis::consequence(root, pv, line);
    CHECK(result.promotes);
    CHECK(result.material == -1);
    CHECK(result.cls == chesswhy::ConsequenceClass::Promotion);
    CHECK(chesswhy::analysis::consequence_rank(result) == 1);
}

TEST_CASE("positive mate score takes precedence over other consequence classes",
          "[analysis][unit]") {
    const chess::Board root("k7/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const std::array pv{std::string("e2e3")};
    chesswhy::engine::Line line;
    line.mate = 2;
    const auto result = chesswhy::analysis::consequence(root, pv, line);
    CHECK(result.cls == chesswhy::ConsequenceClass::Mate);
    CHECK(result.mate_in == 2);
    CHECK(chesswhy::analysis::consequence_rank(result) == 4);
}

TEST_CASE("illegal PV suffix stops consequence walking defensively", "[analysis][unit]") {
    const chess::Board root("k7/8/8/8/8/8/4R3/4K3 w - - 0 1");
    const std::array pv{std::string("e2e3"), std::string("a8a7"), std::string("a1a8")};
    const chesswhy::engine::Line line;
    const auto result = chesswhy::analysis::consequence(root, pv, line);
    CHECK(result.plies == 2);
}

TEST_CASE("consequence rank follows the frozen class table", "[analysis][unit]") {
    using chesswhy::Consequence;
    using chesswhy::ConsequenceClass;
    using chesswhy::analysis::consequence_rank;

    CHECK(consequence_rank(Consequence{ConsequenceClass::Mate, 1, 0, false, 1}) == 4);
    CHECK(consequence_rank(Consequence{ConsequenceClass::Material, std::nullopt, 3, false, 1}) ==
          3);
    CHECK(consequence_rank(Consequence{ConsequenceClass::Material, std::nullopt, 1, false, 1}) ==
          2);
    CHECK(consequence_rank(Consequence{ConsequenceClass::Material, std::nullopt, 0, false, 1}) ==
          0);
    CHECK(consequence_rank(Consequence{ConsequenceClass::Promotion, std::nullopt, 0, true, 1}) ==
          1);
    CHECK(consequence_rank(Consequence{ConsequenceClass::Positional, std::nullopt, 0, false, 1}) ==
          0);
}
