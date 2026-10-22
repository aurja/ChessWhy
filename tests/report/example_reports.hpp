#pragma once

#include "chesswhy/games.hpp"
#include "chesswhy/openings.hpp"
#include "chesswhy/report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <chess.hpp>

namespace chesswhy::test_examples {

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
    value.depth = 12;
    value.in_multipv = true;
    value.line_san = std::move(line);
    value.walked_plies = static_cast<int>(value.line_san.size());
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
    const chess::Board board(fen);
    report.position.in_check = board.inCheck();
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board);
    report.position.legal_moves = static_cast<int>(legal_moves.size());
    report.opening.dataset = openings::kDataset;
    GameSourceCounts game_source;
    game_source.id = "lichess_broadcast";
    game_source.table = games::kTable;
    game_source.platform = "over the board (may include online events relayed as broadcasts)";
    game_source.players = "both rated 2200 or more";
    game_source.months = "2020-01..2026-09";
    game_source.credit = "Lichess broadcast database, database.lichess.org, CC BY-SA 4.0";
    game_source.max_ply = 40;
    game_source.min_games = 5;
    report.games.by_source.push_back(std::move(game_source));
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
    only_move.depth = 12;
    only_move.loss_units = 0;
    only_move.verdict = Verdict::Fine;
    only_move.in_multipv = true;
    only_move.gives_check = false;
    only_move.line_san = {"g6", "Qxg6"};
    only_move.walked_plies = static_cast<int>(only_move.line_san.size());
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

    report.position.positional.push_back(PositionalFeature{.tag = PositionalTag::BishopPair,
                                                           .side = Side::White,
                                                           .pieces = {},
                                                           .file = std::nullopt,
                                                           .value = std::nullopt});

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

    GameSourceCounts& game_source = report.games.by_source.front();
    game_source.found = true;
    game_source.games = 1840;
    game_source.white = 640;
    game_source.draws = 790;
    game_source.black = 410;
    game_source.moves.push_back(GameMoveCount{.san = "Nf6", .uci = "g8f6", .games = 1140});

    report.candidates.push_back(move("d6", "d7d6", {"d6"}, 1234, kF03));
    report.candidates.back().positional.push_back(
        PositionalChange{.ply = 1,
                         .change = PositionalChangeKind::Appears,
                         .holds_at_end = true,
                         .feature = PositionalFeature{.tag = PositionalTag::MobilityLead,
                                                      .side = Side::White,
                                                      .pieces = {},
                                                      .file = std::nullopt,
                                                      .value = 12}});
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
    asked.depth = 12;
    asked.loss_units = 700;
    asked.verdict = Verdict::Blunder;
    asked.in_multipv = false;
    asked.gives_check = false;
    asked.line_san = {"Nf6", "Qxf7#"};
    asked.walked_plies = static_cast<int>(asked.line_san.size());
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
    asked.positional.push_back(
        PositionalChange{.ply = 1,
                         .change = PositionalChangeKind::Disappears,
                         .holds_at_end = false,
                         .feature = PositionalFeature{.tag = PositionalTag::CastlingKingside,
                                                      .side = Side::White,
                                                      .pieces = {},
                                                      .file = std::nullopt,
                                                      .value = std::nullopt}});
    asked.positional.push_back(
        PositionalChange{.ply = 2,
                         .change = PositionalChangeKind::Appears,
                         .holds_at_end = true,
                         .feature = PositionalFeature{
                             .tag = PositionalTag::DoubledPawns,
                             .side = Side::Black,
                             .pieces = {piece_on_square('c', 6, 'p'), piece_on_square('c', 7, 'p')},
                             .file = 2,
                             .value = std::nullopt}});
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
    TacticJudgement tactic;
    tactic.forced = true;
    only_move.tactic = std::move(tactic);
    report.candidates.push_back(std::move(only_move));
    return report;
}

[[nodiscard]] inline Report tactic_with_labels() {
    Report report = base_report(kF03, Side::Black);
    MoveAssessment best = move("Nf6", "g8f6", {"Nf6", "Qxf7#"}, 1234, kF03);
    best.motifs.push_back(motif(MotifTag::Fork, Side::Black, 1, piece_on_square('f', 3, 'n'),
                                {piece_on_square('e', 1, 'K'), piece_on_square('d', 4, 'Q')}));
    best.motifs.back().contribution = MotifContribution::Direct;
    best.motifs.push_back(motif(MotifTag::PinAbsolute, Side::White, 0, piece_on_square('e', 1, 'R'),
                                {piece_on_square('e', 5, 'n'), piece_on_square('e', 8, 'k')}));
    best.motifs.back().restriction = MotifRestriction{
        .piece = piece_on_square('e', 5, 'n'),
        .restricted = {"e5c6", "e5d7"},
        .free = {},
        .held_to = HeldTo{.kind = HeldTo::Kind::Square, .name = "e5"},
    };
    best.motifs.push_back(motif(MotifTag::PinRelative, Side::White, 0, piece_on_square('b', 1, 'B'),
                                {piece_on_square('c', 2, 'n'), piece_on_square('e', 4, 'q')}));
    best.motifs.back().restriction = MotifRestriction{
        .piece = piece_on_square('c', 2, 'n'),
        .restricted = {"c2b4"},
        .free = {},
        .held_to = HeldTo{.kind = HeldTo::Kind::Square, .name = "c2"},
    };
    best.motifs.push_back(motif(MotifTag::OverloadedDefender, Side::Black, 1,
                                piece_on_square('d', 5, 'n'),
                                {piece_on_square('e', 4, 'R'), piece_on_square('c', 4, 'Q')}));
    best.motifs.back().restriction = MotifRestriction{
        .piece = piece_on_square('d', 5, 'n'),
        .restricted = {"d5e7"},
        .free = {},
        .held_to = HeldTo{.kind = HeldTo::Kind::Square, .name = "d5"},
    };
    best.motifs.push_back(motif(MotifTag::PinTactical, Side::Black, 1, piece_on_square('d', 5, 'n'),
                                {piece_on_square('g', 3, 'P'), piece_on_square('e', 2, 'Q'),
                                 piece_on_square('g', 2, 'K')}));
    best.motifs.back().restriction = MotifRestriction{
        .piece = piece_on_square('g', 3, 'P'),
        .restricted = {"g3g4"},
        .free = {},
        .held_to = HeldTo{.kind = HeldTo::Kind::Square, .name = "g3"},
    };
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

    report.position.motifs.push_back(
        motif(MotifTag::BackRankWeakness, Side::Black, 0, piece_on_square('e', 8, 'k')));
    report.position.motifs.back().contribution = MotifContribution::Indirect;
    report.candidates.push_back(std::move(best));
    MoveAssessment alternative = move("g6", "g7g6", {"g6", "Qxf7#"}, 254, kF03);
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
    report.candidates.push_back(std::move(alternative));
    return report;
}

[[nodiscard]] inline Report refutation_judgement() {
    Report report = base_report(kF03, Side::Black);
    MoveAssessment candidate =
        move("Nf6", "g8f6", {"Nf6", "Qxf7#"}, 534, kF03, Side::Black, Verdict::Blunder, 700);
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

[[nodiscard]] inline Report schema_base_report(const char* fen, Side side, int white_es_units) {
    Report report = base_report(fen, side);
    report.position.eval_white = Eval{.es_units = white_es_units, .cp = 0, .mate = std::nullopt};
    report.engine.nodes = {
        .a = 1'000'000, .b = 200'000, .c = 200'000, .d = 100'000, .e = 100'000, .r = 100'000};

    const chess::Board board(fen);
    Threats threats;
    if (board.inCheck()) {
        threats.null_move_reliable.reset();
        threats.urgency = {.more_urgent = side == Side::White ? Side::Black : Side::White,
                           .rule = UrgencyRule::InCheck};
    } else {
        const auto to_move = board.sideToMove();
        const auto non_pawn_non_king =
            board.us(to_move) ^
            (board.pieces(chess::PieceType::PAWN, chess::PieceType::KING) & board.us(to_move));
        threats.null_move_reliable = non_pawn_non_king.count() > 1;
        threats.urgency = {.more_urgent = std::nullopt, .rule = UrgencyRule::Quiet};
    }
    report.threats = std::move(threats);
    return report;
}

[[nodiscard]] inline MoveAssessment schema_move(std::string san, std::string uci, int es_units,
                                                int depth, std::vector<std::string> line,
                                                int walked, bool gives_check = false,
                                                int loss_units = 0,
                                                Verdict verdict = Verdict::Fine) {
    MoveAssessment result;
    result.san = std::move(san);
    result.uci = std::move(uci);
    result.eval = Eval{.es_units = es_units, .cp = 0, .mate = std::nullopt};
    result.depth = depth;
    result.loss_units = loss_units;
    result.verdict = verdict;
    result.in_multipv = true;
    result.gives_check = gives_check;
    result.line_san = std::move(line);
    result.walked_plies = walked;
    return result;
}

inline void add_schema_root_judgement(Report& report) {
    if (report.candidates.empty()) {
        return;
    }
    const chess::Board root(report.position.fen);
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, root);
    MoveAssessment& best = report.candidates.front();
    if (legal_moves.size() == 1) {
        TacticJudgement judgement;
        judgement.forced = true;
        best.tactic = std::move(judgement);
        return;
    }

    std::ranges::sort(legal_moves, [](chess::Move lhs, chess::Move rhs) {
        return chess::uci::moveToUci(lhs) < chess::uci::moveToUci(rhs);
    });
    const chess::Move best_move = chess::uci::uciToMove(root, best.uci);
    const auto alternative_move = std::ranges::find_if(
        legal_moves, [best_move](chess::Move move) { return move != best_move; });
    if (alternative_move == legal_moves.end()) {
        return;
    }

    const std::string alternative_uci = chess::uci::moveToUci(*alternative_move);
    const std::string alternative_san = chess::uci::moveToSan(root, *alternative_move);
    TacticJudgement judgement;
    judgement.alternative = TacticAlternative{
        .san = alternative_san,
        .uci = alternative_uci,
        .eval = Eval{.es_units = best.eval.es_units, .cp = 0, .mate = std::nullopt},
    };
    judgement.margin_units = 0;
    best.tactic = std::move(judgement);

    MoveAssessment alternative = schema_move(alternative_san, alternative_uci, best.eval.es_units,
                                             best.depth, {alternative_san}, 1);
    chess::Board after = root;
    after.makeMove(*alternative_move);
    if (after.inCheck()) {
        alternative.gives_check = true;
        alternative.creates_threat_skipped = CreatesThreatSkip::GivesCheck;
    } else {
        chess::Movelist replies;
        chess::movegen::legalmoves(replies, after);
        if (replies.empty()) {
            alternative.creates_threat_skipped = CreatesThreatSkip::GameOver;
        } else {
            chess::Board after_null = after;
            after_null.makeNullMove();
            chess::Movelist null_replies;
            chess::movegen::legalmoves(null_replies, after_null);
            if (null_replies.empty()) {
                alternative.creates_threat_skipped = CreatesThreatSkip::NoLegalMove;
            } else {
                const Side threat_side =
                    report.position.side_to_move == Side::White ? Side::Black : Side::White;
                alternative.creates_threat = threat(
                    threat_side, after.getFen(), {chess::uci::moveToSan(after, replies.front())},
                    consequence(ConsequenceClass::Positional, 0, 1));
            }
        }
    }
    report.candidates.insert(report.candidates.begin() + 1, std::move(alternative));
}

inline void add_schema_refutation_judgements(Report& report) {
    for (MoveAssessment& candidate : report.candidates) {
        if (!candidate.refutation || candidate.refutation->line_san.empty()) {
            continue;
        }
        Threat& refutation = *candidate.refutation;
        const chess::Board root(refutation.root_fen);
        chess::Movelist legal_moves;
        chess::movegen::legalmoves(legal_moves, root);
        TacticJudgement judgement;
        if (legal_moves.size() == 1) {
            judgement.forced = true;
            refutation.tactic = std::move(judgement);
            continue;
        }
        const chess::Move refutation_move = chess::uci::parseSan(root, refutation.line_san.front());
        const auto alternative_move = std::ranges::find_if(
            legal_moves, [refutation_move](chess::Move move) { return move != refutation_move; });
        if (alternative_move == legal_moves.end()) {
            continue;
        }
        const int alternative_es_units = 2000 - candidate.eval.es_units;
        judgement.alternative = TacticAlternative{
            .san = chess::uci::moveToSan(root, *alternative_move),
            .uci = chess::uci::moveToUci(*alternative_move),
            .eval = Eval{.es_units = alternative_es_units, .cp = 0, .mate = std::nullopt},
        };
        judgement.margin_units = 0;
        refutation.tactic = std::move(judgement);
    }
}

[[nodiscard]] inline Report schema_example() {
    constexpr const char* fen = "4k3/4r3/8/8/8/8/8/4R1K1 b - - 0 1";
    Report report = schema_base_report(fen, Side::Black, 1000);
    Motif pin = motif(MotifTag::PinAbsolute, Side::White, 0, piece_on_square('e', 1, 'R'),
                      {piece_on_square('e', 7, 'r'), piece_on_square('e', 8, 'k')});
    pin.restriction = MotifRestriction{
        .piece = piece_on_square('e', 7, 'r'),
        .restricted = {"e7a7", "e7b7", "e7c7", "e7d7", "e7f7", "e7g7", "e7h7"},
        .free = {"e7e1", "e7e2", "e7e3", "e7e4", "e7e5", "e7e6"},
        .held_to = HeldTo{.kind = HeldTo::Kind::File, .name = "e"},
    };
    report.position.motifs.push_back(std::move(pin));

    MoveAssessment candidate = schema_move("Rxe1+", "e7e1", 1000, 8, {"Rxe1+"}, 1, true);
    candidate.creates_threat_skipped = CreatesThreatSkip::GivesCheck;
    report.candidates.push_back(std::move(candidate));
    report.fine_truncated = true;
    add_schema_root_judgement(report);
    return report;
}

[[nodiscard]] inline Report schema_quiet_example() {
    constexpr const char* fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Report report = schema_base_report(fen, Side::White, 1100);
    MoveAssessment candidate = schema_move("e4", "e2e4", 1100, 9, {"e4", "e5"}, 2);
    candidate.creates_threat =
        threat(Side::White, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", {"Nf3"},
               consequence(ConsequenceClass::Positional, 0, 1));
    report.candidates.push_back(std::move(candidate));
    report.fine_truncated = true;
    add_schema_root_judgement(report);
    return report;
}

[[nodiscard]] inline Report schema_stalemate_example() {
    constexpr const char* fen = "k7/8/2K5/1Q6/8/8/8/8 w - - 0 1";
    Report report = schema_base_report(fen, Side::White, 1000);
    MoveAssessment candidate = schema_move("Qb6", "b5b6", 1000, 8, {"Qb6"}, 1);
    candidate.creates_threat_skipped = CreatesThreatSkip::GameOver;
    report.candidates.push_back(std::move(candidate));
    report.fine_truncated = true;
    add_schema_root_judgement(report);
    return report;
}

[[nodiscard]] inline Report schema_refutation_example() {
    Report report = schema_base_report(kF03, Side::Black, 766);
    MoveAssessment best = schema_move("d6", "d7d6", 1234, 17, {"d6"}, 1);
    best.creates_threat =
        threat(Side::Black, "r1bqkbnr/ppp2ppp/2np4/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 1 4",
               {"Nge7"}, consequence(ConsequenceClass::Positional, 0, 1));
    report.candidates.push_back(std::move(best));

    MoveAssessment blunder =
        schema_move("Nf6", "g8f6", 534, 16, {"Nf6", "Qxf7#"}, 2, false, 700, Verdict::Blunder);
    blunder.creates_threat =
        threat(Side::Black, "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNBQK1NR b KQkq - 5 4",
               {"Rg8"}, consequence(ConsequenceClass::Positional, 0, 1));
    blunder.refutation = threat(Side::White, kAfterNf6, {"Qxf7#"},
                                consequence(ConsequenceClass::Mate, 0, 1, false, 1));
    report.candidates.push_back(std::move(blunder));
    add_schema_root_judgement(report);
    add_schema_refutation_judgements(report);
    report.fine_truncated = false;
    return report;
}

[[nodiscard]] inline Report schema_asked_example() {
    Report report = schema_refutation_example();
    report.input.move = "d6";
    report.asked_move = report.candidates.front();
    return report;
}

[[nodiscard]] inline Report schema_check_example() {
    Report report = schema_base_report(kF02, Side::Black, 200);
    MoveAssessment candidate = schema_move("g6", "g7g6", 1800, 12, {"g6", "Qxg6+"}, 2);
    candidate.creates_threat =
        threat(Side::Black, "rnbqkbnr/ppppp2p/5pp1/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 3",
               {"Nc6"}, consequence(ConsequenceClass::Positional, 0, 1));
    report.candidates.push_back(std::move(candidate));
    add_schema_root_judgement(report);
    report.fine_truncated = true;
    return report;
}

[[nodiscard]] inline Report schema_pgn_example() {
    Report report = schema_base_report(kAfterE4E5, Side::White, 1200);
    report.input.source = InputSource::Pgn;
    report.input.pgn_game = 1;
    report.input.pgn_ply = 2;
    report.opening.current = OpeningName{.eco = "C20", .name = "King's Pawn Game"};
    report.opening.last_named = NamedPly{.opening = *report.opening.current, .ply = 2};

    MoveAssessment candidate = schema_move("Nf3", "g1f3", 1200, 14, {"Nf3", "Nc6"}, 2);
    candidate.creates_threat =
        threat(Side::White, "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
               {"Nxe5"}, consequence(ConsequenceClass::Positional, 0, 1));
    report.candidates.push_back(std::move(candidate));
    add_schema_root_judgement(report);
    report.fine_truncated = true;
    return report;
}

[[nodiscard]] inline std::array<Report, 8> schema_reports() {
    Report terminal = terminal_checkmate();
    terminal.fine_truncated = true;
    return {std::move(terminal),    schema_check_example(),     schema_example(),
            schema_quiet_example(), schema_stalemate_example(), schema_refutation_example(),
            schema_asked_example(), schema_pgn_example()};
}

[[nodiscard]] inline std::array<Report, 8> all_reports() {
    return {terminal_checkmate(),        side_in_check(),
            normal_position(),           pgn_opening(),
            tactic_with_labels(),        forced_judgement(),
            negative_margin_judgement(), refutation_judgement()};
}

} // namespace chesswhy::test_examples
