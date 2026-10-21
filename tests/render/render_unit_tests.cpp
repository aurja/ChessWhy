#include "chesswhy/render.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../report/example_reports.hpp"

TEST_CASE("example reports match the golden text", "[unit]") {
    constexpr std::array<const char*, 8> filenames{
        "terminal_checkmate.txt", "side_in_check.txt", "normal_position.txt", "pgn_opening.txt",
        "tactic_with_labels.txt", "forced_judgement.txt", "negative_margin_judgement.txt",
        "refutation_judgement.txt"};
    const auto reports = chesswhy::test_examples::all_reports();

    for (std::size_t index = 0; index < reports.size(); ++index) {
        CAPTURE(filenames[index]);
        const std::string path =
            std::string(CHESSWHY_SOURCE_DIR) + "/tests/render/golden/" + filenames[index];
        const std::ifstream golden_file(path, std::ios::binary);
        REQUIRE(golden_file.good());
        std::ostringstream golden_content;
        golden_content << golden_file.rdbuf();
        const std::string rendered = chesswhy::render::text(reports[index]);
        CHECK(rendered == golden_content.str());

        std::istringstream rendered_lines(rendered);
        std::string line;
        while (std::getline(rendered_lines, line)) {
            if (line.empty()) {
                continue;
            }
            const std::size_t provenance = line.rfind(" [");
            CHECK(provenance != std::string::npos);
            if (provenance != std::string::npos) {
                CHECK(line.back() == ']');
                CHECK(provenance + 3 < line.size());
                CHECK(line.find(']', provenance + 2) == line.size() - 1);
            }
        }
    }
}

TEST_CASE("motif rows and level-independent output", "[unit]") {
    auto report = chesswhy::test_examples::normal_position();
    const std::string club_text = chesswhy::render::text(report);
    report.input.level = chesswhy::Level::Beginner;
    CHECK(chesswhy::render::text(report) == club_text);
    CHECK(club_text.find("fork: N e5 → r c6, k g6 (ply 3) [rule]") != std::string::npos);
    CHECK(club_text.find("hanging_piece: n d5, gain 3 (ply 1) [rule]") != std::string::npos);
    CHECK(club_text.find("skewer: q f6 → R f1 (ply 1) [rule]") == std::string::npos);
    CHECK(club_text.find("back_rank_weakness: k e8 (ply 0) [rule]") == std::string::npos);
}

TEST_CASE("tactic judgements, labels and contributions render from report fields", "[unit]") {
    const std::string tactic_text =
        chesswhy::render::text(chesswhy::test_examples::tactic_with_labels());
    CHECK(tactic_text.find("tactic: next best g6 is 0.49 worse [engine]") != std::string::npos);
    CHECK(tactic_text.find("removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]") !=
          std::string::npos);
    CHECK(tactic_text.find("fork: n f3 → K e1, Q d4 (ply 1, direct) [rule]") !=
          std::string::npos);
    CHECK(tactic_text.find("restricted movement (absolute pin): R e1 → n e5, k e8 (ply 0) "
                           "[rule]") != std::string::npos);
    CHECK(tactic_text.find("restricted movement (relative pin): B b1 → n c2, q e4 (ply 0) "
                           "[rule]") != std::string::npos);
    CHECK(tactic_text.find("restricted movement (overloaded defender): n d5 → R e4, Q c4 "
                           "(ply 1) [rule]") != std::string::npos);
    CHECK(tactic_text.find("restricted movement (tactic: Nd5-f4 fork): n d5 → "
                           "P g3, Q e2, K g2 (ply 1, direct) [rule]") != std::string::npos);
    CHECK(tactic_text.find("back_rank_weakness: k e8") == std::string::npos);
    const auto asked_copy_report = chesswhy::test_examples::tactic_with_asked_move();
    const std::string asked_copy_text = chesswhy::render::text(asked_copy_report);
    const std::size_t asked_line = asked_copy_text.find("Asked move line: Nf6 Qxf7# [engine]");
    REQUIRE(asked_line != std::string::npos);
    CHECK(asked_copy_text.find("tactic: next best g6 is 0.49 worse [engine]", asked_line) !=
          std::string::npos);

    const std::string forced_text =
        chesswhy::render::text(chesswhy::test_examples::forced_judgement());
    CHECK(forced_text.find("forced: only legal move [rule]") != std::string::npos);
    CHECK(forced_text.find("tactic:") == std::string::npos);

    const std::string negative_text =
        chesswhy::render::text(chesswhy::test_examples::negative_margin_judgement());
    CHECK(negative_text.find("tactic:") == std::string::npos);
    CHECK(negative_text.find("forced:") == std::string::npos);

    const std::string refutation_text =
        chesswhy::render::text(chesswhy::test_examples::refutation_judgement());
    CHECK(refutation_text.find("Candidate 1 refutation line: Qxf7# [engine]\n"
                               "tactic: next best Qe2 is 0.20 worse [engine]\n"
                               "removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]") !=
          std::string::npos);
    CHECK(refutation_text.find("fork: Q h5 → k f7, k e8 (ply 1, direct) [rule]") !=
          std::string::npos);
}

TEST_CASE("position evaluation renders the mate score when available", "[unit]") {
    auto report = chesswhy::test_examples::normal_position();
    report.position.eval_white = chesswhy::Eval{.es_units = 1000, .cp = std::nullopt, .mate = 3};
    const std::string rendered = chesswhy::render::text(report);
    CHECK(rendered.find("Position evaluation: 50.0% ES, mate=3 [engine]") != std::string::npos);
}
