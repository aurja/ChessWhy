#pragma once

#include "chesswhy/report.hpp"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace chesswhy::test_examples {

inline constexpr const char* kDataset = "lichess-org/chess-openings@a6189a3";
inline constexpr const char* kF02 = "rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2";
inline constexpr const char* kF03 =
    "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3";
inline constexpr const char* kF13 = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3";
inline constexpr const char* kAfterNf6 =
    "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 4 4";
inline constexpr const char* kAfterE4E5 =
    "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2";

[[nodiscard]] inline Consequence consequence(ConsequenceClass cls, int material, int plies,
                                             bool promotes = false,
                                             std::optional<int> mate_in = std::nullopt) {
    Consequence result;
    result.cls = cls;
    result.mate_in = mate_in;
    result.material = material;
    result.promotes = promotes;
    result.plies = plies;
    return result;
}

[[nodiscard]] inline Threat threat(Side side, std::string root_fen, std::vector<std::string> line,
                                   Consequence result) {
    Threat value;
    value.side = side;
    value.root_fen = std::move(root_fen);
    value.line_san = std::move(line);
    value.consequence = result;
    return value;
}

[[nodiscard]] inline PieceOnSquare piece_on_square(char file, int rank, char piece) {
    return PieceOnSquare{
        .square = static_cast<std::uint8_t>(((rank - 1) * 8) + (file - 'a')),
        .piece = piece,
    };
}

[[nodiscard]] inline Motif motif(MotifTag tag, Side side, int ply, PieceOnSquare actor,
                                 std::vector<PieceOnSquare> targets = {},
                                 std::optional<int> gain = std::nullopt) {
    Motif value;
    value.tag = tag;
    value.side = side;
    value.ply = ply;
    value.actor = actor;
    value.targets = std::move(targets);
    value.gain = gain;
    return value;
}

[[nodiscard]] inline MoveAssessment move(std::string san, std::string uci,
                                         std::vector<std::string> line, int es_units,
                                         std::string root_fen, Side threat_side = Side::Black,
                                         Verdict verdict = Verdict::Fine, int loss_units = 0) {
    MoveAssessment value;
    value.san = std::move(san);
    value.uci = std::move(uci);
    value.eval.es_units = es_units;
    value.eval.cp = 12;
    value.loss_units = loss_units;
    value.verdict = verdict;
    value.in_multipv = true;
    value.line_san = std::move(line);
    value.creates_threat = threat(threat_side, std::move(root_fen), {"Nxe4"},
                                  consequence(ConsequenceClass::Positional, 0, 6));
    return value;
}

[[nodiscard]] inline Report base_report(const std::string& fen, Side side) {
    Report report;
    report.input.fen = fen;
    report.input.level = Level::Club;
    report.input.budget = Budget::Standard;
    report.engine.name = "Stockfish 19";
    report.engine.threads = 1;
    report.engine.hash_mb = 64;
    report.engine.multipv = 5;
    report.engine.reproducible = true;
    report.engine.nodes = {.a = 1000, .b = 1000, .c = 1000, .d = 1000, .e = 1000, .r = 1000};
    report.engine.nodes_searched = 5000;
    report.position.fen = fen;
    report.position.side_to_move = side;
    report.opening.dataset = kDataset;
    return report;
}

[[nodiscard]] inline Report terminal_checkmate() {
    Report report = base_report(kF13, Side::White);
    report.position.in_check = true;
    report.position.terminal = Terminal::Checkmate;
    report.position.eval_white.reset();
    report.engine.nodes_searched = 0;
    return report;
}

[[nodiscard]] inline Report side_in_check() {
    Report report = base_report(kF02, Side::Black);
    report.position.in_check = true;
    report.position.eval_white = Eval{.es_units = 1800, .cp = 500, .mate = std::nullopt};

    Threats threats;
    threats.white.reset();
    threats.black =
        threat(Side::Black, kF02, {"g6", "Qxg6"}, consequence(ConsequenceClass::Material, 3, 2));
    threats.null_move_reliable = std::nullopt;
    threats.urgency = {.more_urgent = Side::White, .rule = UrgencyRule::InCheck};
    report.threats = std::move(threats);

    MoveAssessment only_move;
    only_move.san = "g6";
    only_move.uci = "g7g6";
    only_move.eval = Eval{.es_units = 1800, .cp = 500, .mate = std::nullopt};
    only_move.loss_units = 0;
    only_move.verdict = Verdict::Fine;
    only_move.in_multipv = true;
    only_move.gives_check = false;
    only_move.line_san = {"g6", "Qxg6"};
    only_move.creates_threat =
        threat(Side::Black, "rnbqkbnr/ppppp2p/5pp1/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 0 2",
               {"Nf6"}, consequence(ConsequenceClass::Material, 3, 4));
    report.candidates.push_back(std::move(only_move));
    return report;
}

[[nodiscard]] inline Report normal_position() {
    Report report = base_report(kF03, Side::Black);
    report.input.move = "Nf6";
    report.position.eval_white = Eval{.es_units = 1234, .cp = 45, .mate = std::nullopt};
    report.position.motifs.push_back(
        motif(MotifTag::HangingPiece, Side::White, 0, piece_on_square('d', 5, 'n'), {}, 3));
    report.position.motifs.push_back(
        motif(MotifTag::BackRankWeakness, Side::White, 0, piece_on_square('e', 8, 'k')));

    Threats threats;
    threats.white =
        threat(Side::White, "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 3 3",
               {"Qxf7#"}, consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    threats.white->motifs.push_back(
        motif(MotifTag::Fork, Side::White, 3, piece_on_square('e', 5, 'N'),
              {piece_on_square('c', 6, 'r'), piece_on_square('g', 6, 'k')}));
    threats.black =
        threat(Side::Black, kF03, {"Nf6"}, consequence(ConsequenceClass::Positional, 0, 6));
    threats.black->motifs.push_back(motif(MotifTag::DiscoveredCheck, Side::Black, 1,
                                          piece_on_square('f', 6, 'q'),
                                          {piece_on_square('e', 1, 'K')}));
    threats.null_move_reliable = true;
    threats.urgency = {.more_urgent = Side::White, .rule = UrgencyRule::ConsequenceClass};
    report.threats = std::move(threats);

    report.candidates.push_back(move("d6", "d7d6", {"d6"}, 1234, kF03));
    report.candidates.back().motifs.push_back(
        motif(MotifTag::Promotion, Side::Black, 1, piece_on_square('e', 8, 'Q')));
    if (auto& creates_threat = report.candidates.back().creates_threat; creates_threat) {
        creates_threat->motifs.push_back(motif(MotifTag::DiscoveredAttack, Side::Black, 1,
                                               piece_on_square('c', 5, 'b'),
                                               {piece_on_square('h', 5, 'Q')}));
    }
    report.candidates.push_back(move("g6", "g7g6", {"g6"}, 1234, kF03));
    report.candidates.push_back(move("Qf6", "d8f6", {"Qf6"}, 1234, kF03));
    report.candidates.push_back(move("Qe7", "d8e7", {"Qe7"}, 1234, kF03));
    report.candidates.push_back(move("Bc5", "f8c5", {"Bc5"}, 1234, kF03));
    report.fine_truncated = true;

    MoveAssessment asked;
    asked.san = "Nf6";
    asked.uci = "g8f6";
    asked.eval = Eval{.es_units = 534, .cp = -230, .mate = std::nullopt};
    asked.loss_units = 700;
    asked.verdict = Verdict::Blunder;
    asked.in_multipv = false;
    asked.gives_check = false;
    asked.line_san = {"Nf6", "Qxf7#"};
    asked.motifs.push_back(motif(MotifTag::Skewer, Side::Black, 1, piece_on_square('f', 6, 'q'),
                                 {piece_on_square('f', 1, 'R')}));
    asked.parries_threat = ParryInfo{.parries = false, .test = ParryTest::Searched};
    asked.creates_threat =
        threat(Side::Black, "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNBQK1NR b KQkq - 4 4",
               {"Nxe4"}, consequence(ConsequenceClass::Positional, 0, 6));
    asked.refutation = threat(Side::White, kAfterNf6, {"Qxf7#"},
                              consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    asked.refutation->motifs.push_back(
        motif(MotifTag::HangingPiece, Side::White, 1, piece_on_square('d', 5, 'n'), {}, 3));
    report.asked_move = std::move(asked);
    return report;
}

[[nodiscard]] inline Report pgn_opening() {
    Report report = base_report(kAfterE4E5, Side::White);
    report.input.source = InputSource::Pgn;
    report.input.pgn_game = 1;
    report.input.pgn_ply = 2;
    report.position.eval_white = Eval{.es_units = 1234, .cp = 45, .mate = std::nullopt};

    Threats threats;
    threats.white =
        threat(Side::White, kAfterE4E5, {"Nf3"}, consequence(ConsequenceClass::Positional, 0, 6));
    threats.black =
        threat(Side::Black, "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2", {"Nc6"},
               consequence(ConsequenceClass::Positional, 0, 6));
    threats.null_move_reliable = true;
    threats.urgency = {.more_urgent = std::nullopt, .rule = UrgencyRule::Quiet};
    report.threats = std::move(threats);

    report.candidates.push_back(move("Nf3", "g1f3", {"Nf3", "Nc6"}, 1234, kAfterE4E5, Side::White));
    report.candidates.push_back(move("Nc3", "b1c3", {"Nc3", "Nc6"}, 1234, kAfterE4E5, Side::White));
    report.candidates.push_back(move("Bc4", "f1c4", {"Bc4", "Nc6"}, 1234, kAfterE4E5, Side::White));
    report.candidates.push_back(move("d4", "d2d4", {"d4", "exd4"}, 1234, kAfterE4E5, Side::White));
    report.candidates.push_back(move("f4", "f2f4", {"f4", "exf4"}, 1234, kAfterE4E5, Side::White));
    report.fine_truncated = true;

    report.opening.current = OpeningName{.eco = "C20", .name = "King's Pawn Game"};
    report.opening.last_named =
        NamedPly{.opening = OpeningName{.eco = "C20", .name = "King's Pawn Game"}, .ply = 2};
    return report;
}

[[nodiscard]] inline TacticLabel removal_label(Side side = Side::Black) {
    TacticLabel label;
    label.tag = TacticLabelTag::RemovalOfDefender;
    label.side = side;
    label.ply = 1;
    label.actor = piece_on_square('d', 5, 'b');
    label.targets = {piece_on_square('d', 5, 'N'), piece_on_square('e', 4, 'R')};
    return label;
}

[[nodiscard]] inline Report forced_judgement() {
    Report report = base_report(kF02, Side::Black);
    report.position.in_check = true;
    MoveAssessment only_move = move("g6", "g7g6", {"g6", "Qxg6"}, 1800, kF02);
    only_move.creates_threat.reset();
    TacticJudgement tactic;
    tactic.forced = true;
    only_move.tactic = std::move(tactic);
    report.candidates.push_back(std::move(only_move));
    return report;
}

[[nodiscard]] inline Report tactic_with_labels() {
    Report report = base_report(kF03, Side::Black);
    MoveAssessment best = move("Nf6", "g8f6", {"Nf6", "Qxf7#"}, 1234, kF03);
    best.creates_threat.reset();
    best.motifs.push_back(motif(MotifTag::Fork, Side::Black, 1, piece_on_square('f', 3, 'n'),
                                {piece_on_square('e', 1, 'K'), piece_on_square('d', 4, 'Q')}));
    best.motifs.back().contribution = MotifContribution::Direct;
    best.motifs.push_back(motif(MotifTag::PinAbsolute, Side::White, 0,
                                piece_on_square('e', 1, 'R'),
                                {piece_on_square('e', 5, 'n'), piece_on_square('e', 8, 'k')}));
    best.motifs.push_back(motif(MotifTag::PinRelative, Side::White, 0,
                                piece_on_square('b', 1, 'B'),
                                {piece_on_square('c', 2, 'n'), piece_on_square('e', 4, 'q')}));
    best.motifs.push_back(motif(MotifTag::OverloadedDefender, Side::Black, 1,
                                piece_on_square('d', 5, 'n'),
                                {piece_on_square('e', 4, 'R'), piece_on_square('c', 4, 'Q')}));
    best.motifs.push_back(motif(MotifTag::PinTactical, Side::Black, 1,
                                piece_on_square('d', 5, 'n'),
                                {piece_on_square('g', 3, 'P'), piece_on_square('e', 2, 'Q'),
                                 piece_on_square('g', 2, 'K')}));
    best.motifs.back().threat = MotifThreat{
        .kind = ThreatKind::Fork,
        .square = piece_on_square('f', 4, 'P').square,
    };
    best.motifs.back().contribution = MotifContribution::Direct;
    TacticJudgement tactic;
    tactic.alternative = TacticAlternative{
        .san = "g6",
        .uci = "g7g6",
        .eval = Eval{.es_units = 254, .cp = -230, .mate = std::nullopt},
    };
    tactic.margin_units = 980;
    tactic.is_tactic = true;
    tactic.labels.push_back(removal_label());
    best.tactic = std::move(tactic);

    report.position.motifs.push_back(motif(MotifTag::BackRankWeakness, Side::Black, 0,
                                           piece_on_square('e', 8, 'k')));
    report.position.motifs.back().contribution = MotifContribution::Indirect;
    report.candidates.push_back(std::move(best));
    MoveAssessment alternative = move("g6", "g7g6", {"g6", "Qxf7#"}, 254, kF03);
    alternative.creates_threat.reset();
    report.candidates.push_back(std::move(alternative));
    return report;
}

[[nodiscard]] inline Report tactic_with_asked_move() {
    Report report = tactic_with_labels();
    report.input.move = report.candidates.front().san;
    report.asked_move = report.candidates.front();
    return report;
}

[[nodiscard]] inline Report negative_margin_judgement() {
    Report report = base_report(kF03, Side::Black);
    MoveAssessment best = move("d6", "d7d6", {"d6"}, 1000, kF03);
    best.creates_threat.reset();
    TacticJudgement tactic;
    tactic.alternative = TacticAlternative{
        .san = "g6",
        .uci = "g7g6",
        .eval = Eval{.es_units = 1100, .cp = 10, .mate = std::nullopt},
    };
    tactic.margin_units = -100;
    best.tactic = std::move(tactic);
    report.candidates.push_back(std::move(best));
    MoveAssessment alternative = move("g6", "g7g6", {"g6"}, 1100, kF03);
    alternative.creates_threat.reset();
    report.candidates.push_back(std::move(alternative));
    return report;
}

[[nodiscard]] inline Report refutation_judgement() {
    Report report = base_report(kF03, Side::Black);
    MoveAssessment candidate =
        move("Nf6", "g8f6", {"Nf6", "Qxf7#"}, 534, kF03, Side::Black, Verdict::Blunder, 700);
    candidate.creates_threat.reset();
    candidate.refutation = threat(Side::White, kAfterNf6, {"Qxf7#"},
                                  consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    candidate.refutation->motifs.push_back(
        motif(MotifTag::Fork, Side::White, 1, piece_on_square('h', 5, 'Q'),
              {piece_on_square('f', 7, 'k'), piece_on_square('e', 8, 'k')}));
    candidate.refutation->motifs.back().contribution = MotifContribution::Direct;
    TacticJudgement tactic;
    tactic.alternative = TacticAlternative{
        .san = "Qe2",
        .uci = "h5e2",
        .eval = Eval{.es_units = 1066, .cp = 1000, .mate = std::nullopt},
    };
    tactic.margin_units = 400;
    tactic.is_tactic = true;
    tactic.labels.push_back(removal_label(Side::White));
    candidate.refutation->tactic = std::move(tactic);
    report.candidates.push_back(std::move(candidate));
    return report;
}

[[nodiscard]] inline std::array<Report, 8> all_reports() {
    return {terminal_checkmate(), side_in_check(), normal_position(), pgn_opening(),
            tactic_with_labels(), forced_judgement(), negative_margin_judgement(),
            refutation_judgement()};
}

} // namespace chesswhy::test_examples
