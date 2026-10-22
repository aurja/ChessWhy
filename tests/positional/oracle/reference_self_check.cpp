#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "reference.hpp"

namespace {

using chesswhy::PositionalChangeKind;
using chesswhy::PositionalFeature;
using chesswhy::PositionalTag;
using chesswhy::Side;
using chesswhy::positional_oracle::reference_features;
using chesswhy::positional_oracle::reference_line_changes;
using chesswhy::positional_oracle::reference_mobility;
using chesswhy::positional_oracle::same_identity;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

[[nodiscard]] std::vector<PositionalFeature> tagged(const chess::Board& board, PositionalTag tag,
                                                    Side side) {
    std::vector<PositionalFeature> result;
    for (const PositionalFeature& feature : reference_features(board)) {
        if (feature.tag == tag && feature.side == side) {
            result.push_back(feature);
        }
    }
    return result;
}

[[nodiscard]] chess::Move legal_move(chess::Board& board, std::string_view uci) {
    const chess::Move move = chess::uci::uciToMove(board, uci);
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board);
    if (move == chess::Move::NO_MOVE || std::ranges::find(legal_moves, move) == legal_moves.end()) {
        throw std::runtime_error("self-check move is illegal: " + std::string(uci));
    }
    return move;
}

void check_pawn_structure() {
    const chess::Board board("4k3/8/8/8/2PP4/P7/P5PP/4K3 w - - 0 1");
    const auto isolated = tagged(board, PositionalTag::IsolatedPawn, Side::White);
    require(isolated.size() == 1 && isolated.front().file == 0 &&
                isolated.front().pieces.size() == 2 && isolated.front().pieces[0].square == 8 &&
                isolated.front().pieces[1].square == 16,
            "isolated doubled pawns are grouped by file in ascending square order");
    require(tagged(board, PositionalTag::DoubledPawns, Side::White).size() == 1,
            "doubled pawns hold with two pawns on one file");
    const auto passers = tagged(board, PositionalTag::PassedPawn, Side::White);
    require(passers.size() == 5, "only the front pawn on each clear file is passed");
    const auto connected = tagged(board, PositionalTag::ConnectedPassedPawns, Side::White);
    require(connected.size() == 2 && connected[0].pieces.size() == 2 &&
                connected[1].pieces.size() == 2,
            "connected passed pawns group maximal consecutive-file runs");
    const auto islands = tagged(board, PositionalTag::PawnIslands, Side::White);
    require(islands.size() == 1 && islands.front().value == 3,
            "pawn islands count maximal consecutive-file runs");

    const chess::Board p05("5r2/1pr1ppkp/p2p2p1/3P4/P2q1P2/1P5P/2P1Q1PK/R4R2 w - - 5 24");
    const auto p05_white = tagged(p05, PositionalTag::PawnIslands, Side::White);
    const auto p05_black = tagged(p05, PositionalTag::PawnIslands, Side::Black);
    require(p05_white.size() == 1 && p05_white.front().value == 2 && p05_black.size() == 1 &&
                p05_black.front().value == 2,
            "P05 file runs give two pawn islands for each side");
    const chess::Board p08("r2qr1k1/1b1n1pp1/1pp2b1p/p2p4/1P1P4/PQNBPN2/5PPP/R4RK1 b - - 4 15");
    const auto p08_white = tagged(p08, PositionalTag::PawnIslands, Side::White);
    const auto p08_black = tagged(p08, PositionalTag::PawnIslands, Side::Black);
    require(p08_white.size() == 1 && p08_white.front().value == 2 && p08_black.size() == 1 &&
                p08_black.front().value == 2,
            "P08 file runs give two pawn islands for each side");

    const chess::Board backward("4k3/8/8/1p1P4/8/2P5/8/4K3 w - - 0 1");
    const auto white_backward = tagged(backward, PositionalTag::BackwardPawn, Side::White);
    require(white_backward.size() == 1 && white_backward.front().file == 2 &&
                white_backward.front().pieces.size() == 1 &&
                white_backward.front().pieces.front().square == 18,
            "white backward pawn requires the attacked stop square");
    const chess::Board black_backward("4k3/8/2p5/8/1P1p4/8/8/4K3 b - - 0 1");
    const auto black_features = tagged(black_backward, PositionalTag::BackwardPawn, Side::Black);
    require(black_features.size() == 1 && black_features.front().file == 2 &&
                black_features.front().pieces.size() == 1 &&
                black_features.front().pieces.front().square == 42,
            "black backward pawn uses relative ranks and the black forward direction");

    const chess::Board p13("7k/8/2p5/3pPP2/2pP2P1/2P5/8/4K3 w - - 0 1");
    const auto p13_white = tagged(p13, PositionalTag::BackwardPawn, Side::White);
    const auto p13_black = tagged(p13, PositionalTag::BackwardPawn, Side::Black);
    require(
        std::ranges::none_of(p13_white, [](const auto& feature) { return feature.file == 3; }) &&
            std::ranges::none_of(p13_black, [](const auto& feature) { return feature.file == 3; }),
        "P13 d4 and d5 are each supported by an adjacent same-or-lower pawn");

    const chess::Board chain("4k3/8/8/8/4P3/3P4/2P5/4K3 w - - 0 1");
    const auto chains = tagged(chain, PositionalTag::PawnChain, Side::White);
    require(chains.size() == 1 && chains.front().pieces.size() == 3 &&
                chains.front().pieces[0].square == 10 && chains.front().pieces[1].square == 19 &&
                chains.front().pieces[2].square == 28,
            "pawn chain needs three pawns and reports base to head");
}

void check_king_safety() {
    const chess::Board rights("r3k2r/8/8/4p3/8/8/8/R3K2R w KQkq - 0 1");
    require(tagged(rights, PositionalTag::CastlingKingside, Side::White).size() == 1 &&
                tagged(rights, PositionalTag::CastlingQueenside, Side::White).size() == 1 &&
                tagged(rights, PositionalTag::CastlingKingside, Side::Black).size() == 1 &&
                tagged(rights, PositionalTag::CastlingQueenside, Side::Black).size() == 1,
            "castling features report both rights for both sides");
    const auto white_shield = tagged(rights, PositionalTag::KingShieldGap, Side::White);
    const auto black_shield = tagged(rights, PositionalTag::KingShieldGap, Side::Black);
    require(white_shield.size() == 3 && white_shield[0].file == 3 && white_shield[1].file == 4 &&
                white_shield[2].file == 5 && black_shield.size() == 3 &&
                black_shield[0].file == 3 && black_shield[1].file == 4 && black_shield[2].file == 5,
            "king shield gaps cover every king file and inspect two relative-rank squares");
    const auto white_half_open = tagged(rights, PositionalTag::HalfOpenFileNearKing, Side::White);
    const auto white_open = tagged(rights, PositionalTag::OpenFileNearKing, Side::White);
    require(white_half_open.size() == 1 && white_half_open.front().file == 4 &&
                white_open.size() == 2 && white_open[0].file == 3 && white_open[1].file == 5,
            "each king file is independently open or half-open for the king");

    const chess::Board open_file("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    require(tagged(open_file, PositionalTag::OpenFileNearKing, Side::White).size() == 3 &&
                tagged(open_file, PositionalTag::OpenFileNearKing, Side::Black).size() == 3,
            "all three king files are open when no pawn occupies any of them");

    const chess::Board attacked("4k3/8/8/8/8/8/4q3/4K3 w - - 0 1");
    const auto attackers = tagged(attacked, PositionalTag::KingZoneAttacker, Side::White);
    require(attackers.size() == 1 && attackers.front().pieces.front().square == 12 &&
                attackers.front().pieces.front().piece == 'q',
            "king zone attackers report the attacking piece and square");
}

void check_pieces() {
    const chess::Board files("4k3/8/8/p7/8/2B5/8/RB2K2R w - - 0 1");
    require(tagged(files, PositionalTag::BishopPair, Side::White).size() == 1,
            "bishop pair contains bishops on both square colours");
    const auto open_rook = tagged(files, PositionalTag::RookOpenFile, Side::White);
    const auto half_open_rook = tagged(files, PositionalTag::RookHalfOpenFile, Side::White);
    require(open_rook.size() == 1 && open_rook.front().file == 7 && half_open_rook.size() == 1 &&
                half_open_rook.front().file == 0,
            "rook open and half-open files inspect both pawn colours");

    const chess::Board seventh("4k3/R7/8/8/8/8/8/4K3 w - - 0 1");
    const auto seventh_rook = tagged(seventh, PositionalTag::RookSeventh, Side::White);
    require(seventh_rook.size() == 1 && seventh_rook.front().pieces.front().square == 48,
            "rook on relative rank seven qualifies against the enemy back-rank king");

    const chess::Board outpost("4k3/8/8/4N3/3P4/8/8/4K3 w - - 0 1");
    require(tagged(outpost, PositionalTag::Outpost, Side::White).size() == 1,
            "pawn-defended minor piece on a safe relative-rank square is an outpost");

    const chess::Board bad_bishop("4k3/8/8/8/8/1p1p4/1P1P4/2B1K3 w - - 0 1");
    const auto bad = tagged(bad_bishop, PositionalTag::BadBishop, Side::White);
    require(bad.size() == 1 && bad.front().pieces.size() == 3 &&
                bad.front().pieces[0].square == 2 && bad.front().pieces[1].square == 9 &&
                bad.front().pieces[2].square == 11,
            "bad bishop reports bishops then all blocked majority-colour pawns");

    const chess::Board p41("2b4k/8/2p1p3/2P1P3/8/3p1p2/3P1P2/KB6 w - - 0 1");
    require(tagged(p41, PositionalTag::BadBishop, Side::White).empty() &&
                tagged(p41, PositionalTag::BadBishop, Side::Black).size() == 1,
            "P41 has a bad bishop only for Black under square-colour majority");
}

void check_counts_and_material() {
    const chess::Board mobility("4k3/8/8/8/8/8/1N3N2/4KQ2 w - - 0 1");
    const auto mobility_lead = tagged(mobility, PositionalTag::MobilityLead, Side::White);
    require(mobility_lead.size() == 1 && mobility_lead.front().value >= 10,
            "mobility lead uses a ten-square threshold");

    const chess::Board p43("8/8/8/8/8/8/8/NB2K1kR b - - 0 1");
    require(reference_mobility(p43, chess::Color::WHITE) == 17 &&
                reference_mobility(p43, chess::Color::BLACK) == 0 &&
                tagged(p43, PositionalTag::MobilityLead, Side::White).front().value == 17,
            "P43 counts seven empty h-file squares and the enemy king square, plus bishop and "
            "knight attacks");
    const chess::Board p44("nb2k1Kr/8/8/8/8/8/8/8 w - - 0 1");
    require(reference_mobility(p44, chess::Color::BLACK) == 17 &&
                reference_mobility(p44, chess::Color::WHITE) == 0 &&
                tagged(p44, PositionalTag::MobilityLead, Side::Black).front().value == 17,
            "P44 applies the same mobility count from Black's perspective");

    const chess::Board space("4k3/8/8/2PPPP2/8/8/8/4K3 w - - 0 1");
    const auto space_lead = tagged(space, PositionalTag::SpaceLead, Side::White);
    require(space_lead.size() == 1 && space_lead.front().value >= 3,
            "space lead uses a three-square threshold");

    const chess::Board development("1nb1kbn1/8/8/8/8/8/8/R3K2R w - - 0 1");
    const auto development_lead = tagged(development, PositionalTag::DevelopmentLead, Side::White);
    require(development_lead.size() == 1 && development_lead.front().value == 4,
            "development lead counts minor pieces on relative rank one");
    require(tagged(development, PositionalTag::ExchangeUp, Side::White).size() == 1,
            "exchange up requires a rook advantage and an enemy minor-piece advantage");
    require(tagged(mobility, PositionalTag::MaterialAhead, Side::White).size() == 1,
            "material ahead reports the positive piece-value difference");
}

void check_line_changes_and_identity() {
    chess::Board castling("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const chess::Move king_move = legal_move(castling, "e1e2");
    const std::array castling_line{king_move};
    const auto castle_changes = reference_line_changes(castling, castling_line);
    for (const PositionalTag tag :
         {PositionalTag::CastlingKingside, PositionalTag::CastlingQueenside}) {
        const auto found = std::ranges::find_if(castle_changes, [tag](const auto& change) {
            return change.feature.tag == tag && change.feature.side == Side::White;
        });
        require(found != castle_changes.end() && found->ply == 1 &&
                    found->change == PositionalChangeKind::Disappears && found->holds_at_end,
                "castling-right loss remains absent at the line end");
    }

    chess::Board open_file("4k3/8/8/8/8/1n6/P7/R3K3 w - - 0 1");
    const chess::Move capture = legal_move(open_file, "a2b3");
    const std::array capture_line{capture};
    const auto open_changes = reference_line_changes(open_file, capture_line);
    const auto rook_open = std::ranges::find_if(open_changes, [](const auto& change) {
        return change.feature.tag == PositionalTag::RookOpenFile &&
               change.feature.side == Side::White;
    });
    require(rook_open != open_changes.end() && rook_open->ply == 1 &&
                rook_open->change == PositionalChangeKind::Appears && rook_open->holds_at_end,
            "an opened rook file is an appearance that holds at the line end");

    chess::Board pl02("7k/8/8/8/8/8/8/R3K3 w - - 0 1");
    const chess::Move rook_move = legal_move(pl02, "a1b1");
    const std::array rook_line{rook_move};
    const auto rook_changes = reference_line_changes(pl02, rook_line);
    const auto rook_a_disappears = std::ranges::find_if(rook_changes, [](const auto& change) {
        return change.feature.tag == PositionalTag::RookOpenFile &&
               change.feature.side == Side::White && change.feature.file == 0 &&
               change.change == PositionalChangeKind::Disappears;
    });
    require(rook_a_disappears != rook_changes.end() && rook_a_disappears->holds_at_end,
            "PL02's disappeared a-file identity remains absent at the end");

    chess::Board pl03("k7/8/8/8/8/8/8/4K2R w K - 0 1");
    const chess::Move castle_move = legal_move(pl03, "e1g1");
    const std::array castle_line{castle_move};
    const auto pl03_changes = reference_line_changes(pl03, castle_line);
    const auto kingside_disappears = std::ranges::find_if(pl03_changes, [](const auto& change) {
        return change.feature.tag == PositionalTag::CastlingKingside &&
               change.feature.side == Side::White &&
               change.change == PositionalChangeKind::Disappears;
    });
    require(kingside_disappears != pl03_changes.end() && kingside_disappears->holds_at_end,
            "PL03's disappeared castling right remains absent at the end");

    chess::Board temporary_passer("4k3/8/2b5/1p6/P7/8/8/4K3 w - - 0 1");
    const chess::Move capture_pawn = legal_move(temporary_passer, "a4b5");
    chess::Board after_capture = temporary_passer;
    after_capture.makeMove(capture_pawn);
    const chess::Move bishop_takes = legal_move(after_capture, "c6b5");
    const std::array temporary_line{capture_pawn, bishop_takes};
    const auto temporary_changes = reference_line_changes(temporary_passer, temporary_line);
    const auto passer_appears = std::ranges::find_if(temporary_changes, [](const auto& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White && change.change == PositionalChangeKind::Appears;
    });
    const auto passer_disappears = std::ranges::find_if(temporary_changes, [](const auto& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White &&
               change.change == PositionalChangeKind::Disappears;
    });
    require(passer_appears != temporary_changes.end() && passer_appears->ply == 1 &&
                !passer_appears->holds_at_end && passer_disappears != temporary_changes.end() &&
                passer_disappears->ply == 2 && passer_disappears->holds_at_end,
            "a transient passer appears without holding and disappears by the end");

    chess::Board advancing("4k3/8/8/8/P7/8/8/4K3 w - - 0 1");
    const chess::Move advance = legal_move(advancing, "a4a5");
    const std::array advance_line{advance};
    const auto advance_changes = reference_line_changes(advancing, advance_line);
    require(std::ranges::none_of(
                advance_changes,
                [](const auto& change) { return change.feature.tag == PositionalTag::PassedPawn; }),
            "a passed pawn advancing along its file keeps its identity and causes no change");

    const PositionalFeature first{.tag = PositionalTag::PassedPawn,
                                  .side = Side::White,
                                  .pieces = {{.square = 24, .piece = 'P'}},
                                  .file = 0,
                                  .value = std::nullopt};
    const PositionalFeature advanced{.tag = PositionalTag::PassedPawn,
                                     .side = Side::White,
                                     .pieces = {{.square = 32, .piece = 'P'}},
                                     .file = 0,
                                     .value = std::nullopt};
    const PositionalFeature moved_lead{.tag = PositionalTag::MobilityLead,
                                       .side = Side::White,
                                       .pieces = {},
                                       .file = std::nullopt,
                                       .value = 10};
    PositionalFeature larger_lead = moved_lead;
    larger_lead.value = 15;
    const PositionalFeature material{.tag = PositionalTag::MaterialAhead,
                                     .side = Side::White,
                                     .pieces = {},
                                     .file = std::nullopt,
                                     .value = 5};
    PositionalFeature changed_material = material;
    changed_material.value = 6;
    require(same_identity(first, advanced) && same_identity(moved_lead, larger_lead) &&
                !same_identity(material, changed_material),
            "identity follows file for a passer, tag/side for a lead, and value for material");

    const PositionalFeature connected_before{
        .tag = PositionalTag::ConnectedPassedPawns,
        .side = Side::White,
        .pieces = {{.square = 17, .piece = 'P'}, {.square = 18, .piece = 'P'}},
        .file = std::nullopt,
        .value = std::nullopt};
    const PositionalFeature connected_after{
        .tag = PositionalTag::ConnectedPassedPawns,
        .side = Side::White,
        .pieces = {{.square = 25, .piece = 'P'}, {.square = 26, .piece = 'P'}},
        .file = std::nullopt,
        .value = std::nullopt};
    const PositionalFeature chain_before{.tag = PositionalTag::PawnChain,
                                         .side = Side::White,
                                         .pieces = {{.square = 8, .piece = 'P'},
                                                    {.square = 17, .piece = 'P'},
                                                    {.square = 26, .piece = 'P'}},
                                         .file = std::nullopt,
                                         .value = std::nullopt};
    const PositionalFeature chain_after{.tag = PositionalTag::PawnChain,
                                        .side = Side::White,
                                        .pieces = {{.square = 8, .piece = 'P'},
                                                   {.square = 17, .piece = 'P'},
                                                   {.square = 25, .piece = 'P'}},
                                        .file = std::nullopt,
                                        .value = std::nullopt};
    const PositionalFeature bad_bishop_before{.tag = PositionalTag::BadBishop,
                                              .side = Side::White,
                                              .pieces = {{.square = 2, .piece = 'B'},
                                                         {.square = 9, .piece = 'P'},
                                                         {.square = 11, .piece = 'P'}},
                                              .file = std::nullopt,
                                              .value = std::nullopt};
    const PositionalFeature bad_bishop_after{.tag = PositionalTag::BadBishop,
                                             .side = Side::White,
                                             .pieces = {{.square = 20, .piece = 'B'},
                                                        {.square = 25, .piece = 'P'},
                                                        {.square = 27, .piece = 'P'}},
                                             .file = std::nullopt,
                                             .value = std::nullopt};
    const PositionalFeature knight_outpost{.tag = PositionalTag::Outpost,
                                           .side = Side::White,
                                           .pieces = {{.square = 36, .piece = 'N'}},
                                           .file = std::nullopt,
                                           .value = std::nullopt};
    const PositionalFeature bishop_at_same_square{.tag = PositionalTag::Outpost,
                                                  .side = Side::White,
                                                  .pieces = {{.square = 36, .piece = 'B'}},
                                                  .file = std::nullopt,
                                                  .value = std::nullopt};
    require(same_identity(connected_before, connected_after) &&
                !same_identity(chain_before, chain_after) &&
                same_identity(bad_bishop_before, bad_bishop_after) &&
                !same_identity(knight_outpost, bishop_at_same_square),
            "run-file, pawn-square, bishop-colour and outpost-piece identities are distinct");
}

} // namespace

int main() {
    try {
        check_pawn_structure();
        std::cout << "PASS pawn structure, backward pawns and chains\n";
        check_king_safety();
        std::cout << "PASS king safety and castling rights\n";
        check_pieces();
        std::cout << "PASS pieces, files, outposts and bad bishops\n";
        check_counts_and_material();
        std::cout << "PASS mobility, space, development and material\n";
        check_line_changes_and_identity();
        std::cout << "PASS line changes and identity semantics\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "positional_reference_self_check: " << error.what() << '\n';
        return 1;
    }
}
