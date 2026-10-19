#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace {

using chesswhy::Motif;
using chesswhy::MotifTag;
using chesswhy::PieceOnSquare;
using chesswhy::Side;
using nlohmann::json;

[[nodiscard]] chess::Square square(std::string_view name) {
    return {name};
}

[[nodiscard]] std::vector<chess::Move>
parse_line(chess::Board& board, std::initializer_list<std::string_view> uci_moves) {
    std::vector<chess::Move> line;
    line.reserve(uci_moves.size());
    for (const std::string_view uci : uci_moves) {
        REQUIRE((uci.size() == 4 || uci.size() == 5));
        const chess::Move move = chess::uci::uciToMove(board, uci);
        REQUIRE(move != chess::Move::NO_MOVE);
        REQUIRE(board.isLegal(move));
        line.push_back(move);
        board.makeMove(move);
    }
    return line;
}

[[nodiscard]] const Motif* find_motif(const std::vector<Motif>& motifs, MotifTag tag, Side side,
                                      std::string_view actor) {
    const auto actor_square = static_cast<std::uint8_t>(square(actor).index());
    const auto found = std::ranges::find_if(motifs, [=](const Motif& motif) {
        return motif.tag == tag && motif.side == side && motif.actor.square == actor_square;
    });
    return found == motifs.end() ? nullptr : &*found;
}

void expect_motif(const std::vector<Motif>& motifs, MotifTag tag, Side side,
                  std::string_view actor_square, char actor_piece,
                  std::initializer_list<std::pair<std::string_view, char>> targets = {}) {
    const Motif* motif = find_motif(motifs, tag, side, actor_square);
    REQUIRE(motif != nullptr);
    const PieceOnSquare expected_actor{
        .square = static_cast<std::uint8_t>(square(actor_square).index()), .piece = actor_piece};
    CHECK(motif->actor == expected_actor);
    std::vector<PieceOnSquare> expected_targets;
    expected_targets.reserve(targets.size());
    for (const auto& [target_square, target_piece] : targets) {
        expected_targets.push_back(
            PieceOnSquare{.square = static_cast<std::uint8_t>(square(target_square).index()),
                          .piece = target_piece});
    }
    CHECK(motif->targets == expected_targets);
}

[[nodiscard]] std::vector<Motif>
line_motifs_from(std::string_view fen, std::initializer_list<std::string_view> uci_moves) {
    chess::Board board(fen);
    const std::vector<chess::Move> line = parse_line(board, uci_moves);
    const chess::Board root(fen);
    return chesswhy::tactics::line_motifs(root, line);
}

[[nodiscard]] bool square_name_is_valid(const json& value) {
    if (!value.is_string()) {
        return false;
    }
    const std::string name = value.get<std::string>();
    return name.size() == 2 && name[0] >= 'a' && name[0] <= 'h' && name[1] >= '1' && name[1] <= '8';
}

[[nodiscard]] std::string square_name(std::uint8_t index) {
    return static_cast<std::string>(chess::Square(index));
}
[[nodiscard]] bool valid_fixture_tag(const json& value) {
    if (!value.is_string()) {
        return false;
    }
    constexpr std::array tags{
        MotifTag::HangingPiece,
        MotifTag::PinAbsolute,
        MotifTag::PinRelative,
        MotifTag::BackRankWeakness,
        MotifTag::OverloadedDefender,
        MotifTag::TrappedPiece,
        MotifTag::Fork,
        MotifTag::Skewer,
        MotifTag::DiscoveredCheck,
        MotifTag::DiscoveredAttack,
        MotifTag::DoubleCheck,
        MotifTag::BackRankMate,
        MotifTag::RemovalOfDefender,
        MotifTag::Promotion,
    };
    const std::string tag = value.get<std::string>();
    return std::ranges::any_of(
        tags, [tag](MotifTag candidate) { return chesswhy::to_string(candidate) == tag; });
}

[[nodiscard]] bool valid_fixture_instance_field(std::string_view field) {
    return field == "tag" || field == "side" || field == "ply" || field == "actor" ||
           field == "targets";
}

[[nodiscard]] bool fixture_instance_matches(const Motif& motif, const json& expected,
                                            bool require_all_fields) {
    if (require_all_fields) {
        if (!expected.contains("tag") || !expected.contains("side") || !expected.contains("ply") ||
            !expected.contains("actor") || !expected.contains("targets")) {
            return false;
        }
    }
    if (expected.contains("tag") &&
        (!expected["tag"].is_string() ||
         expected["tag"].get<std::string>() != chesswhy::to_string(motif.tag))) {
        return false;
    }
    if (expected.contains("side") &&
        (!expected["side"].is_string() ||
         expected["side"].get<std::string>() != chesswhy::to_string(motif.side))) {
        return false;
    }
    if (expected.contains("ply") &&
        (!expected["ply"].is_number_integer() || expected["ply"].get<int>() != motif.ply)) {
        return false;
    }
    if (expected.contains("actor") &&
        (!square_name_is_valid(expected["actor"]) ||
         expected["actor"].get<std::string>() != square_name(motif.actor.square))) {
        return false;
    }
    if (expected.contains("targets")) {
        if (!expected["targets"].is_array() || expected["targets"].size() != motif.targets.size()) {
            return false;
        }
        for (std::size_t index = 0; index < motif.targets.size(); ++index) {
            if (!square_name_is_valid(expected["targets"][index]) ||
                expected["targets"][index].get<std::string>() !=
                    square_name(motif.targets[index].square)) {
                return false;
            }
        }
    }
    return true;
}

void validate_fixture_instance(const json& expected, bool include) {
    REQUIRE(expected.is_object());
    for (auto iterator = expected.begin(); iterator != expected.end(); ++iterator) {
        REQUIRE(valid_fixture_instance_field(iterator.key()));
    }
    if (include) {
        REQUIRE(expected.contains("tag"));
        REQUIRE(valid_fixture_tag(expected["tag"]));
        REQUIRE(expected.contains("side"));
        REQUIRE(expected["side"].is_string());
        REQUIRE((expected["side"] == "white" || expected["side"] == "black"));
        REQUIRE(expected.contains("ply"));
        REQUIRE(expected["ply"].is_number_integer());
        REQUIRE(expected["ply"].get<int>() >= 0);
        REQUIRE(expected.contains("actor"));
        REQUIRE(square_name_is_valid(expected["actor"]));
        REQUIRE(expected.contains("targets"));
        REQUIRE(expected["targets"].is_array());
        for (const auto& target : expected["targets"]) {
            REQUIRE(square_name_is_valid(target));
        }
    } else {
        if (expected.contains("tag")) {
            REQUIRE(valid_fixture_tag(expected["tag"]));
        }
        if (expected.contains("side")) {
            REQUIRE(expected["side"].is_string());
            REQUIRE((expected["side"] == "white" || expected["side"] == "black"));
        }
        if (expected.contains("ply")) {
            REQUIRE(expected["ply"].is_number_integer());
            REQUIRE(expected["ply"].get<int>() >= 0);
        }
        if (expected.contains("actor")) {
            REQUIRE(square_name_is_valid(expected["actor"]));
        }
        if (expected.contains("targets")) {
            REQUIRE(expected["targets"].is_array());
            for (const auto& target : expected["targets"]) {
                REQUIRE(square_name_is_valid(target));
            }
        }
    }
}

} // namespace

TEST_CASE("piece values and static exchange evaluation cover x-rays and pins", "[unit]") {
    using chesswhy::tactics::piece_value;
    using chesswhy::tactics::see;

    CHECK(piece_value(chess::PieceType::PAWN) == 1);
    CHECK(piece_value(chess::PieceType::KNIGHT) == 3);
    CHECK(piece_value(chess::PieceType::BISHOP) == 3);
    CHECK(piece_value(chess::PieceType::ROOK) == 5);
    CHECK(piece_value(chess::PieceType::QUEEN) == 9);
    CHECK(piece_value(chess::PieceType::KING) == 0);

    SECTION("x-ray recapture is revealed after the first capture") {
        const chess::Board board("7k/8/8/5p2/4q3/3P4/8/K3R3 w - - 0 1");
        CHECK(see(board, square("e4"), chess::Color::WHITE) == 9);
    }
    SECTION("a pinned defender cannot leave its pin ray") {
        const chess::Board board("4k3/8/4p3/3n4/2P5/8/8/K3R3 w - - 0 1");
        CHECK(see(board, square("d5"), chess::Color::WHITE) == 3);
    }
}

TEST_CASE("SEE permits king recapture only onto a safe square", "[unit]") {
    using chesswhy::tactics::see;
    const chess::Board defended("8/4k3/3p4/8/2N5/8/8/K2R4 w - - 0 1");
    const chess::Board undefended("8/4k3/3p4/8/2N5/8/8/K7 w - - 0 1");
    CHECK(see(defended, square("d6"), chess::Color::WHITE) == 1);
    CHECK(see(undefended, square("d6"), chess::Color::WHITE) == 0);
}

TEST_CASE("SEE does not treat en passant as a capture on the pawn square", "[unit]") {
    using chesswhy::tactics::see;
    const chess::Board board("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    CHECK(see(board, square("d5"), chess::Color::WHITE) == 0);
    CHECK(see(board, square("d6"), chess::Color::WHITE) == 0);
}

TEST_CASE("state motifs include hanging pieces and pin kinds", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto hanging = position_motifs(chess::Board("7k/8/8/4r3/8/2B5/8/K7 w - - 0 1"));
    expect_motif(hanging, MotifTag::HangingPiece, Side::White, "e5", 'r');
    const Motif* hanging_piece = find_motif(hanging, MotifTag::HangingPiece, Side::White, "e5");
    REQUIRE(hanging_piece != nullptr);
    CHECK(hanging_piece->gain == 5);

    const chess::Board low_gain("7k/8/5p2/4n3/3P4/8/8/K7 w - - 0 1");
    CHECK(chesswhy::tactics::see(low_gain, square("e5"), chess::Color::WHITE) == 2);
    CHECK(find_motif(position_motifs(low_gain), MotifTag::HangingPiece, Side::White, "e5") ==
          nullptr);
    CHECK(find_motif(position_motifs(chess::Board("7k/8/3p4/4n3/8/2B5/8/K7 w - - 0 1")),
                     MotifTag::HangingPiece, Side::White, "e5") == nullptr);

    const auto absolute = position_motifs(chess::Board("4k3/4n3/8/8/8/8/8/K3R3 w - - 0 1"));
    expect_motif(absolute, MotifTag::PinAbsolute, Side::White, "e1", 'R',
                 {{"e7", 'n'}, {"e8", 'k'}});
    const auto no_absolute = position_motifs(chess::Board("4k3/4n3/8/8/2B5/8/8/K7 w - - 0 1"));
    CHECK(find_motif(no_absolute, MotifTag::PinAbsolute, Side::White, "c4") == nullptr);

    const auto relative = position_motifs(chess::Board("4q1k1/8/4n3/8/8/8/8/K3R3 w - - 0 1"));
    expect_motif(relative, MotifTag::PinRelative, Side::White, "e1", 'R',
                 {{"e6", 'n'}, {"e8", 'q'}});
    const auto equal_rear = position_motifs(chess::Board("4b1k1/8/4n3/8/8/8/8/K3R3 w - - 0 1"));
    CHECK(find_motif(equal_rear, MotifTag::PinRelative, Side::White, "e1") == nullptr);

    const auto losing_relative =
        position_motifs(chess::Board("4q2k/8/4n3/8/8/8/8/K3R2r w - - 0 1"));
    CHECK(find_motif(losing_relative, MotifTag::PinRelative, Side::White, "e1") == nullptr);
}
TEST_CASE("hanging motifs follow the turn and suppress captures in progress", "[unit]") {
    using chesswhy::tactics::position_motifs;
    using chesswhy::tactics::see;

    constexpr std::string_view fen =
        "r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4";
    const chess::Board root(fen);
    const auto root_motifs = position_motifs(root);
    CHECK(std::ranges::none_of(root_motifs, [](const Motif& motif) {
        return motif.tag == MotifTag::HangingPiece && motif.side == Side::Black;
    }));

    chess::Board line_board(fen);
    const auto line = parse_line(line_board, {"b5c6", "d7c6"});
    chess::Board after_capture(fen);
    after_capture.makeMove(line.front());
    CHECK(see(after_capture, square("c6"), chess::Color::BLACK) == 3);

    const auto line_result = chesswhy::tactics::line_motifs(root, line);
    CHECK(std::ranges::none_of(line_result, [](const Motif& motif) {
        return motif.tag == MotifTag::HangingPiece && motif.side == Side::Black && motif.ply == 1;
    }));
}

TEST_CASE("state motifs identify back-rank weakness and overloaded defenders", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto weak = position_motifs(chess::Board("7k/6pp/8/8/8/8/8/R5K1 w - - 0 1"));
    expect_motif(weak, MotifTag::BackRankWeakness, Side::White, "h8", 'k');
    const auto luft = position_motifs(chess::Board("7k/6p1/8/8/8/8/8/R5K1 w - - 0 1"));
    CHECK(find_motif(luft, MotifTag::BackRankWeakness, Side::White, "h8") == nullptr);
    const auto start =
        position_motifs(chess::Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    CHECK(std::ranges::none_of(
        start, [](const Motif& motif) { return motif.tag == MotifTag::BackRankWeakness; }));

    const auto overloaded = position_motifs(chess::Board("7k/8/8/3p4/2n1n3/8/1N3N2/K7 w - - 0 1"));
    expect_motif(overloaded, MotifTag::OverloadedDefender, Side::White, "d5", 'p',
                 {{"c4", 'n'}, {"e4", 'n'}});
    const auto no_overload = position_motifs(chess::Board("7k/8/3p4/8/2n1n3/8/1N3N2/K7 w - - 0 1"));
    CHECK(find_motif(no_overload, MotifTag::OverloadedDefender, Side::White, "d6") == nullptr);
}

TEST_CASE("state motifs find trapped pieces only when every legal escape stays lost", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto trapped = position_motifs(chess::Board("k6n/8/8/8/8/8/2B5/4KR1R w - - 0 1"));
    expect_motif(trapped, MotifTag::TrappedPiece, Side::White, "h8", 'n');
    const Motif* trapped_piece = find_motif(trapped, MotifTag::TrappedPiece, Side::White, "h8");
    REQUIRE(trapped_piece != nullptr);
    CHECK(trapped_piece->gain == 3);
    const auto escape = position_motifs(chess::Board("k6n/8/8/8/8/8/8/4KR1R w - - 0 1"));
    CHECK(find_motif(escape, MotifTag::TrappedPiece, Side::White, "h8") == nullptr);
}

TEST_CASE("state motifs are ordered and omitted for terminal positions", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto motifs = position_motifs(chess::Board("7k/6pp/8/2n5/1B6/8/8/R5K1 w - - 0 1"));
    REQUIRE(motifs.size() >= 2);
    CHECK(motifs[0].tag == MotifTag::HangingPiece);
    CHECK(motifs[1].tag == MotifTag::BackRankWeakness);
    for (std::size_t index = 1; index < motifs.size(); ++index) {
        CHECK(static_cast<unsigned>(motifs[index - 1].tag) <=
              static_cast<unsigned>(motifs[index].tag));
    }
    const auto same_tag = position_motifs(chess::Board("r6k/8/8/2n5/1B6/8/2N5/R5K1 w - - 0 1"));
    std::vector<std::uint8_t> hanging_actors;
    for (const Motif& motif : same_tag) {
        if (motif.tag == MotifTag::HangingPiece) {
            hanging_actors.push_back(motif.actor.square);
        }
    }
    CHECK((hanging_actors ==
           std::vector<std::uint8_t>{static_cast<std::uint8_t>(square("c5").index()),
                                     static_cast<std::uint8_t>(square("a8").index())}));

    CHECK(position_motifs(chess::Board("7k/6Q1/5K2/8/8/8/8/8 b - - 0 1")).empty());
    CHECK(position_motifs(chess::Board("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1")).empty());
}

TEST_CASE("move motifs detect forks and skewers", "[unit]") {
    const auto fork = line_motifs_from("8/8/2k1q3/8/8/5N2/8/K7 w - - 0 1", {"f3d4"});
    expect_motif(fork, MotifTag::Fork, Side::White, "d4", 'N', {{"c6", 'k'}, {"e6", 'q'}});
    const auto en_prise = line_motifs_from("3r4/8/2k1q3/8/8/5N2/8/K7 w - - 0 1", {"f3d4"});
    CHECK(find_motif(en_prise, MotifTag::Fork, Side::White, "d4") == nullptr);

    const auto skewer = line_motifs_from("7k/8/5r2/4q3/8/8/1B6/K1R5 w - - 0 1", {"b2c3"});
    expect_motif(skewer, MotifTag::Skewer, Side::White, "c3", 'B', {{"e5", 'q'}, {"f6", 'r'}});
    const auto wrong_order = line_motifs_from("7k/8/5r2/4b3/8/8/1B6/K1R5 w - - 0 1", {"b2c3"});
    CHECK(find_motif(wrong_order, MotifTag::Skewer, Side::White, "c3") == nullptr);
    const auto unsafe_king_front =
        line_motifs_from("7r/1Q1nkppp/4p3/p1r5/Pp6/6P1/NqP4P/4RRK1 w - - 2 23",
                         {"f1f7", "e7f7", "b7d7", "f7g6", "e1e6"});
    CHECK(find_motif(unsafe_king_front, MotifTag::Skewer, Side::White, "f7") == nullptr);
}

TEST_CASE("move motifs detect discoveries and double check", "[unit]") {
    const auto discovered_check = line_motifs_from("4k3/8/8/8/8/8/4N3/K3R3 w - - 0 1", {"e2f4"});
    expect_motif(discovered_check, MotifTag::DiscoveredCheck, Side::White, "e1", 'R',
                 {{"e8", 'k'}});
    const auto blocked_check = line_motifs_from("4k3/8/8/4p3/8/8/4N3/K3R3 w - - 0 1", {"e2f4"});
    CHECK(find_motif(blocked_check, MotifTag::DiscoveredCheck, Side::White, "e1") == nullptr);

    const auto discovered_attack = line_motifs_from("4q2k/8/8/8/8/8/4N3/K3R3 w - - 0 1", {"e2f4"});
    expect_motif(discovered_attack, MotifTag::DiscoveredAttack, Side::White, "e1", 'R',
                 {{"e8", 'q'}});
    const auto defended_target = line_motifs_from("4b2k/3q4/8/8/8/8/4N3/K3R3 w - - 0 1", {"e2f4"});
    CHECK(find_motif(defended_target, MotifTag::DiscoveredAttack, Side::White, "e1") == nullptr);

    const auto double_check = line_motifs_from("7k/8/8/8/8/8/7B/K6R w - - 0 1", {"h2e5"});
    expect_motif(double_check, MotifTag::DoubleCheck, Side::White, "e5", 'B', {{"h8", 'k'}});
    const auto single_check = line_motifs_from("7k/8/8/8/8/8/7B/K6R w - - 0 1", {"h2f4"});
    CHECK(find_motif(single_check, MotifTag::DoubleCheck, Side::White, "f4") == nullptr);
}
TEST_CASE("castling rook checks are not discoveries or solo double checks", "[unit]") {
    const auto queenside = line_motifs_from("3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", {"e1c1"});
    CHECK(std::ranges::none_of(queenside, [](const Motif& motif) {
        return motif.tag == MotifTag::DiscoveredCheck || motif.tag == MotifTag::DoubleCheck;
    }));

    const auto kingside = line_motifs_from("5k2/8/8/8/8/8/8/4K2R w K - 0 1", {"e1g1"});
    CHECK(std::ranges::none_of(kingside, [](const Motif& motif) {
        return motif.tag == MotifTag::DiscoveredCheck || motif.tag == MotifTag::DoubleCheck;
    }));
}

TEST_CASE("move motifs detect back-rank mate, defender removal, and promotion", "[unit]") {
    const auto mate = line_motifs_from("7k/6pp/8/8/8/8/8/R6K w - - 0 1", {"a1a8"});
    expect_motif(mate, MotifTag::BackRankMate, Side::White, "a8", 'R', {{"h8", 'k'}});
    const auto luft = line_motifs_from("7k/6p1/7p/8/8/8/8/R6K w - - 0 1", {"a1a8"});
    CHECK(find_motif(luft, MotifTag::BackRankMate, Side::White, "a8") == nullptr);

    const auto removal =
        line_motifs_from("7k/8/4p3/3n4/2B5/8/8/K3R3 w - - 0 1", {"e1e6", "h8g8", "c4d5"});
    expect_motif(removal, MotifTag::RemovalOfDefender, Side::White, "e6", 'R',
                 {{"e6", 'p'}, {"d5", 'n'}});
    CHECK(find_motif(line_motifs_from("7k/8/4p3/3n4/2B5/8/8/K3R3 w - - 0 1", {"e1e6", "h8g8"}),
                     MotifTag::RemovalOfDefender, Side::White, "e6") == nullptr);
    const auto unprofitable_removal =
        line_motifs_from("r1r3k1/5pp1/q4n1p/p2p4/1b1Q4/3NP1B1/1P3PPP/R1R3K1 b kq - 1 23",
                         {"c8c4", "c1c4", "d5c4", "d3b4", "a5b4", "a1a6"});
    CHECK(find_motif(unprofitable_removal, MotifTag::RemovalOfDefender, Side::White, "c4") ==
          nullptr);

    const auto promotion = line_motifs_from("7k/P7/8/8/8/8/8/4K3 w - - 0 1", {"a7a8q"});
    expect_motif(promotion, MotifTag::Promotion, Side::White, "a8", 'Q');
    const auto ordinary_move = line_motifs_from("7k/8/P7/8/8/8/8/4K3 w - - 0 1", {"a6a7"});
    CHECK(find_motif(ordinary_move, MotifTag::Promotion, Side::White, "a7") == nullptr);
}

TEST_CASE("a state motif is reported only on the ply it first appears", "[unit]") {
    chess::Board board("4k3/p3n3/8/8/8/8/8/K4R2 w - - 0 1");
    const std::vector<chess::Move> line = parse_line(board, {"f1e1", "a7a6"});
    const auto motifs =
        chesswhy::tactics::line_motifs(chess::Board("4k3/p3n3/8/8/8/8/8/K4R2 w - - 0 1"), line);
    std::vector<int> pin_plies;
    for (const Motif& motif : motifs) {
        if (motif.tag == MotifTag::PinAbsolute && motif.side == Side::White &&
            motif.actor.square == static_cast<std::uint8_t>(square("e1").index())) {
            pin_plies.push_back(motif.ply);
        }
    }
    CHECK(pin_plies == std::vector<int>{1});
}

TEST_CASE("trapped-piece line motifs require later actor activity", "[unit]") {
    constexpr std::string_view fen = "k6n/8/8/8/8/8/2B5/4K1RR w - - 0 1";
    const auto ignored = line_motifs_from(fen, {"g1f1", "a8b8"});
    CHECK(find_motif(ignored, MotifTag::TrappedPiece, Side::White, "h8") == nullptr);

    const auto retained = line_motifs_from(fen, {"g1f1", "h8f7"});
    const Motif* trapped = find_motif(retained, MotifTag::TrappedPiece, Side::White, "h8");
    REQUIRE(trapped != nullptr);
    CHECK(trapped->ply == 1);
}

TEST_CASE("motif fixtures exercise every frozen include and exclude", "[unit]") {
    const auto path =
        std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / "motifs.json";
    std::ifstream input(path);
    REQUIRE(input.is_open());
    const json root = json::parse(input);
    REQUIRE(root.is_object());
    REQUIRE(root.contains("comment"));
    REQUIRE(root["comment"].is_array());
    REQUIRE(root.contains("motif_fixtures"));
    REQUIRE(root["motif_fixtures"].is_array());

    for (const auto& fixture : root["motif_fixtures"]) {
        REQUIRE(fixture.is_object());
        REQUIRE(fixture.size() == 7);
        for (const std::string_view field : {"id", "title", "source", "fen"}) {
            REQUIRE(fixture.contains(field));
            REQUIRE(fixture[std::string(field)].is_string());
        }
        REQUIRE(fixture.contains("line_uci"));
        REQUIRE(fixture["line_uci"].is_array());
        REQUIRE(fixture.contains("includes"));
        REQUIRE(fixture["includes"].is_array());
        REQUIRE(fixture.contains("excludes"));
        REQUIRE(fixture["excludes"].is_array());

        const std::string fen = fixture["fen"].get<std::string>();
        chess::Board board(fen);
        const chess::Board root_board = board;
        std::vector<chess::Move> line;
        line.reserve(fixture["line_uci"].size());
        for (const auto& move_json : fixture["line_uci"]) {
            REQUIRE(move_json.is_string());
            const std::string uci = move_json.get<std::string>();
            REQUIRE((uci.size() == 4 || uci.size() == 5));
            REQUIRE(chess::Square::is_valid_string_sq(std::string_view(uci).substr(0, 2)));
            REQUIRE(chess::Square::is_valid_string_sq(std::string_view(uci).substr(2, 2)));
            const chess::Move move = chess::uci::uciToMove(board, uci);
            REQUIRE(move != chess::Move::NO_MOVE);
            REQUIRE(board.isLegal(move));
            line.push_back(move);
            board.makeMove(move);
        }

        const std::vector<Motif> state = chesswhy::tactics::position_motifs(root_board);
        const std::vector<Motif> walked = chesswhy::tactics::line_motifs(root_board, line);
        const auto actual_at_ply = [&state, &walked](int ply) -> const std::vector<Motif>& {
            return ply == 0 ? state : walked;
        };

        for (const auto& expected : fixture["includes"]) {
            INFO("fixture " << fixture["id"].get<std::string>() << " include " << expected.dump());
            validate_fixture_instance(expected, true);
            const int ply = expected["ply"].get<int>();
            REQUIRE(ply >= 0);
            REQUIRE(static_cast<std::size_t>(ply) <= line.size());
            const auto& actual = actual_at_ply(ply);
            CHECK(std::ranges::any_of(actual, [&expected](const Motif& motif) {
                return fixture_instance_matches(motif, expected, true);
            }));
        }
        for (const auto& forbidden : fixture["excludes"]) {
            INFO("fixture " << fixture["id"].get<std::string>() << " exclude " << forbidden.dump());
            validate_fixture_instance(forbidden, false);
            const int ply = forbidden.contains("ply") ? forbidden["ply"].get<int>() : -1;
            if (ply >= 0) {
                REQUIRE(static_cast<std::size_t>(ply) <= line.size());
                const auto& actual = actual_at_ply(ply);
                CHECK(std::ranges::none_of(actual, [&forbidden](const Motif& motif) {
                    return fixture_instance_matches(motif, forbidden, false);
                }));
            } else {
                CHECK(std::ranges::none_of(state, [&forbidden](const Motif& motif) {
                    return fixture_instance_matches(motif, forbidden, false);
                }));
                CHECK(std::ranges::none_of(walked, [&forbidden](const Motif& motif) {
                    return fixture_instance_matches(motif, forbidden, false);
                }));
            }
        }
    }
}
