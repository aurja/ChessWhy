#include "chesswhy/analysis.hpp"
#include "chesswhy/positional.hpp"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

[[nodiscard]] std::vector<chesswhy::TempoPly> tempo_plies(std::size_t count) {
    std::vector<chesswhy::TempoPly> result(count);
    for (std::size_t index = 0; index < count; ++index) {
        result[index].ply = static_cast<int>(index + 1);
        result[index].side = index % 2 == 0 ? chesswhy::Side::White : chesswhy::Side::Black;
    }
    return result;
}

[[nodiscard]] std::vector<chess::Move> parse_san_line(const chess::Board& root,
                                                      std::initializer_list<const char*> sans) {
    std::vector<chess::Move> result;
    result.reserve(sans.size());
    chess::Board board = root;
    for (const char* san : sans) {
        const chess::Move move = chess::uci::parseSan(board, san);
        result.push_back(move);
        board.makeMove(move);
    }
    return result;
}

} // namespace
TEST_CASE("analysis node budgets match the frozen search table", "[analysis][unit]") {
    const auto quick = chesswhy::analysis::node_budget(chesswhy::Budget::Quick);
    CHECK(quick.a == 250'000);
    CHECK(quick.b == 50'000);
    CHECK(quick.c == 50'000);
    CHECK(quick.d == 25'000);
    CHECK(quick.e == 25'000);
    CHECK(quick.l == 10'000);
    CHECK(quick.r == 25'000);
    const auto standard = chesswhy::analysis::node_budget(chesswhy::Budget::Standard);
    CHECK(standard.a == 1'000'000);
    CHECK(standard.b == 200'000);
    CHECK(standard.c == 200'000);
    CHECK(standard.d == 100'000);
    CHECK(standard.e == 100'000);
    CHECK(standard.l == 30'000);
    CHECK(standard.r == 100'000);
    const auto deep = chesswhy::analysis::node_budget(chesswhy::Budget::Deep);
    CHECK(deep.a == 4'000'000);
    CHECK(deep.b == 800'000);
    CHECK(deep.c == 800'000);
    CHECK(deep.d == 400'000);
    CHECK(deep.e == 400'000);
    CHECK(deep.l == 160'000);
    CHECK(deep.r == 400'000);
}

TEST_CASE("expected score keeps WDL units exact", "[analysis][unit]") {
    chesswhy::engine::Line line;
    line.win = 437;
    line.draw = 281;
    line.loss = 282;
    CHECK(chesswhy::analysis::es_units(line) == 1'155);
}

TEST_CASE("UCI move line retains a positive search depth", "[analysis][unit]") {
    const auto shallow =
        chesswhy::engine::parse_info_line("info depth 1 score cp 0 wdl 350 300 350 pv d2d4");
    REQUIRE(shallow.has_value());
    if (shallow.has_value()) {
        CHECK(shallow->depth == 1);
    }
    CHECK_FALSE(chesswhy::engine::parse_info_line("info depth 0 score cp 0 wdl 350 300 350 pv d2d4")
                    .has_value());
}

TEST_CASE("verdict boundaries use the playable and inaccuracy cutoffs", "[analysis][unit]") {
    using chesswhy::Verdict;
    using chesswhy::analysis::verdict;

    CHECK(verdict(0, true, false) == Verdict::Best);
    CHECK(verdict(100, true, false) == Verdict::Best);
    CHECK(verdict(100, false, false) == Verdict::Okay);
    CHECK(verdict(101, false, false) == Verdict::Inaccuracy);
    CHECK(verdict(200, false, false) == Verdict::Inaccuracy);
    CHECK(verdict(201, false, false) == Verdict::Mistake);
    CHECK(verdict(201, false, true) == Verdict::Blunder);
}

TEST_CASE("same score compares cp or mate, not expected score", "[analysis][unit]") {
    using chesswhy::Eval;
    using chesswhy::analysis::same_score;

    CHECK(same_score(Eval{.es_units = 1000, .cp = 12, .mate = std::nullopt},
                     Eval{.es_units = 1200, .cp = 12, .mate = std::nullopt}));
    CHECK_FALSE(same_score(Eval{.es_units = 1000, .cp = 12, .mate = std::nullopt},
                           Eval{.es_units = 1000, .cp = 13, .mate = std::nullopt}));
    CHECK(same_score(Eval{.es_units = 1000, .cp = std::nullopt, .mate = 2},
                     Eval{.es_units = 900, .cp = std::nullopt, .mate = 2}));
    CHECK_FALSE(same_score(Eval{.es_units = 1000, .cp = std::nullopt, .mate = 2},
                           Eval{.es_units = 1000, .cp = 12, .mate = std::nullopt}));
    CHECK_FALSE(same_score(Eval{.es_units = 1000, .cp = std::nullopt, .mate = 2},
                           Eval{.es_units = 1000, .cp = std::nullopt, .mate = 3}));
}

TEST_CASE("tactic and allows-tactic use the move's line consequence", "[analysis][unit]") {
    using chesswhy::Consequence;
    using chesswhy::ConsequenceClass;
    using chesswhy::Eval;
    using chesswhy::analysis::allows_tactic;
    using chesswhy::analysis::is_tactic;

    CHECK(is_tactic(Consequence{.cls = ConsequenceClass::Mate,
                                .mate_in = std::nullopt,
                                .material = 0,
                                .promotes = false,
                                .plies = 0}));
    CHECK(is_tactic(Consequence{.cls = ConsequenceClass::Material,
                                .mate_in = std::nullopt,
                                .material = 0,
                                .promotes = false,
                                .plies = 0}));
    CHECK_FALSE(is_tactic(Consequence{.cls = ConsequenceClass::Promotion,
                                      .mate_in = std::nullopt,
                                      .material = 0,
                                      .promotes = false,
                                      .plies = 0}));
    CHECK_FALSE(is_tactic(Consequence{.cls = ConsequenceClass::Positional,
                                      .mate_in = std::nullopt,
                                      .material = 0,
                                      .promotes = false,
                                      .plies = 0}));
    CHECK(allows_tactic(Consequence{.cls = ConsequenceClass::Positional,
                                    .mate_in = std::nullopt,
                                    .material = -1,
                                    .promotes = false,
                                    .plies = 0},
                        Eval{.es_units = 1000, .cp = std::nullopt, .mate = std::nullopt}));
    CHECK_FALSE(allows_tactic(Consequence{.cls = ConsequenceClass::Positional,
                                          .mate_in = std::nullopt,
                                          .material = 0,
                                          .promotes = false,
                                          .plies = 0},
                              Eval{.es_units = 1000, .cp = std::nullopt, .mate = std::nullopt}));
    CHECK(allows_tactic(Consequence{.cls = ConsequenceClass::Positional,
                                    .mate_in = std::nullopt,
                                    .material = 0,
                                    .promotes = false,
                                    .plies = 0},
                        Eval{.es_units = 1000, .cp = std::nullopt, .mate = -1}));
    CHECK_FALSE(allows_tactic(Consequence{.cls = ConsequenceClass::Positional,
                                          .mate_in = std::nullopt,
                                          .material = 0,
                                          .promotes = false,
                                          .plies = 0},
                              Eval{.es_units = 1000, .cp = std::nullopt, .mate = 1}));
}

TEST_CASE("opportunity margin boundary is inclusive", "[analysis][unit]") {
    using chesswhy::analysis::is_opportunity_margin;

    CHECK_FALSE(is_opportunity_margin(259));
    CHECK(is_opportunity_margin(260));
    CHECK_FALSE(is_opportunity_margin(-1));
}

TEST_CASE("selection sets Search A width and nodes", "[analysis][unit]") {
    using chesswhy::Selection;
    using Keyword = Selection::Keyword;
    using chesswhy::analysis::judges_every_move;
    using chesswhy::analysis::multipv_lines;
    using chesswhy::analysis::search_a_nodes;

    const Selection best{.keywords = {Keyword::Best}, .top = std::nullopt, .moves = {}};
    CHECK_FALSE(judges_every_move(best));
    CHECK(multipv_lines(best, 31) == 5);
    CHECK(multipv_lines(best, 3) == 3);
    CHECK(search_a_nodes(chesswhy::Budget::Standard, multipv_lines(best, 31)) == 1'000'000);

    const Selection top_three{.keywords = {}, .top = 3, .moves = {}};
    CHECK(multipv_lines(top_three, 31) == 5);
    CHECK(search_a_nodes(chesswhy::Budget::Standard, multipv_lines(top_three, 31)) == 1'000'000);
    const Selection top_seven{.keywords = {}, .top = 7, .moves = {}};
    CHECK(multipv_lines(top_seven, 31) == 7);
    CHECK(search_a_nodes(chesswhy::Budget::Standard, multipv_lines(top_seven, 31)) == 1'400'000);
    CHECK(multipv_lines(top_seven, 4) == 4);

    constexpr std::array whole_board_keywords{Keyword::Playable,   Keyword::Inaccuracies,
                                              Keyword::Mistakes,   Keyword::Blunders,
                                              Keyword::Unplayable, Keyword::All};
    for (const auto keyword : whole_board_keywords) {
        const Selection selection{.keywords = {keyword}, .top = std::nullopt, .moves = {}};
        CHECK(judges_every_move(selection));
        CHECK(multipv_lines(selection, 31) == 31);
        CHECK(search_a_nodes(chesswhy::Budget::Standard, multipv_lines(selection, 31)) ==
              6'200'000);
        CHECK(multipv_lines(selection, 3) == 3);
    }
    CHECK(multipv_lines(best, 0) == 0);
}

TEST_CASE("first five and selected lines are detailed", "[analysis][unit]") {
    using chesswhy::analysis::is_detailed_line;

    for (int rank = 1; rank <= chesswhy::analysis::kDetailedLines; ++rank) {
        CHECK(is_detailed_line(rank, false));
    }
    CHECK_FALSE(is_detailed_line(6, false));
    CHECK(is_detailed_line(6, true));
    CHECK_FALSE(is_detailed_line(0, true));
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
    const std::array<std::string, 0> empty_pv{};
    CHECK(chesswhy::analysis::walked_plies(root, empty_pv) == 0);
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

TEST_CASE("tempo labels use the frozen boundaries", "[analysis][unit]") {
    using chesswhy::TempoLabel;
    using chesswhy::analysis::tempo_label;

    CHECK(tempo_label(-1) == TempoLabel::Zugzwang);
    CHECK(tempo_label(0) == TempoLabel::NonTempo);
    CHECK(tempo_label(99) == TempoLabel::NonTempo);
    CHECK(tempo_label(100) == std::nullopt);
    CHECK(tempo_label(200) == std::nullopt);
    CHECK(tempo_label(201) == TempoLabel::TempoDependent);
}

TEST_CASE("tempo-dependent positions require a labelled root value", "[analysis][unit]") {
    using chesswhy::TempoLabel;
    using chesswhy::TempoValue;
    using chesswhy::analysis::tempo_dependent;

    CHECK_FALSE(tempo_dependent(std::nullopt));
    TempoValue root;
    root.label = TempoLabel::TempoDependent;
    CHECK(tempo_dependent(root));
    root.label = std::nullopt;
    CHECK_FALSE(tempo_dependent(root));
    root.label = TempoLabel::NonTempo;
    CHECK_FALSE(tempo_dependent(root));
}

TEST_CASE("tempo ledger labels cover threat, reply, counter-threat and neither",
          "[analysis][unit]") {
    using chesswhy::TempoPlyLabel;
    using chesswhy::analysis::label_ledger;

    auto plies = tempo_plies(8);
    plies[0].threatening = true;
    plies[1].stops_threat = true;
    plies[2].threatening = true;
    plies[3].threatening = true;
    plies[3].stops_threat = true;
    plies[4].threatening = true;
    plies[4].stops_threat = false;
    plies[5].stops_threat = false;
    plies[6].stops_threat = true;

    label_ledger(plies);

    CHECK(plies[0].label == TempoPlyLabel::Threatening);
    CHECK(plies[1].label == TempoPlyLabel::PureReply);
    CHECK(plies[2].label == TempoPlyLabel::Threatening);
    CHECK(plies[3].label == TempoPlyLabel::CounterThreat);
    CHECK(plies[4].label == TempoPlyLabel::Threatening);
    CHECK(plies[5].label == TempoPlyLabel::Neither);
    CHECK(plies[6].label == TempoPlyLabel::Neither);
    CHECK(plies[7].label == TempoPlyLabel::Neither);
    CHECK(plies[0].tempo_gained);
    CHECK_FALSE(plies[2].tempo_gained);
    CHECK_FALSE(plies[3].tempo_gained);
    CHECK_FALSE(plies[4].tempo_gained);
    CHECK_FALSE(plies[5].tempo_gained);
    CHECK_FALSE(plies[6].tempo_gained);
    CHECK_FALSE(plies[7].tempo_gained);
}

TEST_CASE("material balance uses pawn, minor, rook and queen values", "[analysis][unit]") {
    const chess::Board board("4k3/3q4/2r1b3/3p4/4P3/3BN3/3R4/4K3 w - - 0 1");

    CHECK(chesswhy::analysis::material_balance(board, chess::Color::WHITE) == -6);
    CHECK(chesswhy::analysis::material_balance(board, chess::Color::BLACK) == 6);
}

TEST_CASE("initiative runs identify each ending", "[analysis][unit]") {
    using chesswhy::InitiativeEnding;
    using chesswhy::analysis::initiative_runs;

    const chess::Board start;
    const auto line = parse_san_line(start, {"e4", "e5", "Nf3", "Nc6"});
    auto spent_plies = tempo_plies(3);
    spent_plies[0].threatening = true;
    spent_plies[1].stops_threat = true;
    chesswhy::analysis::label_ledger(spent_plies);
    auto spent = initiative_runs(spent_plies, start, std::span(line).first(3), {});
    REQUIRE(spent.size() == 1);
    CHECK(spent[0].ending == InitiativeEnding::Spent);
    CHECK(spent[0].start_ply == 1);
    CHECK(spent[0].end_ply == 3);
    CHECK(spent[0].tempi == 1);

    auto unfinished_before_ending = tempo_plies(2);
    unfinished_before_ending[0].threatening = true;
    unfinished_before_ending[1].stops_threat = true;
    chesswhy::analysis::label_ledger(unfinished_before_ending);
    const auto unfinished_short =
        initiative_runs(unfinished_before_ending, start, std::span(line).first(2), {});
    REQUIRE(unfinished_short.size() == 1);
    CHECK(unfinished_short[0].ending == InitiativeEnding::Unfinished);
    CHECK(unfinished_short[0].end_ply == 2);

    auto unfinished_reply = tempo_plies(3);
    unfinished_reply[0].threatening = true;
    unfinished_reply[1].stops_threat = true;
    unfinished_reply[2].threatening = true;
    chesswhy::analysis::label_ledger(unfinished_reply);
    const auto unfinished_after_ending =
        initiative_runs(unfinished_reply, start, std::span(line).first(3), {});
    REQUIRE(unfinished_after_ending.size() == 1);
    CHECK(unfinished_after_ending[0].ending == InitiativeEnding::Unfinished);
    CHECK(unfinished_after_ending[0].end_ply == 3);

    auto contested_plies = tempo_plies(4);
    contested_plies[0].threatening = true;
    contested_plies[1].stops_threat = true;
    contested_plies[2].threatening = true;
    contested_plies[3].threatening = true;
    contested_plies[3].stops_threat = true;
    chesswhy::analysis::label_ledger(contested_plies);
    const auto contested = initiative_runs(contested_plies, start, line, {});
    REQUIRE(contested.size() == 1);
    CHECK(contested[0].ending == InitiativeEnding::Contested);
    CHECK(contested[0].end_ply == 4);

    auto unstopped_plies = tempo_plies(4);
    unstopped_plies[0].threatening = true;
    unstopped_plies[1].stops_threat = true;
    unstopped_plies[2].threatening = true;
    unstopped_plies[3].stops_threat = false;
    chesswhy::analysis::label_ledger(unstopped_plies);
    const auto runs = initiative_runs(unstopped_plies, start, line, {});
    REQUIRE(runs.size() == 1);
    CHECK(runs[0].ending == InitiativeEnding::Runs);
    CHECK(runs[0].end_ply == 4);

    const chess::Board mate_root("7k/5K2/6Q1/p7/8/8/8/R7 w - - 0 1");
    const auto mate_line = parse_san_line(mate_root, {"Ra2", "a4", "Qh5#"});
    auto mate_plies = tempo_plies(3);
    mate_plies[0].threatening = true;
    mate_plies[1].stops_threat = true;
    mate_plies[2].threatening = true;
    chesswhy::analysis::label_ledger(mate_plies);
    const auto mate_run = initiative_runs(mate_plies, mate_root, mate_line, {});
    REQUIRE(mate_run.size() == 1);
    CHECK(mate_run[0].ending == InitiativeEnding::Runs);
    CHECK(mate_run[0].end_ply == 3);
}

TEST_CASE("initiative runs reject invalid labelled ledgers", "[analysis][unit]") {
    using chesswhy::TempoPlyLabel;
    using chesswhy::analysis::initiative_runs;

    const chess::Board root;
    const auto line = parse_san_line(root, {"e4", "e5", "Nf3", "Nc6"});
    auto plies = tempo_plies(4);
    plies[0].tempo_gained = true;
    plies[2].threatening = true;
    plies[3].stops_threat = true;
    plies[3].label = TempoPlyLabel::PureReply;

    CHECK_THROWS_AS(initiative_runs(plies, root, line, {}), std::logic_error);
}

TEST_CASE("initiative scanning resumes at the previous run's ending ply", "[analysis][unit]") {
    using chesswhy::InitiativeEnding;
    using chesswhy::InitiativeOutcome;
    using chesswhy::analysis::initiative_runs;

    const chess::Board start;
    const auto line = parse_san_line(start, {"e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6"});
    auto plies = tempo_plies(6);
    plies[0].threatening = true;
    plies[1].stops_threat = true;
    plies[2].threatening = true;
    plies[3].threatening = true;
    plies[3].stops_threat = true;
    plies[4].stops_threat = true;
    chesswhy::analysis::label_ledger(plies);
    const auto runs = initiative_runs(plies, start, line, {});
    REQUIRE(runs.size() == 2);
    CHECK(runs[0].start_ply == 1);
    CHECK(runs[0].end_ply == 4);
    CHECK(runs[0].ending == InitiativeEnding::Contested);
    CHECK(runs[0].outcome == InitiativeOutcome::Evaporated);
    CHECK(runs[1].start_ply == 4);
    CHECK(runs[1].end_ply == 6);
    CHECK(runs[1].ending == InitiativeEnding::Spent);
    CHECK(runs[1].outcome == InitiativeOutcome::Evaporated);
}

TEST_CASE("initiative conversion checks material and used positional changes", "[analysis][unit]") {
    using chesswhy::InitiativeOutcome;
    using chesswhy::PositionalChange;
    using chesswhy::PositionalChangeKind;
    using chesswhy::PositionalTag;
    using chesswhy::Side;
    using chesswhy::analysis::initiative_runs;

    const chess::Board material_root("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
    const auto material_line = parse_san_line(material_root, {"exd5", "Kd7", "Kf2"});
    auto material_plies = tempo_plies(3);
    material_plies[0].threatening = true;
    material_plies[1].stops_threat = true;
    chesswhy::analysis::label_ledger(material_plies);
    const auto material_runs = initiative_runs(material_plies, material_root, material_line, {});
    REQUIRE(material_runs.size() == 1);
    CHECK(material_runs[0].outcome == InitiativeOutcome::Converted);

    const chess::Board positional_root;
    const auto positional_line = parse_san_line(positional_root, {"e4", "e5", "Nf3"});
    auto positional_plies = tempo_plies(3);
    positional_plies[0].threatening = true;
    positional_plies[1].stops_threat = true;
    chesswhy::analysis::label_ledger(positional_plies);
    PositionalChange used_change;
    used_change.ply = 2;
    used_change.change = PositionalChangeKind::Appears;
    used_change.net_change = true;
    used_change.in_use = true;
    used_change.feature.tag = PositionalTag::RookSeventh;
    used_change.feature.side = Side::White;
    const std::array changes{used_change};
    const auto positional_runs =
        initiative_runs(positional_plies, positional_root, positional_line, changes);
    REQUIRE(positional_runs.size() == 1);
    CHECK(positional_runs[0].outcome == InitiativeOutcome::Converted);

    const auto evaporated = initiative_runs(positional_plies, positional_root, positional_line, {});
    REQUIRE(evaporated.size() == 1);
    CHECK(evaporated[0].outcome == InitiativeOutcome::Evaporated);
}
