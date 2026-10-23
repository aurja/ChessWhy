#include "chesswhy/render.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../report/example_reports.hpp"

namespace {

[[nodiscard]] std::string render_template_lines(const chesswhy::Report& report) {
    const std::string rendered = chesswhy::render::text(report);
    std::istringstream rendered_lines(rendered);
    std::ostringstream selected_lines;
    std::string line;
    while (std::getline(rendered_lines, line)) {
        if (line.starts_with("Engine:") || line.starts_with("Budget:") ||
            line.starts_with("Candidates:") || line.starts_with("Candidate 1:") ||
            line.starts_with("Candidate 1 facts read to ply") ||
            line.starts_with("Asked move 1 facts read to ply") ||
            line.find("parries threat: false (deferred_by_check") != std::string::npos ||
            line.find("creates threat line (if") != std::string::npos ||
            line.find("creates threat: not searched") != std::string::npos ||
            line.starts_with("restricted movement") || line.starts_with("hanging_piece")) {
            selected_lines << line << '\n';
        }
    }
    return selected_lines.str();
}

[[nodiscard]] chesswhy::MotifRestriction restriction(chesswhy::PieceOnSquare piece,
                                                     std::vector<std::string> restricted,
                                                     std::vector<std::string> free,
                                                     chesswhy::HeldTo held_to) {
    return chesswhy::MotifRestriction{
        .piece = piece,
        .restricted = std::move(restricted),
        .free = std::move(free),
        .held_to = std::move(held_to),
    };
}

} // namespace

TEST_CASE("PostM3_EM2 template wording matches its golden lines", "[unit]") {
    using namespace chesswhy;
    Report report = test_examples::base_report(test_examples::kF03, Side::Black);
    report.engine.nodes =
        NodeBudget{.a = 1000000, .b = 200000, .c = 200000, .d = 100000, .e = 100000, .r = 100000};

    MoveAssessment candidate;
    candidate.san = "g6";
    candidate.uci = "g7g6";
    candidate.eval.es_units = 960;
    candidate.loss_units = 0;
    candidate.verdict = Verdict::Best;
    candidate.depth = 16;
    candidate.line_san = {"g6", "Nf6", "Qe5", "d6", "Qf4", "Nc6", "d4", "g5", "Be3", "Bg7"};
    candidate.walked_plies = 6;
    candidate.creates_threat =
        test_examples::threat(Side::Black, test_examples::kF03, {"Nxe4"},
                              test_examples::consequence(ConsequenceClass::Material, 3, 1));

    const auto add_restriction = [&candidate](MotifTag tag, PieceOnSquare actor,
                                              std::vector<PieceOnSquare> targets,
                                              MotifRestriction motif_restriction) {
        Motif motif;
        motif.tag = tag;
        motif.actor = actor;
        motif.targets = std::move(targets);
        motif.restriction = std::move(motif_restriction);
        candidate.motifs.push_back(std::move(motif));
    };
    add_restriction(
        MotifTag::PinAbsolute, test_examples::piece_on_square('e', 1, 'R'),
        {test_examples::piece_on_square('e', 5, 'n'), test_examples::piece_on_square('e', 8, 'k')},
        restriction(test_examples::piece_on_square('e', 5, 'N'), {"e5c6", "e5d7"}, {},
                    HeldTo{.kind = HeldTo::Kind::Square, .name = "e5"}));
    add_restriction(
        MotifTag::PinRelative, test_examples::piece_on_square('b', 1, 'B'),
        {test_examples::piece_on_square('c', 2, 'n'), test_examples::piece_on_square('e', 4, 'q')},
        restriction(test_examples::piece_on_square('c', 2, 'n'), {"c2c4"}, {"c2c3"},
                    HeldTo{.kind = HeldTo::Kind::File, .name = "e"}));
    add_restriction(
        MotifTag::OverloadedDefender, test_examples::piece_on_square('d', 5, 'n'),
        {test_examples::piece_on_square('e', 4, 'R'), test_examples::piece_on_square('c', 4, 'Q')},
        restriction(test_examples::piece_on_square('d', 5, 'n'), {"d5f4"}, {"d5b4"},
                    HeldTo{.kind = HeldTo::Kind::Rank, .name = "7"}));
    add_restriction(
        MotifTag::PinRelative, test_examples::piece_on_square('d', 5, 'n'),
        {test_examples::piece_on_square('g', 3, 'P'), test_examples::piece_on_square('e', 2, 'Q')},
        restriction(test_examples::piece_on_square('d', 5, 'n'), {"d5e7"}, {"d5f6"},
                    HeldTo{.kind = HeldTo::Kind::Diagonal, .name = "a4-e8"}));
    add_restriction(MotifTag::OverloadedDefender, test_examples::piece_on_square('e', 4, 'q'),
                    {test_examples::piece_on_square('h', 4, 'R')},
                    restriction(test_examples::piece_on_square('e', 4, 'q'), {"e4f4"},
                                {"e4a2", "e4d1", "e4g8"},
                                HeldTo{.kind = HeldTo::Kind::Squares, .name = "a2,d1,g8"}));
    add_restriction(MotifTag::PinRelative, test_examples::piece_on_square('e', 5, 'N'),
                    {test_examples::piece_on_square('f', 3, 'p')},
                    restriction(test_examples::piece_on_square('e', 5, 'N'), {"e5c6"}, {"e5f3"},
                                HeldTo{.kind = HeldTo::Kind::Squares, .name = "f3"}));
    add_restriction(MotifTag::PinAbsolute, test_examples::piece_on_square('a', 8, 'R'),
                    {test_examples::piece_on_square('a', 7, 'p')},
                    restriction(test_examples::piece_on_square('a', 7, 'P'), {"a7a8q"}, {},
                                HeldTo{.kind = HeldTo::Kind::Square, .name = "a7"}));
    candidate.motifs.push_back(test_examples::motif(MotifTag::HangingPiece, Side::White, 3,
                                                    test_examples::piece_on_square('h', 6, 'n'), {},
                                                    3));
    report.candidates.push_back(std::move(candidate));
    MoveAssessment game_over;
    game_over.san = "d6";
    game_over.uci = "d7d6";
    game_over.creates_threat_skipped = CreatesThreatSkip::GameOver;
    report.candidates.push_back(std::move(game_over));
    MoveAssessment no_legal_move;
    no_legal_move.san = "Qf6";
    no_legal_move.uci = "d8f6";
    no_legal_move.creates_threat_skipped = CreatesThreatSkip::NoLegalMove;
    report.candidates.push_back(std::move(no_legal_move));

    MoveAssessment skipped;
    skipped.san = "Nf6";
    skipped.uci = "g8f6";
    skipped.line_san = {"Nf6"};
    skipped.walked_plies = 1;
    skipped.gives_check = true;
    skipped.parries_threat = ParryInfo{.parries = false, .test = ParryTest::DeferredByCheck};
    skipped.creates_threat_skipped = CreatesThreatSkip::GivesCheck;
    report.asked_moves.push_back(std::move(skipped));
    report.input.moves = "Nf6";
    report.input.selection.keywords.clear();
    report.input.selection.moves = {"g8f6"};

    const std::string path =
        std::string(CHESSWHY_SOURCE_DIR) + "/tests/render/golden/postm3_em2_template.txt";
    const std::ifstream golden_file(path, std::ios::binary);
    REQUIRE(golden_file.good());
    std::ostringstream golden_content;
    golden_content << golden_file.rdbuf();
    CHECK(render_template_lines(report) == golden_content.str());
}

TEST_CASE("example reports match the golden text", "[unit]") {
    constexpr std::array<const char*, 8> filenames{
        "terminal_checkmate.txt",        "side_in_check.txt",
        "normal_position.txt",           "pgn_opening.txt",
        "tactic_with_labels.txt",        "forced_opportunity.txt",
        "negative_margin_opportunity.txt", "refutation_opportunity.txt"};
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
    CHECK(club_text.find("promotion: Q e8 (ply 1) [rule]") != std::string::npos);
    CHECK(club_text.find("skewer: q f6 → R f1 (ply 1) [rule]") == std::string::npos);
    CHECK(club_text.find("back_rank_weakness: k e8 (ply 0) [rule]") == std::string::npos);
}

TEST_CASE("positional and game facts render only from report fields", "[unit]") {
    const std::string rendered = chesswhy::render::text(chesswhy::test_examples::normal_position());

    CHECK(rendered.find("+ mobility_lead: white 12 (ply 1) [rule]") != std::string::npos);
    CHECK(rendered.find("- castling_kingside: white (ply 1, not at end) [rule]") !=
          std::string::npos);
    CHECK(rendered.find("+ doubled_pawns: black, c file: p c6, p c7 (ply 2) [rule]") !=
          std::string::npos);
    CHECK(rendered.find("bishop_pair") == std::string::npos);
    CHECK(rendered.find("games: played in 1140 of 1840 games [games]") != std::string::npos);
    CHECK(rendered.find("games: not played in these games [games]") != std::string::npos);
    CHECK(rendered.find("games: 1840 games, both rated 2200 or more "
                        "(platform: over the board (may include online events relayed as "
                        "broadcasts); source: Lichess broadcast database, database.lichess.org, "
                        "CC BY-SA 4.0; months: 2020-01..2026-09): White won 640, draws 790, "
                        "Black won 410 [games]") != std::string::npos);
    const std::string not_found =
        chesswhy::render::text(chesswhy::test_examples::terminal_checkmate());
    CHECK(not_found.find("games: position not in the table "
                         "(it keeps positions reached in at least 5 games within the first 40 "
                         "plies) [games]") != std::string::npos);
}

TEST_CASE("opportunities, tactic labels and contributions render from report fields", "[unit]") {
    const std::string tactic_text =
        chesswhy::render::text(chesswhy::test_examples::tactic_with_labels());
    CHECK(tactic_text.find("opportunity: next best g6 is 0.49 worse [engine]") !=
          std::string::npos);
    CHECK(tactic_text.find("removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]") !=
          std::string::npos);
    CHECK(tactic_text.find("fork: n f3 → K e1, Q d4 (ply 1, direct) [rule]") != std::string::npos);
    CHECK(tactic_text.find("back_rank_weakness: k e8") == std::string::npos);

    const std::string asked_text =
        chesswhy::render::text(chesswhy::test_examples::normal_position());
    CHECK(asked_text.find("Asked move 1 line: Nf6 Qxf7# [engine]") != std::string::npos);
    CHECK(asked_text.find("Asked move 1 refutation line: Qxf7# [engine]") != std::string::npos);

    const std::string forced_text =
        chesswhy::render::text(chesswhy::test_examples::forced_opportunity());
    CHECK(forced_text.find("forced: only legal move [rule]") != std::string::npos);
    CHECK(forced_text.find("opportunity:") == std::string::npos);

    const std::string negative_text =
        chesswhy::render::text(chesswhy::test_examples::negative_margin_opportunity());
    CHECK(negative_text.find("opportunity:") == std::string::npos);
    CHECK(negative_text.find("forced:") == std::string::npos);

    const std::string refutation_text =
        chesswhy::render::text(chesswhy::test_examples::refutation_opportunity());
    CHECK(refutation_text.find("Candidate 2 refutation line: Qxf7# [engine]\n"
                               "opportunity: next best Qe2 is 0.20 worse [engine]\n"
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
