#include "chesswhy/move_report.hpp"
#include "chesswhy/render.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
            line.starts_with("Engine lines:") || line.starts_with("Engine line 1:") ||
            line.starts_with("Engine line 1 facts read to ply") ||
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
[[nodiscard]] chesswhy::MoveReportDocument base_move_document() {
    using namespace chesswhy;
    MoveReportDocument document;
    document.input.fen = test_examples::kF03;
    document.input.budget = Budget::Standard;
    document.engine.name = "Stockfish 19";
    document.engine.threads = 1;
    document.engine.hash_mb = 64;
    document.engine.nodes = NodeBudget{
        .a = 1000000, .b = 200000, .c = 200000, .d = 100000, .e = 100000, .l = 30000, .r = 100000};
    document.position = PositionSummary{
        .fen = test_examples::kF03,
        .side_to_move = Side::Black,
        .in_check = false,
        .legal_moves = 28,
        .terminal = std::nullopt,
        .eval_white = Eval{.es_units = 1022, .cp = 15, .mate = std::nullopt},
        .tempo_value = std::nullopt,
    };
    document.opening.dataset = "lichess-org/chess-openings@a6189a3";
    document.games.push_back(GameSourceSummary{
        .id = "lichess_broadcast",
        .table = "broadcasts",
        .platform = "over the board",
        .players = "both rated 2200 or more",
        .months = "2020-01..2026-09",
        .credit = "Lichess broadcast database",
        .max_ply = 40,
        .min_games = 5,
        .found = true,
        .games = 10260,
        .white = std::nullopt,
        .draws = std::nullopt,
        .black = std::nullopt,
    });
    return document;
}

[[nodiscard]] chesswhy::MoveReport report_move(std::optional<int> rank, std::string san,
                                               std::string uci, chesswhy::Verdict verdict, int eval,
                                               int loss, int depth, std::vector<std::string> line,
                                               int walked) {
    chesswhy::MoveReport move;
    move.rank = rank;
    move.san = std::move(san);
    move.uci = std::move(uci);
    move.eval = chesswhy::Eval{.es_units = eval, .cp = std::nullopt, .mate = std::nullopt};
    move.depth = depth;
    move.loss_units = loss;
    move.verdict = verdict;
    move.line_san = std::move(line);
    move.walked_plies = walked;
    move.consequence = chesswhy::Consequence{
        .cls = chesswhy::ConsequenceClass::Positional,
        .mate_in = std::nullopt,
        .material = 0,
        .promotes = false,
        .plies = walked,
    };
    return move;
}

[[nodiscard]] chesswhy::TempoValue tempo_value(chesswhy::Side side, int value_units,
                                               chesswhy::TempoLabel label) {
    return chesswhy::TempoValue{
        .side = side,
        .moves_units = value_units,
        .passes_units = 0,
        .value_units = value_units,
        .label = label,
    };
}

[[nodiscard]] chesswhy::TempoPly tempo_ply(int ply, std::string san, chesswhy::Side side,
                                           chesswhy::TempoPlyLabel label, bool tempo_gained) {
    const bool threatening = label == chesswhy::TempoPlyLabel::Threatening ||
                             label == chesswhy::TempoPlyLabel::CounterThreat;
    return chesswhy::TempoPly{
        .ply = ply,
        .san = std::move(san),
        .side = side,
        .threatening = threatening,
        .by_check = false,
        .threat_rank = threatening ? std::optional<int>{2} : std::optional<int>{1},
        .stops_threat =
            label == chesswhy::TempoPlyLabel::PureReply ? std::optional<bool>{true} : std::nullopt,
        .label = label,
        .tempo_gained = tempo_gained,
    };
}

[[nodiscard]] chesswhy::PositionalChange
positional_change(chesswhy::PositionalTag tag, chesswhy::Side side, std::optional<int> value,
                  bool net_change, std::optional<bool> in_use) {
    using namespace chesswhy;
    PositionalFeature feature{
        .tag = tag,
        .side = side,
        .pieces = {},
        .file = std::nullopt,
        .value = value,
    };
    if (tag == PositionalTag::IsolatedPawn) {
        feature.pieces = {test_examples::piece_on_square('c', 2, 'P')};
    }
    return PositionalChange{
        .ply = 2,
        .change = PositionalChangeKind::Appears,
        .holds_at_end = true,
        .net_change = net_change,
        .in_use = in_use,
        .feature = std::move(feature),
    };
}

[[nodiscard]] chesswhy::MoveReportDocument impact_order_document(bool tempo_dependent,
                                                                 bool dynamic_first) {
    using namespace chesswhy;
    MoveReportDocument document = base_move_document();
    document.position.tempo_value = tempo_dependent
                                        ? tempo_value(Side::Black, 628, TempoLabel::TempoDependent)
                                        : tempo_value(Side::Black, 0, TempoLabel::NonTempo);
    MoveReport move = report_move(1, "g6", "g7g6", Verdict::Best, 978, 0, 16,
                                  {"g6", "Nc3", "Qa5", "Bf4", "Nf6", "Qd2"}, 6);
    move.tempo_value = tempo_value(
        Side::White, 788, tempo_dependent ? TempoLabel::TempoDependent : TempoLabel::NonTempo);
    move.tempo_gained = std::vector<TempoGained>{
        {.ply = 2, .san = "Nc3", .side = Side::White},
        {.ply = 4, .san = "Bf4", .side = Side::White},
    };
    move.initiative = std::vector<InitiativeRun>{InitiativeRun{
        .holder = Side::White,
        .start_ply = 2,
        .end_ply = 6,
        .tempi = 2,
        .ending = InitiativeEnding::Spent,
        .outcome = InitiativeOutcome::Converted,
    }};
    move.positional = {
        positional_change(PositionalTag::IsolatedPawn, Side::White, std::nullopt, true, true),
        positional_change(PositionalTag::MobilityLead, Side::Black, 12, true, true),
        positional_change(PositionalTag::DoubledPawns, Side::Black, std::nullopt, false,
                          std::nullopt),
        positional_change(PositionalTag::SpaceLead, Side::White, 4, true, false),
    };
    if (dynamic_first) {
        move.impact_order = {ImpactGroup::Tempo, ImpactGroup::PositionalDynamic,
                             ImpactGroup::PositionalStatic};
    } else {
        move.impact_order = {ImpactGroup::PositionalStatic, ImpactGroup::PositionalDynamic,
                             ImpactGroup::Tempo};
    }
    document.moves.push_back(std::move(move));
    return document;
}

[[nodiscard]] std::array<std::pair<const char*, chesswhy::MoveReportDocument>, 6>
move_report_examples() {
    using namespace chesswhy;
    using namespace chesswhy::test_examples;
    auto best = base_move_document();
    best.position.tempo_value = tempo_value(Side::Black, 628, TempoLabel::TempoDependent);

    best.input.selection.keywords = {Selection::Keyword::Best};
    best.selection = SelectionInfo{
        .selection = best.input.selection,
        .lines_searched = 5,
        .playable_count = 4,
        .playable_truncated = false,
    };
    MoveReport best_move =
        report_move(1, "g6", "g7g6", Verdict::Best, 978, 0, 17, {"g6", "d3", "Bc5", "Nf3"}, 3);
    best_move.tempo_value = tempo_value(Side::White, 788, TempoLabel::TempoDependent);
    best_move.tempo_gained = std::vector<TempoGained>{{.ply = 2, .san = "d3", .side = Side::White}};
    best_move.initiative = std::vector<InitiativeRun>{InitiativeRun{
        .holder = Side::White,
        .start_ply = 2,
        .end_ply = 4,
        .tempi = 1,
        .ending = InitiativeEnding::Spent,
        .outcome = InitiativeOutcome::Converted,
    }};
    best_move.positional.push_back(
        positional_change(PositionalTag::MobilityLead, Side::Black, 12, true, true));
    best_move.positional.push_back(
        positional_change(PositionalTag::IsolatedPawn, Side::White, std::nullopt, true, true));

    best_move.opportunity = OpportunityJudgement{
        .alternative =
            BestAlternative{
                .san = "Bc5",
                .uci = "f8c5",
                .eval = Eval{.es_units = 558, .cp = 15, .mate = std::nullopt},
            },
        .margin_units = 420,
        .is_opportunity = true,
    };
    best_move.motifs.push_back(motif(MotifTag::Fork, Side::Black, 2, piece_on_square('e', 5, 'N'),
                                     {piece_on_square('c', 6, 'r'), piece_on_square('g', 6, 'k')}));
    best_move.motifs.back().contribution = MotifContribution::Direct;
    Motif root_motif =
        motif(MotifTag::BackRankWeakness, Side::Black, 0, piece_on_square('e', 8, 'k'));
    root_motif.contribution = MotifContribution::Indirect;
    best_move.root_motifs.push_back(std::move(root_motif));
    best_move.positional.push_back(PositionalChange{
        .ply = 2,
        .change = PositionalChangeKind::Appears,
        .holds_at_end = true,
        .net_change = false,
        .in_use = std::nullopt,
        .feature =
            PositionalFeature{
                .tag = PositionalTag::IsolatedPawn,
                .side = Side::White,
                .pieces = {piece_on_square('c', 2, 'P')},
                .file = std::nullopt,
                .value = std::nullopt,
            },
    });
    best_move.parried_threat = ParriedThreat{
        .threat = threat(Side::White, test_examples::kF03, {"Qxf7#"},
                         consequence(ConsequenceClass::Mate, 0, 1, false, 1)),
        .test = ParryTest::Searched,
    };
    best_move.created_threat = threat(Side::Black, test_examples::kF03, {"Nxe4"},
                                      consequence(ConsequenceClass::Material, 1, 6));
    best_move.impact_order = {
        ImpactGroup::ParriedThreat, ImpactGroup::CreatedThreat,     ImpactGroup::Motifs,
        ImpactGroup::Tempo,         ImpactGroup::PositionalDynamic, ImpactGroup::PositionalStatic};

    best_move.games = 5421;
    best.moves.push_back(std::move(best_move));

    auto playable = base_move_document();
    playable.input.selection.keywords = {Selection::Keyword::Playable};
    playable.selection = SelectionInfo{
        .selection = playable.input.selection,
        .lines_searched = 28,
        .playable_count = 4,
        .playable_truncated = false,
    };
    playable.position.legal_moves = 31;
    const std::array<std::pair<const char*, const char*>, 4> playable_sans{{
        {"Nf6", "g8f6"},
        {"d6", "d7d6"},
        {"g6", "g7g6"},
        {"Bc5", "f8c5"},
    }};
    for (std::size_t index = 0; index < playable_sans.size(); ++index) {
        const auto [san, uci] = playable_sans[index];
        playable.moves.push_back(report_move(
            static_cast<int>(index + 1), san, uci, index == 0 ? Verdict::Best : Verdict::Okay,
            978 - (static_cast<int>(index) * 15), static_cast<int>(index) * 15, 16, {san}, 1));
    }

    auto blunder = base_move_document();
    blunder.input.selection.keywords = {Selection::Keyword::Blunders};
    blunder.selection = SelectionInfo{
        .selection = blunder.input.selection,
        .lines_searched = 28,
        .playable_count = 4,
        .playable_truncated = false,
    };
    MoveReport bad_move =
        report_move(6, "Nf6", "g8f6", Verdict::Blunder, 534, 700, 12, {"Nf6", "Qxf7#"}, 2);
    bad_move.allows_tactic = true;
    Threat refutation = threat(Side::White, test_examples::kAfterNf6, {"Qxf7#"},
                               consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    refutation.is_tactic = true;
    refutation.opportunity = OpportunityJudgement{
        .alternative =
            BestAlternative{
                .san = "Qe2",
                .uci = "h5e2",
                .eval = Eval{.es_units = 1166, .cp = 0, .mate = std::nullopt},
            },
        .margin_units = 400,
        .is_opportunity = true,
    };
    refutation.motifs.push_back(
        motif(MotifTag::Fork, Side::White, 1, piece_on_square('h', 5, 'Q'),
              {piece_on_square('f', 7, 'k'), piece_on_square('e', 8, 'k')}));
    refutation.motifs.back().contribution = MotifContribution::Direct;
    bad_move.refutation = std::move(refutation);
    blunder.moves.push_back(std::move(bad_move));

    auto asked = base_move_document();
    asked.input.fen = test_examples::kAfterE4E5;
    asked.input.moves = "e2e4";
    asked.input.selection.moves = {"e2e4"};
    asked.position.fen = test_examples::kAfterE4E5;
    asked.position.side_to_move = Side::White;
    asked.position.legal_moves = 20;
    asked.position.eval_white = Eval{.es_units = 1000, .cp = 0, .mate = std::nullopt};
    asked.selection = SelectionInfo{
        .selection = asked.input.selection,
        .lines_searched = 5,
        .playable_count = 4,
        .playable_truncated = false,
    };
    asked.moves.push_back(
        report_move(std::nullopt, "e4", "e2e4", Verdict::Okay, 955, 45, 12, {"e4", "e5"}, 2));

    auto terminal = base_move_document();
    terminal.input.selection.keywords = {Selection::Keyword::Best};
    terminal.selection = SelectionInfo{
        .selection = terminal.input.selection,
        .lines_searched = 0,
        .playable_count = 0,
        .playable_truncated = false,
    };
    terminal.position.fen = test_examples::kF13;
    terminal.position.side_to_move = Side::White;
    terminal.position.legal_moves = 0;
    terminal.position.in_check = true;
    terminal.position.terminal = Terminal::Checkmate;
    terminal.position.eval_white.reset();

    auto empty = base_move_document();
    empty.input.selection = Selection{};
    empty.selection = SelectionInfo{
        .selection = empty.input.selection,
        .lines_searched = 5,
        .playable_count = 4,
        .playable_truncated = false,
    };
    empty.moves.clear();

    std::array<std::pair<const char*, MoveReportDocument>, 6> examples{{
        {"move_best.txt", std::move(best)},
        {"move_playable.txt", std::move(playable)},
        {"move_blunder_refutation.txt", std::move(blunder)},
        {"move_asked_c.txt", std::move(asked)},
        {"move_terminal.txt", std::move(terminal)},
        {"move_empty_selection.txt", std::move(empty)},
    }};
    return examples;
}

[[nodiscard]] bool contains_candidate(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text.find("candidate") != std::string::npos;
}

} // namespace

TEST_CASE("PostM3_EM2 template wording matches its golden lines", "[unit]") {
    using namespace chesswhy;
    Report report = test_examples::base_report(test_examples::kF03, Side::Black);
    report.engine.nodes = NodeBudget{
        .a = 1000000, .b = 200000, .c = 200000, .d = 100000, .e = 100000, .l = 30000, .r = 100000};

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
    const std::string selected = render_template_lines(report);
    const std::ifstream golden_file(path, std::ios::binary);
    REQUIRE(golden_file.good());
    std::ostringstream golden_content;
    golden_content << golden_file.rdbuf();
    CHECK(selected == golden_content.str());
}

TEST_CASE("example reports match the golden text", "[unit]") {
    constexpr std::array<const char*, 8> filenames{"terminal_checkmate.txt",
                                                   "side_in_check.txt",
                                                   "normal_position.txt",
                                                   "pgn_opening.txt",
                                                   "tactic_with_labels.txt",
                                                   "forced_opportunity.txt",
                                                   "negative_margin_opportunity.txt",
                                                   "refutation_opportunity.txt"};
    const auto reports = chesswhy::test_examples::all_reports();

    for (std::size_t index = 0; index < reports.size(); ++index) {
        CAPTURE(filenames[index]);
        const std::string path =
            std::string(CHESSWHY_SOURCE_DIR) + "/tests/render/golden/" + filenames[index];
        const std::string rendered = chesswhy::render::text(reports[index]);
        const std::ifstream golden_file(path, std::ios::binary);
        REQUIRE(golden_file.good());
        std::ostringstream golden_content;
        golden_content << golden_file.rdbuf();
        CHECK(rendered == golden_content.str());
        CHECK_FALSE(contains_candidate(rendered));
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
TEST_CASE("move report documents match golden text without candidate wording", "[unit]") {
    for (auto& [filename, document] : move_report_examples()) {
        CAPTURE(filename);
        const std::string path =
            std::string(CHESSWHY_SOURCE_DIR) + "/tests/render/golden/" + filename;
        const std::string rendered = chesswhy::render::text(document);
        CHECK_FALSE(contains_candidate(rendered));
        const std::ifstream golden_file(path, std::ios::binary);
        REQUIRE(golden_file.good());
        std::ostringstream golden_content;
        golden_content << golden_file.rdbuf();
        CHECK(rendered == golden_content.str());
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
    CHECK(club_text.find("back_rank_weakness: k e8 (ply 0) [rule]") != std::string::npos);
}

TEST_CASE("positional and game facts render only from report fields", "[unit]") {
    const std::string rendered = chesswhy::render::text(chesswhy::test_examples::normal_position());

    CHECK(rendered.find("+ mobility_lead: white 12 (ply 1) [rule]") != std::string::npos);
    CHECK(rendered.find("- castling_kingside: white (ply 1, not at end) "
                        "(passing change) [rule]") != std::string::npos);
    CHECK(rendered.find("+ doubled_pawns: black, c file: p c6, p c7 (ply 2) "
                        "(passing change) [rule]") != std::string::npos);
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
    CHECK(tactic_text.find("Opportunity: margin 0.49 over g6 [engine]") != std::string::npos);
    CHECK(tactic_text.find("removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]") !=
          std::string::npos);
    CHECK(tactic_text.find("fork: n f3 → K e1, Q d4 (ply 1, direct) [rule]") != std::string::npos);
    CHECK(tactic_text.find("back_rank_weakness: k e8 (ply 0) [rule]") != std::string::npos);

    const std::string asked_text =
        chesswhy::render::text(chesswhy::test_examples::normal_position());
    CHECK(asked_text.find("Asked move 1 line: Nf6 Qxf7# [engine]") != std::string::npos);
    CHECK(asked_text.find("Refuted by: Qxf7# ; a tactic: White mates in 1 [engine]") !=
          std::string::npos);

    const std::string forced_text =
        chesswhy::render::text(chesswhy::test_examples::forced_opportunity());
    CHECK(forced_text.find("Forced: only legal move [rule]") != std::string::npos);

    const std::string negative_text =
        chesswhy::render::text(chesswhy::test_examples::negative_margin_opportunity());
    CHECK(negative_text.find("Opportunity:") == std::string::npos);

    const std::string refutation_text =
        chesswhy::render::text(chesswhy::test_examples::refutation_opportunity());
    CHECK(refutation_text.find("Refuted by: Qxf7# ; a tactic: White mates in 1 [engine]\n"
                               "Opportunity: margin 0.20 over Qe2 [engine]\n"
                               "removal_of_defender: b d5 → N d5, R e4 (ply 1) [rule]") !=
          std::string::npos);
    CHECK(refutation_text.find("fork: Q h5 → k f7, k e8 (ply 1, direct) [rule]") !=
          std::string::npos);
}
TEST_CASE("non-detailed engine lines omit unsearched details", "[unit]") {
    using namespace chesswhy;
    Report report = test_examples::normal_position();
    MoveAssessment& line = report.candidates.front();
    line.detailed = false;
    line.parries_threat = ParryInfo{.parries = true, .test = ParryTest::Searched};
    line.creates_threat =
        test_examples::threat(Side::Black, test_examples::kF03, {"Nxe4"},
                              test_examples::consequence(ConsequenceClass::Material, 1, 6));
    line.refutation =
        test_examples::threat(Side::White, test_examples::kAfterNf6, {"Qxf7#"},
                              test_examples::consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    line.refutation->is_tactic = true;
    line.refutation->opportunity = OpportunityJudgement{
        .alternative =
            BestAlternative{
                .san = "Qe2",
                .uci = "h5e2",
                .eval = Eval{.es_units = 1166, .cp = 0, .mate = std::nullopt},
            },
        .margin_units = 400,
        .is_opportunity = true,
    };

    const std::string rendered = render::text(report);
    const std::size_t start = rendered.find("Engine line 1:");
    const std::size_t end = rendered.find("Engine line 2:");
    REQUIRE(start != std::string::npos);
    REQUIRE(end != std::string::npos);
    const std::string line_text = rendered.substr(start, end - start);
    CHECK(line_text.find("Refuted by: Qxf7#") != std::string::npos);
    CHECK(line_text.find("parries threat") == std::string::npos);
    CHECK(line_text.find("creates threat") == std::string::npos);
    CHECK(line_text.find("Opportunity:") == std::string::npos);
}

TEST_CASE("position evaluation renders the mate score when available", "[unit]") {
    auto report = chesswhy::test_examples::normal_position();
    report.position.eval_white = chesswhy::Eval{.es_units = 1000, .cp = std::nullopt, .mate = 3};
    const std::string rendered = chesswhy::render::text(report);
    CHECK(rendered.find("Position evaluation: 50.0% ES, mate=3 [engine]") != std::string::npos);
}

TEST_CASE("full report tempo rows follow the v7 templates", "[unit]") {
    using namespace chesswhy;
    const Report base = test_examples::normal_position();

    const std::array<std::pair<TempoLabel, const char*>, 3> root_labels{{
        {TempoLabel::TempoDependent, "tempo-dependent position"},
        {TempoLabel::NonTempo, "non-tempo position"},
        {TempoLabel::Zugzwang, "zugzwang"},
    }};
    for (const auto& [label, rendered_label] : root_labels) {
        Report report = base;
        report.position.tempo_value = tempo_value(Side::Black, 628, label);
        const std::string rendered = render::text(report);
        const std::string expected =
            std::string("Tempo value: 31.4% for Black (") + rendered_label + ") [engine]";
        CHECK(rendered.find(expected) != std::string::npos);
        CHECK(rendered.find(expected) > rendered.find("Position evaluation:"));
    }
    Report no_root_value = base;
    no_root_value.position.tempo_value.reset();
    CHECK(render::text(no_root_value).find("Tempo value:") == std::string::npos);

    Report unlabelled = base;
    unlabelled.position.tempo_value = tempo_value(Side::Black, 628, TempoLabel::NonTempo);
    unlabelled.position.tempo_value->label.reset();
    const std::string unlabelled_text = render::text(unlabelled);
    CHECK(unlabelled_text.find("Tempo value: 31.4% for Black [engine]") != std::string::npos);
    CHECK(unlabelled_text.find("Tempo value: 31.4% for Black (") == std::string::npos);

    Report report = base;
    REQUIRE(report.candidates.size() >= 4);
    report.position.tempo_value = tempo_value(Side::Black, 628, TempoLabel::TempoDependent);
    MoveAssessment& detailed = report.candidates[0];
    detailed.tempo_value = tempo_value(Side::White, 788, TempoLabel::TempoDependent);
    detailed.tempo_ledger = TempoLedger{
        .plies =
            {
                tempo_ply(1, "Qxd5", Side::Black, TempoPlyLabel::Threatening, true),
                tempo_ply(2, "Nc3", Side::White, TempoPlyLabel::PureReply, false),
                tempo_ply(3, "Qa5", Side::Black, TempoPlyLabel::Neither, false),
                tempo_ply(4, "Bf4", Side::White, TempoPlyLabel::Threatening, true),
                tempo_ply(5, "Nf6", Side::Black, TempoPlyLabel::PureReply, false),
                tempo_ply(6, "Qd2", Side::White, TempoPlyLabel::Neither, false),
            },
        .tempi = Tempi{.white = 1, .black = 1},
        .runs =
            {
                InitiativeRun{.holder = Side::Black,
                              .start_ply = 1,
                              .end_ply = 3,
                              .tempi = 1,
                              .ending = InitiativeEnding::Spent,
                              .outcome = InitiativeOutcome::Evaporated},
                InitiativeRun{.holder = Side::White,
                              .start_ply = 4,
                              .end_ply = 6,
                              .tempi = 1,
                              .ending = InitiativeEnding::Spent,
                              .outcome = InitiativeOutcome::Converted},
            },
    };
    detailed.tempo_ledger_skipped.reset();
    detailed.positional = {
        positional_change(PositionalTag::MobilityLead, Side::White, 12, false, std::nullopt),
        positional_change(PositionalTag::SpaceLead, Side::Black, 3, true, false),
    };
    report.candidates[1].tempo_ledger.reset();
    report.candidates[1].tempo_ledger_skipped = TempoLedgerSkip::NullMoveUnreliable;
    report.candidates[2].tempo_ledger.reset();
    report.candidates[2].tempo_ledger_skipped = TempoLedgerSkip::InCheck;
    report.candidates[3].tempo_ledger.reset();
    report.candidates[3].tempo_ledger_skipped = TempoLedgerSkip::NotDetailed;

    const std::string rendered = render::text(report);
    CHECK(rendered.find("After the move: tempo value 39.4% for White "
                        "(tempo-dependent position) [engine]") != std::string::npos);
    CHECK(rendered.find("Tempo ledger: 1. Qxd5 threatening, tempo gained; 2. Nc3 pure reply; "
                        "3. Qa5 neither; 4. Bf4 threatening, tempo gained; "
                        "5. Nf6 pure reply; 6. Qd2 neither; White 1 tempo, Black 1 [engine]") !=
          std::string::npos);
    CHECK(rendered.find("Initiative: Black from ply 1, 1 tempo; spent at ply 3; "
                        "evaporated [rule]") != std::string::npos);
    CHECK(rendered.find("Initiative: White from ply 4, 1 tempo; spent at ply 6; "
                        "converted [rule]") != std::string::npos);
    CHECK(rendered.find("Tempo ledger: skipped (null move unreliable) [engine]") !=
          std::string::npos);
    CHECK(rendered.find("Tempo ledger: skipped (S in check) [engine]") != std::string::npos);
    CHECK(rendered.find("Tempo ledger: skipped (line not detailed) [engine]") != std::string::npos);
    CHECK(rendered.find("(passing change) [rule]") != std::string::npos);
    CHECK(rendered.find("(fails the use test) [rule]") != std::string::npos);
    CHECK(rendered.find("because") == std::string::npos);
    CHECK(rendered.find("wins") == std::string::npos);
}

TEST_CASE("move report tempo and positional groups follow impact order", "[unit]") {
    using namespace chesswhy;
    const std::string dynamic_first = render::text(impact_order_document(true, true));
    CHECK(dynamic_first.find("Tempo value: 31.4% for Black "
                             "(tempo-dependent position) [engine]") != std::string::npos);
    CHECK(dynamic_first.find("After the move: tempo value 39.4% for White "
                             "(tempo-dependent position) [engine]") != std::string::npos);
    CHECK(dynamic_first.find("Tempo: White gains a tempo with Nc3 (ply 2) [engine]") !=
          std::string::npos);
    CHECK(dynamic_first.find("Initiative: White from ply 2, 2 tempi; spent at ply 6; "
                             "converted [rule]") != std::string::npos);
    const std::size_t tempo = dynamic_first.find("After the move: tempo value");
    const std::size_t dynamic = dynamic_first.find("By ply 6: + mobility_lead: black 12");
    const std::size_t statik = dynamic_first.find("By ply 6: + isolated_pawn: white");
    REQUIRE(tempo != std::string::npos);
    REQUIRE(dynamic != std::string::npos);
    REQUIRE(statik != std::string::npos);
    CHECK(tempo < dynamic);
    CHECK(dynamic < statik);
    CHECK(dynamic_first.find("doubled_pawns") == std::string::npos);
    CHECK(dynamic_first.find("space_lead") == std::string::npos);
    CHECK(dynamic_first.find("because") == std::string::npos);
    CHECK(dynamic_first.find("wins") == std::string::npos);

    const std::string static_first = render::text(impact_order_document(false, false));
    const std::size_t static_row = static_first.find("By ply 6: + isolated_pawn: white");
    const std::size_t dynamic_row = static_first.find("By ply 6: + mobility_lead: black 12");
    const std::size_t tempo_row = static_first.find("After the move: tempo value");
    REQUIRE(static_row != std::string::npos);
    REQUIRE(dynamic_row != std::string::npos);
    REQUIRE(tempo_row != std::string::npos);
    CHECK(static_row < dynamic_row);
    CHECK(dynamic_row < tempo_row);
    CHECK(static_first.find("doubled_pawns") == std::string::npos);
    CHECK(static_first.find("space_lead") == std::string::npos);
    CHECK(static_first.find("Tempo value: 0.0% for Black (non-tempo position) [engine]") !=
          std::string::npos);
}
