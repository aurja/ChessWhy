#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
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
        MotifTag::PinTactical,
        MotifTag::Fork,
        MotifTag::Skewer,
        MotifTag::DiscoveredCheck,
        MotifTag::DiscoveredAttack,
        MotifTag::DoubleCheck,
        MotifTag::BackRankMate,
        MotifTag::Promotion,
    };
    const std::string tag = value.get<std::string>();
    return std::ranges::any_of(
        tags, [tag](MotifTag candidate) { return chesswhy::to_string(candidate) == tag; });
}
[[nodiscard]] bool valid_fixture_threat(const json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("kind") ||
        !value["kind"].is_string() || !value.contains("square") ||
        !square_name_is_valid(value["square"])) {
        return false;
    }
    constexpr std::array kinds{chesswhy::ThreatKind::Fork,
                               chesswhy::ThreatKind::Skewer,
                               chesswhy::ThreatKind::DiscoveredCheck,
                               chesswhy::ThreatKind::DiscoveredAttack,
                               chesswhy::ThreatKind::DoubleCheck,
                               chesswhy::ThreatKind::BackRankMate,
                               chesswhy::ThreatKind::Promotion,
                               chesswhy::ThreatKind::Checkmate,
                               chesswhy::ThreatKind::Capture};
    const std::string kind = value["kind"].get<std::string>();
    return std::ranges::any_of(kinds, [kind](chesswhy::ThreatKind candidate) {
        return chesswhy::to_string(candidate) == kind;
    });
}
[[nodiscard]] bool valid_fixture_uci(const json& value) {
    if (!value.is_string()) {
        return false;
    }
    const std::string uci = value.get<std::string>();
    if ((uci.size() != 4 && uci.size() != 5) ||
        !chess::Square::is_valid_string_sq(std::string_view(uci).substr(0, 2)) ||
        !chess::Square::is_valid_string_sq(std::string_view(uci).substr(2, 2))) {
        return false;
    }
    return uci.size() == 4 || uci[4] == 'q' || uci[4] == 'r' || uci[4] == 'b' || uci[4] == 'n';
}

[[nodiscard]] bool valid_fixture_held_to(const json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("kind") ||
        !value["kind"].is_string() || !value.contains("name") || !value["name"].is_string()) {
        return false;
    }
    const std::string kind = value["kind"].get<std::string>();
    const std::string name = value["name"].get<std::string>();
    if (kind == "file") {
        return name.size() == 1 && name[0] >= 'a' && name[0] <= 'h';
    }
    if (kind == "rank") {
        return name.size() == 1 && name[0] >= '1' && name[0] <= '8';
    }
    if (kind == "square") {
        return name.size() == 2 && name[0] >= 'a' && name[0] <= 'h' && name[1] >= '1' &&
               name[1] <= '8';
    }
    if (kind == "diagonal") {
        if (name.size() != 5 || name[2] != '-' || name[0] >= name[3] || name[0] < 'a' ||
            name[0] > 'h' || name[3] < 'a' || name[3] > 'h' || name[1] < '1' ||
            name[1] > '8' || name[4] < '1' || name[4] > '8') {
            return false;
        }
        const int file_delta = name[3] - name[0];
        const int rank_delta = name[4] - name[1];
        const bool first_on_edge =
            name[0] == 'a' || name[0] == 'h' || name[1] == '1' || name[1] == '8';
        const bool second_on_edge =
            name[3] == 'a' || name[3] == 'h' || name[4] == '1' || name[4] == '8';
        return (rank_delta == file_delta || rank_delta == -file_delta) && first_on_edge &&
               second_on_edge;
    }
    if (kind != "squares") {
        return false;
    }
    std::string previous;
    std::size_t start = 0;
    while (start < name.size()) {
        const std::size_t comma = name.find(',', start);
        const std::string current = name.substr(start, comma - start);
        if (current.size() != 2 || current[0] < 'a' || current[0] > 'h' || current[1] < '1' ||
            current[1] > '8' || (!previous.empty() && current <= previous)) {
            return false;
        }
        previous = current;
        if (comma == std::string::npos) {
            return true;
        }
        start = comma + 1;
    }
    return false;
}

[[nodiscard]] bool valid_fixture_restriction(const json& value) {
    if (!value.is_object() || value.size() != 3 || !value.contains("restricted") ||
        !value["restricted"].is_array() || value["restricted"].empty() ||
        !value.contains("free") || !value["free"].is_array() || !value.contains("held_to") ||
        !valid_fixture_held_to(value["held_to"])) {
        return false;
    }
    const auto valid_moves = [](const json& moves) {
        std::string previous;
        for (const auto& move : moves) {
            if (!valid_fixture_uci(move)) {
                return false;
            }
            const std::string current = move.get<std::string>();
            if (!previous.empty() && current <= previous) {
                return false;
            }
            previous = current;
        }
        return true;
    };
    return valid_moves(value["restricted"]) && valid_moves(value["free"]);
}


[[nodiscard]] bool valid_fixture_instance_field(std::string_view field) {
    return field == "tag" || field == "side" || field == "ply" || field == "actor" ||
           field == "targets" || field == "threat" || field == "restriction";
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
    if (expected.contains("threat")) {
        if (!valid_fixture_threat(expected["threat"]) || !motif.threat ||
            expected["threat"]["kind"] != chesswhy::to_string(motif.threat->kind) ||
            expected["threat"]["square"] != square_name(motif.threat->square)) {
            return false;
        }
    }
    if (expected.contains("restriction")) {
        if (!valid_fixture_restriction(expected["restriction"]) || !motif.restriction) {
            return false;
        }
        const auto& expected_restriction = expected["restriction"];
        const chesswhy::MotifRestriction& actual_restriction = motif.restriction.value();
        if (expected_restriction["restricted"].get<std::vector<std::string>>() !=
                actual_restriction.restricted ||
            expected_restriction["free"].get<std::vector<std::string>>() !=
                actual_restriction.free ||
            expected_restriction["held_to"]["kind"] !=
                chesswhy::to_string(actual_restriction.held_to.kind) ||
            expected_restriction["held_to"]["name"] != actual_restriction.held_to.name) {
            return false;
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
        if (expected["tag"] == chesswhy::to_string(MotifTag::PinTactical)) {
            REQUIRE(expected.contains("threat"));
        }
        if (expected.contains("threat")) {
            REQUIRE(valid_fixture_threat(expected["threat"]));
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
        if (expected.contains("threat")) {
            REQUIRE(valid_fixture_threat(expected["threat"]));
        }
    }
    if (expected.contains("restriction")) {
        REQUIRE(valid_fixture_restriction(expected["restriction"]));
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
    SECTION("equal-value attackers choose the exchange line that respects a pin") {
        const chess::Board board("7k/8/8/8/8/Br6/rPK5/8 b - - 0 1");
        CHECK(see(board, square("a3"), chess::Color::BLACK) == 3);
    }
    SECTION("equal-valued knight and bishop attackers both enter the exchange") {
        const chess::Board board("6k1/6p1/5n2/6B1/4N3/8/6Q1/7K w - - 0 1");
        CHECK(see(board, square("f6"), chess::Color::WHITE) == 3);
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
    const chess::Board check_restricts_hanging("4k3/8/8/8/1r5Q/8/8/4R2K b - - 0 1");
    REQUIRE(check_restricts_hanging.inCheck());
    CHECK(chesswhy::tactics::see(check_restricts_hanging, square("h4"), chess::Color::BLACK) >= 3);
    CHECK(find_motif(position_motifs(check_restricts_hanging), MotifTag::HangingPiece, Side::Black,
                     "h4") == nullptr);

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

TEST_CASE("restricted movement reports held moves and line constraints", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto knight_pin =
        position_motifs(chess::Board("4k3/4n3/8/8/8/8/8/K3R3 w - - 0 1"));
    const Motif* absolute_knight = find_motif(knight_pin, MotifTag::PinAbsolute, Side::White, "e1");
    REQUIRE(absolute_knight != nullptr);
    REQUIRE(absolute_knight->restriction.has_value());
    const auto& knight_restriction = absolute_knight->restriction.value();
    const PieceOnSquare held_knight{.square = static_cast<std::uint8_t>(square("e7").index()),
                                    .piece = 'n'};
    CHECK(knight_restriction.piece == held_knight);
    const std::vector<std::string> knight_moves{
        "e7c6", "e7c8", "e7d5", "e7f5", "e7g6", "e7g8"};
    CHECK(knight_restriction.restricted == knight_moves);
    CHECK(knight_restriction.free.empty());
    const chesswhy::HeldTo knight_held_to{.kind = chesswhy::HeldTo::Kind::Square,
                                          .name = "e7"};
    CHECK(knight_restriction.held_to == knight_held_to);

    constexpr std::string_view file_pin_fen = "4k3/4r3/8/8/8/8/8/K3R3";
    const auto file_pin =
        position_motifs(chess::Board(std::string(file_pin_fen) + " w - - 0 1"));
    const auto file_pin_black_to_move =
        position_motifs(chess::Board(std::string(file_pin_fen) + " b - - 0 1"));
    const Motif* held_rook =
        find_motif(file_pin, MotifTag::PinAbsolute, Side::White, "e1");
    const Motif* held_rook_black_to_move =
        find_motif(file_pin_black_to_move, MotifTag::PinAbsolute, Side::White, "e1");
    REQUIRE(held_rook != nullptr);
    REQUIRE(held_rook_black_to_move != nullptr);
    REQUIRE(held_rook->restriction.has_value());
    REQUIRE(held_rook_black_to_move->restriction.has_value());
    CHECK(held_rook->restriction == held_rook_black_to_move->restriction);
    CHECK_FALSE(held_rook->restriction->free.empty());
    const chesswhy::HeldTo file_held_to{.kind = chesswhy::HeldTo::Kind::File, .name = "e"};
    CHECK(held_rook->restriction->held_to == file_held_to);

    const auto rank_pin = position_motifs(chess::Board("K7/8/8/8/8/8/8/R2r3k w - - 0 1"));
    const Motif* held_rank_rook =
        find_motif(rank_pin, MotifTag::PinAbsolute, Side::White, "a1");
    REQUIRE(held_rank_rook != nullptr);
    REQUIRE(held_rank_rook->restriction.has_value());
    const chesswhy::HeldTo rank_held_to{.kind = chesswhy::HeldTo::Kind::Rank, .name = "1"};
    CHECK(held_rank_rook->restriction->held_to == rank_held_to);

    const auto diagonal_pin =
        position_motifs(chess::Board("8/8/4k3/8/2b5/8/B7/K7 w - - 0 1"));
    const Motif* held_bishop =
        find_motif(diagonal_pin, MotifTag::PinAbsolute, Side::White, "a2");
    REQUIRE(held_bishop != nullptr);
    REQUIRE(held_bishop->restriction.has_value());
    const chesswhy::HeldTo diagonal_held_to{.kind = chesswhy::HeldTo::Kind::Diagonal,
                                            .name = "a2-g8"};
    CHECK(held_bishop->restriction->held_to == diagonal_held_to);

    const auto relative_pin =
        position_motifs(chess::Board("4q1k1/4r3/8/8/8/8/8/K3R3 w - - 0 1"));
    const Motif* held_relative_rook =
        find_motif(relative_pin, MotifTag::PinRelative, Side::White, "e1");
    REQUIRE(held_relative_rook != nullptr);
    REQUIRE(held_relative_rook->restriction.has_value());
    CHECK_FALSE(held_relative_rook->restriction->restricted.empty());
    CHECK_FALSE(held_relative_rook->restriction->free.empty());
    CHECK(held_relative_rook->restriction->held_to == file_held_to);

    const auto overloaded = position_motifs(
        chess::Board("7k/8/8/3p4/2n1n3/8/1N3N2/K7 w - - 0 1"));
    const Motif* overloaded_pawn =
        find_motif(overloaded, MotifTag::OverloadedDefender, Side::White, "d5");
    REQUIRE(overloaded_pawn != nullptr);
    REQUIRE(overloaded_pawn->restriction.has_value());
    const auto& overload_restriction = overloaded_pawn->restriction.value();
    const PieceOnSquare held_overloaded_pawn{
        .square = static_cast<std::uint8_t>(square("d5").index()), .piece = 'p'};
    CHECK(overload_restriction.piece == held_overloaded_pawn);
    CHECK((overload_restriction.restricted ==
           std::vector<std::string>{"d5c4", "d5d4", "d5e4"}));
    CHECK(overload_restriction.free.empty());
    const chesswhy::HeldTo overload_held_to{.kind = chesswhy::HeldTo::Kind::Square,
                                            .name = "d5"};
    CHECK(overload_restriction.held_to == overload_held_to);

    const auto tactical = position_motifs(chess::Board("8/8/6k1/3N2p1/8/8/4q3/K7 b - - 0 1"));
    const Motif* tactical_pin =
        find_motif(tactical, MotifTag::PinTactical, Side::White, "d5");
    REQUIRE(tactical_pin != nullptr);
    REQUIRE(tactical_pin->restriction.has_value());
    const auto& tactical_restriction = tactical_pin->restriction.value();
    const PieceOnSquare held_tactical_pawn{
        .square = static_cast<std::uint8_t>(square("g5").index()), .piece = 'p'};
    CHECK(tactical_restriction.piece == held_tactical_pawn);
    CHECK((tactical_restriction.restricted ==
           std::vector<std::string>{"g5f4", "g5g4", "g5h4"}));
    CHECK(tactical_restriction.free.empty());
    const chesswhy::HeldTo tactical_held_to{.kind = chesswhy::HeldTo::Kind::Square,
                                            .name = "g5"};
    CHECK(tactical_restriction.held_to == tactical_held_to);
}

TEST_CASE("restricted pawn move sets include hypothetical captures", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const auto empty_capture = position_motifs(
        chess::Board("4k3/8/8/4p3/8/8/8/K3R3 w - - 0 1"));
    const Motif* empty_capture_pin =
        find_motif(empty_capture, MotifTag::PinAbsolute, Side::White, "e1");
    REQUIRE(empty_capture_pin != nullptr);
    REQUIRE(empty_capture_pin->restriction.has_value());
    CHECK((empty_capture_pin->restriction->restricted ==
           std::vector<std::string>{"e5d4", "e5f4"}));
    CHECK(empty_capture_pin->restriction->free == std::vector<std::string>{"e5e4"});

    const auto own_piece_capture = position_motifs(
        chess::Board("4k3/8/8/4p3/3n4/8/8/K3R3 w - - 0 1"));
    const Motif* own_piece_pin =
        find_motif(own_piece_capture, MotifTag::PinAbsolute, Side::White, "e1");
    REQUIRE(own_piece_pin != nullptr);
    REQUIRE(own_piece_pin->restriction.has_value());
    CHECK(std::ranges::find(own_piece_pin->restriction->restricted, "e5d4") !=
          own_piece_pin->restriction->restricted.end());

    const auto promotion_capture = position_motifs(
        chess::Board("4k3/8/8/8/8/8/4p3/K2NR3 w - - 0 1"));
    const Motif* promotion_pin =
        find_motif(promotion_capture, MotifTag::PinAbsolute, Side::White, "e1");
    REQUIRE(promotion_pin != nullptr);
    REQUIRE(promotion_pin->restriction.has_value());
    CHECK((promotion_pin->restriction->restricted ==
           std::vector<std::string>{"e2d1b", "e2d1n", "e2d1q", "e2d1r", "e2f1b", "e2f1n",
                                    "e2f1q", "e2f1r"}));
    CHECK(promotion_pin->restriction->free.empty());
}

TEST_CASE("a held piece other than a pawn without legal moves restricts nothing", "[unit]") {
    using chesswhy::tactics::position_motifs;

    const chess::Board boxed_defender("7k/8/3n4/2nrn3/3n4/1N3N2/8/K7 b - - 0 1");
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, boxed_defender);
    CHECK(std::ranges::none_of(legal_moves, [](const chess::Move move) {
        return move.from() == square("d5");
    }));
    CHECK(find_motif(position_motifs(boxed_defender), MotifTag::OverloadedDefender, Side::White,
                     "d5") == nullptr);
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

TEST_CASE("trapped pieces evaluate every legal escape on a side-to-move board", "[unit]") {
    using chesswhy::tactics::position_motifs;
    using chesswhy::tactics::see;

    const chess::Board board("7k/8/5qN1/8/8/8/8/7K b - - 0 1");
    chess::Board white_to_move = board;
    white_to_move.makeNullMove();
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, white_to_move);
    const chess::Square knight = square("g6");
    CHECK(chess::movegen::anylegalmoves(board));
    CHECK_FALSE(board.isAttacked(board.kingSq(chess::Color::WHITE), chess::Color::BLACK));
    CHECK(see(board, knight, chess::Color::BLACK) == 3);
    CHECK(std::ranges::any_of(legal_moves, [&](chess::Move move) {
        return move.from() == knight &&
               white_to_move.at(move.to()).type() == chess::PieceType::KING;
    }));
    for (const chess::Move move : legal_moves) {
        if (move.from() != knight ||
            white_to_move.at<chess::PieceType>(move.to()) == chess::PieceType::KING) {
            continue;
        }
        const chess::Piece captured = white_to_move.at(move.to());
        CHECK((captured == chess::Piece::NONE ||
               chesswhy::tactics::piece_value(captured.type()) < 3));
        chess::Board after = white_to_move;
        after.makeMove(move);
        CHECK(see(after, move.to(), chess::Color::BLACK) >= 1);
    }
    expect_motif(position_motifs(board), MotifTag::TrappedPiece, Side::Black, "g6", 'N');
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
    const auto unprofitable_rear =
        line_motifs_from("5r1k/8/5n2/4q3/8/8/1B6/K1R5 w - - 0 1", {"b2c3"});
    CHECK(find_motif(unprofitable_rear, MotifTag::Skewer, Side::White, "c3") == nullptr);
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

TEST_CASE("move motifs detect back-rank mate and promotion", "[unit]") {
    const auto mate = line_motifs_from("7k/6pp/8/8/8/8/8/R6K w - - 0 1", {"a1a8"});
    expect_motif(mate, MotifTag::BackRankMate, Side::White, "a8", 'R', {{"h8", 'k'}});
    const auto luft = line_motifs_from("7k/6p1/7p/8/8/8/8/R6K w - - 0 1", {"a1a8"});
    CHECK(find_motif(luft, MotifTag::BackRankMate, Side::White, "a8") == nullptr);

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

TEST_CASE("pin tactical options and threat ordering are honored", "[unit]") {
    using chesswhy::ThreatKind;
    using chesswhy::tactics::MotifOptions;
    using chesswhy::tactics::position_motifs;

    const chess::Board rook_blocks_promotion("r3k3/P7/8/8/8/8/8/7K w q - 0 1");
    const auto rook_case = position_motifs(rook_blocks_promotion);
    const auto rook_pin = std::ranges::find_if(rook_case, [](const Motif& motif) {
        return motif.tag == MotifTag::PinTactical && motif.side == Side::White &&
               motif.actor.square == static_cast<std::uint8_t>(square("a7").index()) &&
               motif.threat ==
                   chesswhy::MotifThreat{.kind = ThreatKind::Promotion,
                                         .square = static_cast<std::uint8_t>(square("a8").index())};
    });
    REQUIRE(rook_pin != rook_case.end());
    CHECK(rook_pin->targets ==
          std::vector<PieceOnSquare>{PieceOnSquare{
              .square = static_cast<std::uint8_t>(square("a8").index()), .piece = 'r'}});

    const chess::Board discovery("k7/8/p7/1B6/N7/8/8/R6K w - - 0 1");
    const auto enabled = position_motifs(discovery);
    CHECK(std::ranges::any_of(
        enabled, [](const Motif& motif) { return motif.tag == MotifTag::PinTactical; }));
    const auto disabled = position_motifs(discovery, MotifOptions{.pin_tactical = false});
    CHECK(std::ranges::none_of(
        disabled, [](const Motif& motif) { return motif.tag == MotifTag::PinTactical; }));
    chess::Board line_board = discovery;
    const std::vector<chess::Move> discovery_line = parse_line(line_board, {"a1a2"});
    const auto disabled_line = chesswhy::tactics::line_motifs(discovery, discovery_line,
                                                              MotifOptions{.pin_tactical = false});
    CHECK(std::ranges::none_of(
        disabled_line, [](const Motif& motif) { return motif.tag == MotifTag::PinTactical; }));

    std::vector<const Motif*> ordered;
    for (const Motif& motif : enabled) {
        if (motif.tag == MotifTag::PinTactical &&
            motif.actor.square == static_cast<std::uint8_t>(square("a4").index()) &&
            motif.targets.size() == 2 &&
            motif.targets[0].square == static_cast<std::uint8_t>(square("a6").index()) &&
            motif.targets[1].square == static_cast<std::uint8_t>(square("a8").index())) {
            ordered.push_back(&motif);
        }
    }
    REQUIRE(ordered.size() == 2);
    const auto& first_threat = ordered[0]->threat;
    const auto& second_threat = ordered[1]->threat;
    REQUIRE(first_threat.has_value());
    REQUIRE(second_threat.has_value());
    if (!first_threat.has_value() || !second_threat.has_value()) {
        return;
    }
    CHECK(first_threat->kind == ThreatKind::DiscoveredCheck);
    CHECK(first_threat->square == static_cast<std::uint8_t>(square("b2").index()));
    CHECK(second_threat->kind == ThreatKind::DoubleCheck);
    CHECK(second_threat->square == static_cast<std::uint8_t>(square("b6").index()));
}

TEST_CASE("pin tactical restrictions do not use expired en passant rights", "[unit]") {
    using chesswhy::ThreatKind;
    using chesswhy::tactics::position_motifs;
    using chesswhy::tactics::see;

    const chess::Board board("7k/8/8/3pP3/8/3n4/8/K2R4 w - d6 0 1");
    CHECK(see(board, square("d5"), chess::Color::WHITE) == 0);
    const chess::Board without_blocker("7k/8/8/3pP3/8/8/8/K2R4 w - d6 0 1");
    CHECK(see(without_blocker, square("d5"), chess::Color::WHITE) >= 1);

    const auto motifs = position_motifs(board);
    const auto capture = std::ranges::find_if(motifs, [](const Motif& motif) {
        return motif.tag == MotifTag::PinTactical && motif.side == Side::White &&
               motif.actor.square == static_cast<std::uint8_t>(square("e5").index()) &&
               motif.threat ==
                   chesswhy::MotifThreat{.kind = ThreatKind::Capture,
                                         .square = static_cast<std::uint8_t>(square("d6").index())};
    });
    CHECK(capture == motifs.end());
}

TEST_CASE("pin tactical identity includes its threat", "[unit]") {
    const chess::Board root("7k/6pp/8/8/2B5/8/Q7/1R2K3 w - - 0 1");
    CHECK_FALSE(root.isAttacked(root.kingSq(chess::Color::WHITE), chess::Color::BLACK));
    CHECK_FALSE(root.isAttacked(root.kingSq(chess::Color::BLACK), chess::Color::WHITE));
    chess::Board after = root;
    const std::vector<chess::Move> line = parse_line(after, {"b1a1"});
    const auto unblocked_line = line_motifs_from("k7/8/8/8/Nb6/8/R7/4K3 w - - 0 1", {"a4c3"});
    expect_motif(unblocked_line, MotifTag::DiscoveredCheck, Side::White, "a2", 'R', {{"a8", 'k'}});
    const auto previous = chesswhy::tactics::position_motifs(root);
    const auto walked = chesswhy::tactics::line_motifs(root, line);
    const auto same_actor_and_targets = [](const Motif& motif) {
        return motif.tag == MotifTag::PinTactical && motif.side == Side::White &&
               motif.actor.square == static_cast<std::uint8_t>(square("a2").index()) &&
               motif.targets ==
                   std::vector<PieceOnSquare>{
                       PieceOnSquare{.square = static_cast<std::uint8_t>(square("g7").index()),
                                     .piece = 'p'},
                       PieceOnSquare{.square = static_cast<std::uint8_t>(square("h8").index()),
                                     .piece = 'k'}};
    };
    const auto before = std::ranges::find_if(previous, [&](const Motif& motif) {
        return same_actor_and_targets(motif) && motif.threat &&
               motif.threat->square == static_cast<std::uint8_t>(square("a1").index());
    });
    REQUIRE(before != previous.end());
    const auto current = std::ranges::find_if(walked, [&](const Motif& motif) {
        return same_actor_and_targets(motif) && motif.threat &&
               motif.threat->square == static_cast<std::uint8_t>(square("b2").index()) &&
               motif.ply == 1;
    });
    REQUIRE(current != walked.end());
    CHECK(before->threat != current->threat);
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
namespace {

[[nodiscard]] Motif
test_motif(MotifTag tag, Side side, int ply, std::string_view actor_square, char actor_piece,
           std::initializer_list<std::pair<std::string_view, char>> targets = {}) {
    Motif motif;
    motif.tag = tag;
    motif.side = side;
    motif.ply = ply;
    motif.actor = PieceOnSquare{.square = static_cast<std::uint8_t>(square(actor_square).index()),
                                .piece = actor_piece};
    for (const auto& [target_square, target_piece] : targets) {
        motif.targets.push_back(
            PieceOnSquare{.square = static_cast<std::uint8_t>(square(target_square).index()),
                          .piece = target_piece});
    }
    return motif;
}

[[nodiscard]] json contribution_fixture_instance(const Motif& motif) {
    json targets = json::array();
    for (const PieceOnSquare& target : motif.targets) {
        targets.push_back(square_name(target.square));
    }
    json instance{{"tag", chesswhy::to_string(motif.tag)},
                  {"side", chesswhy::to_string(motif.side)},
                  {"ply", motif.ply},
                  {"actor", square_name(motif.actor.square)},
                  {"targets", std::move(targets)},
                  {"contribution", motif.contribution
                                       ? json(chesswhy::to_string(*motif.contribution))
                                       : json(nullptr)}};
    if (motif.threat) {
        instance["threat"] = {{"kind", chesswhy::to_string(motif.threat->kind)},
                              {"square", square_name(motif.threat->square)}};
    }
    return instance;
}

[[nodiscard]] json label_fixture_instance(const chesswhy::TacticLabel& label) {
    json targets = json::array();
    for (const PieceOnSquare& target : label.targets) {
        targets.push_back(square_name(target.square));
    }
    return {{"tag", chesswhy::to_string(label.tag)},
            {"side", chesswhy::to_string(label.side)},
            {"ply", label.ply},
            {"actor", square_name(label.actor.square)},
            {"targets", std::move(targets)}};
}

[[nodiscard]] bool valid_contribution_fixture_instance(const json& expected) {
    if (!expected.is_object() || expected.size() != (expected.contains("threat") ? 8U : 7U) ||
        !expected.contains("tag") || !valid_fixture_tag(expected["tag"]) ||
        !expected.contains("side") || !expected["side"].is_string() ||
        !(expected["side"] == "white" || expected["side"] == "black") ||
        !expected.contains("ply") || !expected["ply"].is_number_integer() ||
        expected["ply"].get<int>() < 0 || !expected.contains("actor") ||
        !square_name_is_valid(expected["actor"]) || !expected.contains("targets") ||
        !expected["targets"].is_array() || !expected.contains("contribution") ||
        !expected["contribution"].is_string() || !expected.contains("reason") ||
        !expected["reason"].is_string()) {
        return false;
    }
    if ((expected["tag"] == chesswhy::to_string(MotifTag::PinTactical)) !=
            expected.contains("threat") ||
        (expected.contains("threat") && !valid_fixture_threat(expected["threat"]))) {
        return false;
    }
    const std::string contribution = expected["contribution"].get<std::string>();
    if (contribution != "direct" && contribution != "indirect" && contribution != "none") {
        return false;
    }
    return std::ranges::all_of(expected["targets"],
                               [](const json& target) { return square_name_is_valid(target); });
}

[[nodiscard]] bool valid_label_fixture_instance(const json& expected, bool include) {
    if (!expected.is_object()) {
        return false;
    }
    constexpr std::array<std::string_view, 6> allowed_fields{"tag",   "side",    "ply",
                                                             "actor", "targets", "reason"};
    for (auto iterator = expected.begin(); iterator != expected.end(); ++iterator) {
        if (std::ranges::none_of(allowed_fields, [&iterator](std::string_view field) {
                return iterator.key() == field;
            })) {
            return false;
        }
    }
    if (include &&
        (expected.size() != allowed_fields.size() || !expected.contains("tag") ||
         !expected.contains("side") || !expected.contains("ply") || !expected.contains("actor") ||
         !expected.contains("targets") || !expected.contains("reason"))) {
        return false;
    }
    if (expected.contains("tag") &&
        (!expected["tag"].is_string() ||
         expected["tag"] != chesswhy::to_string(chesswhy::TacticLabelTag::RemovalOfDefender))) {
        return false;
    }
    if (expected.contains("side") &&
        (!expected["side"].is_string() ||
         !(expected["side"] == "white" || expected["side"] == "black"))) {
        return false;
    }
    if (expected.contains("ply") &&
        (!expected["ply"].is_number_integer() || expected["ply"].get<int>() < 1)) {
        return false;
    }
    if (expected.contains("actor") && !square_name_is_valid(expected["actor"])) {
        return false;
    }
    if (expected.contains("targets") &&
        (!expected["targets"].is_array() ||
         !std::ranges::all_of(expected["targets"],
                              [](const json& target) { return square_name_is_valid(target); }))) {
        return false;
    }
    return !expected.contains("reason") || expected["reason"].is_string();
}

[[nodiscard]] bool contribution_matches(const Motif& motif, const json& expected) {
    if (!motif.contribution || expected["tag"] != chesswhy::to_string(motif.tag) ||
        expected["side"] != chesswhy::to_string(motif.side) ||
        expected["ply"].get<int>() != motif.ply ||
        expected["actor"].get<std::string>() != square_name(motif.actor.square) ||
        expected["contribution"] != chesswhy::to_string(*motif.contribution) ||
        expected["targets"].size() != motif.targets.size()) {
        return false;
    }
    if (expected.contains("threat") &&
        (!motif.threat || expected["threat"]["kind"] != chesswhy::to_string(motif.threat->kind) ||
         expected["threat"]["square"] != square_name(motif.threat->square))) {
        return false;
    }
    for (std::size_t index = 0; index < motif.targets.size(); ++index) {
        if (expected["targets"][index] != square_name(motif.targets[index].square)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool label_matches_include(const chesswhy::TacticLabel& label, const json& expected) {
    if (expected["tag"] != chesswhy::to_string(label.tag) ||
        expected["side"] != chesswhy::to_string(label.side) ||
        expected["ply"].get<int>() != label.ply ||
        expected["actor"].get<std::string>() != square_name(label.actor.square) ||
        expected["targets"].size() != label.targets.size()) {
        return false;
    }
    for (std::size_t index = 0; index < label.targets.size(); ++index) {
        if (expected["targets"][index] != square_name(label.targets[index].square)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool label_matches_exclude(const chesswhy::TacticLabel& label,
                                         const json& forbidden) {
    if (forbidden.contains("tag") && forbidden["tag"] != chesswhy::to_string(label.tag)) {
        return false;
    }
    if (forbidden.contains("side") && forbidden["side"] != chesswhy::to_string(label.side)) {
        return false;
    }
    if (forbidden.contains("ply") && forbidden["ply"].get<int>() != label.ply) {
        return false;
    }
    if (forbidden.contains("actor") &&
        forbidden["actor"].get<std::string>() != square_name(label.actor.square)) {
        return false;
    }
    if (forbidden.contains("targets")) {
        if (forbidden["targets"].size() != label.targets.size()) {
            return false;
        }
        for (std::size_t index = 0; index < label.targets.size(); ++index) {
            if (forbidden["targets"][index] != square_name(label.targets[index].square)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::vector<chess::Move> parse_fixture_line(chess::Board board, const json& moves) {
    std::vector<chess::Move> line;
    line.reserve(moves.size());
    for (const json& move_json : moves) {
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
    return line;
}

} // namespace

TEST_CASE("denied moves and answers apply restricted-reply rules", "[unit]") {
    SECTION("back-rank replies remove the blocker and distinguish check from capture") {
        const chess::Board checked("4R1k1/5ppp/8/8/8/8/5PPP/4K3 b - - 0 1");
        const Motif weakness =
            test_motif(MotifTag::BackRankWeakness, Side::White, 0, "g8", 'k');
        const auto moves = chesswhy::tactics::denied_moves(checked, weakness);
        std::vector<std::string> uci_moves;
        for (const chess::Move move : moves) {
            uci_moves.push_back(chess::uci::moveToUci(move, checked.chess960()));
        }
        CHECK((uci_moves == std::vector<std::string>{"g8f7", "g8g7", "g8h7"}));

        const chess::Move escape = chess::uci::uciToMove(checked, "g8g7");
        CHECK(chesswhy::tactics::answers(
            checked, weakness, escape, static_cast<std::uint8_t>(square("e8").index())));

        const chess::Board not_checked("6k1/5ppp/8/8/8/8/5PPP/4K3 b - - 0 1");
        const chess::Move same_escape = chess::uci::uciToMove(not_checked, "g8g7");
        CHECK_FALSE(chesswhy::tactics::answers(
            not_checked, weakness, same_escape, static_cast<std::uint8_t>(square("e8").index())));

        const chess::Board still_attacked("4R1k1/5ppp/8/8/8/8/8/4K1R1 b - - 0 1");
        const chess::Move unsafe_escape = chess::uci::uciToMove(still_attacked, "g8g7");
        CHECK_FALSE(chesswhy::tactics::answers(
            still_attacked, weakness, unsafe_escape,
            static_cast<std::uint8_t>(square("e8").index())));
    }

    SECTION("an absolute-pin line check is ignored, but other attacks still count") {
        const chess::Board board("4k3/4n3/6Q1/8/8/8/8/4R2K b - - 0 1");
        const Motif pin = test_motif(MotifTag::PinAbsolute, Side::White, 0, "e1", 'R',
                                     {{"e7", 'n'}, {"e8", 'k'}});
        const auto denied = chesswhy::tactics::denied_moves(board, pin);
        const chess::Move capture = chess::uci::uciToMove(board, "e7g6");
        REQUIRE(std::ranges::find(denied, capture) != denied.end());
        CHECK(chesswhy::tactics::answers(
            board, pin, capture, static_cast<std::uint8_t>(square("g6").index())));

        const chess::Board double_check("4k3/4n3/5NQ1/8/8/8/8/4R2K b - - 0 1");
        const chess::Move only_queen_captured = chess::uci::uciToMove(double_check, "e7g6");
        CHECK_FALSE(chesswhy::tactics::answers(
            double_check, pin, only_queen_captured,
            static_cast<std::uint8_t>(square("g6").index())));
    }

    SECTION("a denied capture answers a quiet tactic move, including at j equals k") {
        const chess::Board root("4k3/8/8/4N3/R2n3q/8/8/K7 w - - 0 1");
        chess::Board after = root;
        const std::vector<chess::Move> line = parse_line(after, {"e5c6"});
        const chess::Move reply = chess::uci::uciToMove(after, "d4c6");
        const Motif root_pin =
            test_motif(MotifTag::PinRelative, Side::White, 0, "a4", 'R',
                       {{"d4", 'n'}, {"h4", 'q'}});
        const auto denied = chesswhy::tactics::denied_moves(after, root_pin);
        REQUIRE(std::ranges::find(denied, reply) != denied.end());
        CHECK(chesswhy::tactics::answers(
            after, root_pin, reply, static_cast<std::uint8_t>(square("c6").index())));

        std::vector<Motif> root_motif{root_pin};
        chesswhy::tactics::judge_contributions(root, line, root_motif);
        CHECK(root_motif[0].contribution == chesswhy::MotifContribution::Indirect);

        Motif made_by_move = root_pin;
        made_by_move.ply = 1;
        std::vector<Motif> ply_one_motif{made_by_move};
        chesswhy::tactics::judge_contributions(root, line, ply_one_motif);
        CHECK(ply_one_motif[0].contribution == chesswhy::MotifContribution::Indirect);
    }
    SECTION("a relative-pin move on the actor-to-rear segment is not denied") {
        const chess::Board board(
            "1b2r1k1/1p4pp/p1p2p2/8/3P3N/P1P1r2P/1P1Q1PPq/3KR3 w - - 0 29");
        const Motif pin = test_motif(MotifTag::PinRelative, Side::Black, 0, "e8", 'r',
                                     {{"e3", 'P'}, {"e1", 'R'}});
        const auto denied = chesswhy::tactics::denied_moves(board, pin);
        CHECK(denied.empty());
    }
    SECTION("the checking rook's own check is not ignored for a later relative pin") {
        const chess::Board root(
            "3r1r1k/1pp1q1pp/8/3B1p2/1pPn1P2/b3B3/P3QP2/1K5R w - - 0 29");
        chess::Board after = root;
        const std::vector<chess::Move> line =
            parse_line(after, {"h1h7", "h8h7", "e2h5"});
        chess::Board checking_position = root;
        const std::vector<chess::Move> checking_move = parse_line(checking_position, {"h1h7"});
        const Motif pin = test_motif(MotifTag::PinRelative, Side::White, 1, "h7", 'R',
                                     {{"g7", 'p'}, {"e7", 'q'}});
        REQUIRE(checking_position.inCheck());
        const auto denied = chesswhy::tactics::denied_moves(checking_position, pin);
        CHECK(std::ranges::none_of(denied, [&](const chess::Move reply) {
            return chesswhy::tactics::answers(
                checking_position, pin, reply,
                static_cast<std::uint8_t>(checking_move.front().to().index()));
        }));

        std::vector<Motif> motifs{pin};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }
}

TEST_CASE("contributions mark move motifs and X captures direct", "[unit]") {
    SECTION("a move motif is direct on its producing ply") {
        const chess::Board root("6k1/8/6q1/5N2/1B6/8/8/K7 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"f5e7"});
        std::vector<Motif> motifs = chesswhy::tactics::line_motifs(root, line);
        auto fork = std::ranges::find_if(motifs, [](const Motif& motif) {
            return motif.tag == MotifTag::Fork && motif.side == Side::White && motif.ply == 1;
        });
        REQUIRE(fork != motifs.end());
        std::vector<Motif> judged{*fork};
        chesswhy::tactics::judge_contributions(root, line, judged);
        CHECK(judged[0].contribution == chesswhy::MotifContribution::Direct);
    }

    SECTION("an X capture of a target is direct") {
        const chess::Board root("7k/8/8/2n5/8/8/8/K1R5 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"c1c5"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::HangingPiece, Side::White, 0, "a1", 'K', {{"c5", 'n'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::Direct);
    }

    SECTION("an X capture makes an earlier indirect contribution direct") {
        const chess::Board root("7k/8/8/4n3/8/8/1N6/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"b2c4", "h8g8", "e1e5"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::PinRelative, Side::White, 0, "e1", 'R', {{"e5", 'n'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::Direct);
    }
}

TEST_CASE("motif-piece movement is not direct and blocks later indirect attribution", "[unit]") {
    SECTION("an X actor moving while capturing a non-motif piece is not direct") {
        const chess::Board root("7k/8/8/4n3/8/8/1Np5/K1R5 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"c1c2", "h8g8", "b2c4"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::PinRelative, Side::White, 0, "c1", 'R', {{"e5", 'n'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }

    SECTION("a Y target moving is not direct and blocks later indirect attribution") {
        const chess::Board root("7k/8/8/4n2P/8/8/8/KR6 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"a1a2", "e5f7", "h5h6"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::PinRelative, Side::White, 0, "b1", 'R', {{"e5", 'n'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }
}

TEST_CASE("contributions count X captures, not castling movement, as direct", "[unit]") {
    SECTION("castling does not make a motif rook direct") {
        const chess::Board root("4k2r/8/8/8/8/8/8/4K2R w K - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"e1g1"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::HangingPiece, Side::White, 0, "h1", 'R', {{"h8", 'r'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }

    SECTION("en passant directly captures the passed pawn") {
        const chess::Board root("7k/3p3p/8/4P3/8/8/P7/K7 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line =
            parse_line(board, {"a2a3", "h7h6", "a3a4", "d7d5", "e5d6"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::HangingPiece, Side::White, 4, "e5", 'P', {{"d5", 'p'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::Direct);
    }
}

TEST_CASE("indirect contributions require a denied reply that answers an X move", "[unit]") {
    SECTION("a pinned Y motif piece may capture a move's landing square only on the pin line") {
        const chess::Board root("4k3/4n3/8/8/8/8/8/K3RR2 w - - 0 1");
        const auto all_motifs = chesswhy::tactics::position_motifs(root);
        const auto pin = std::ranges::find_if(all_motifs, [](const Motif& motif) {
            return motif.tag == MotifTag::PinAbsolute && motif.side == Side::White &&
                   motif.actor.square == static_cast<std::uint8_t>(square("e1").index());
        });
        REQUIRE(pin != all_motifs.end());
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"f1f5"});
        std::vector<Motif> motifs{*pin};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::Indirect);
    }

    SECTION("a pin with no denied reply to a check is not indirect") {
        const chess::Board root(
            "rn1qk2r/ppp2ppp/5n2/3P2B1/1b2p1b1/2N5/PP3PPP/R2QKBNR w KQkq - 1 8");
        chess::Board after = root;
        const std::vector<chess::Move> line =
            parse_line(after, {"d1a4", "b8c6", "d5c6", "b4c3", "b2c3"});
        std::vector<Motif> motifs{test_motif(MotifTag::PinRelative, Side::White, 1, "a4", 'Q',
                                             {{"a7", 'p'}, {"a8", 'r'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }

    SECTION("the C01 knight's denied reply answers Qa4+") {
        const chess::Board root(
            "rn1qk2r/ppp2ppp/5n2/3P2B1/1b2p1b1/2N5/PP3PPP/R2QKBNR w KQkq - 1 8");
        chess::Board after = root;
        const std::vector<chess::Move> line =
            parse_line(after, {"d1a4", "b8c6", "d5c6", "b4c3", "b2c3"});
        chess::Board checking_position = root;
        const std::vector<chess::Move> checking_line =
            parse_line(checking_position, {"d1a4"});
        REQUIRE(checking_line.size() == 1);
        const Motif pin = test_motif(MotifTag::PinRelative, Side::White, 0, "g5", 'B',
                                     {{"f6", 'n'}, {"d8", 'q'}});
        const chess::Move reply = chess::uci::uciToMove(checking_position, "f6d7");
        const auto denied = chesswhy::tactics::denied_moves(checking_position, pin);
        REQUIRE(std::ranges::find(denied, reply) != denied.end());
        CHECK(chesswhy::tactics::answers(
            checking_position, pin, reply,
            static_cast<std::uint8_t>(checking_line.front().to().index())));

        std::vector<Motif> motifs{pin};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::Indirect);
    }

    SECTION("a castling move is excluded from indirect classification") {
        const chess::Board root("4k2r/8/8/8/8/5B2/8/4K2R w K - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"e1g1"});
        std::vector<Motif> motifs{
            test_motif(MotifTag::HangingPiece, Side::White, 0, "f3", 'B', {{"h8", 'r'}})};
        chesswhy::tactics::judge_contributions(root, line, motifs);
        CHECK(motifs[0].contribution == chesswhy::MotifContribution::None);
    }
}

TEST_CASE("contribution none covers Y-side, ply-n and ply-zero motifs", "[unit]") {
    const chess::Board root("7k/8/8/8/8/8/8/K1R5 w - - 0 1");
    chess::Board board = root;
    const std::vector<chess::Move> line = parse_line(board, {"c1c2"});
    std::vector<Motif> motifs{test_motif(MotifTag::HangingPiece, Side::Black, 0, "h8", 'k'),
                              test_motif(MotifTag::HangingPiece, Side::White, 1, "c2", 'R'),
                              test_motif(MotifTag::HangingPiece, Side::White, 0, "a1", 'K')};
    chesswhy::tactics::judge_contributions(root, line, motifs);
    for (const Motif& motif : motifs) {
        REQUIRE(motif.contribution.has_value());
        CHECK(motif.contribution == chesswhy::MotifContribution::None);
    }
}

TEST_CASE("removal-of-defender labels revive T33 and reject T34", "[unit]") {
    SECTION("T33: removing the bishop releases the knight for capture") {
        const chess::Board root("7k/8/3b4/1N2n3/8/8/8/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"b5d6", "h8h7", "e1e5"});
        const auto labels = chesswhy::tactics::tactic_labels(root, line);
        REQUIRE(labels.size() == 1);
        CHECK(labels[0].tag == chesswhy::TacticLabelTag::RemovalOfDefender);
        CHECK(labels[0].side == Side::White);
        CHECK(labels[0].ply == 1);
        CHECK((labels[0].actor ==
               PieceOnSquare{static_cast<std::uint8_t>(square("d6").index()), 'N'}));
        REQUIRE(labels[0].targets.size() == 2);
        CHECK((labels[0].targets[0] ==
               PieceOnSquare{static_cast<std::uint8_t>(square("d6").index()), 'b'}));
        CHECK((labels[0].targets[1] ==
               PieceOnSquare{static_cast<std::uint8_t>(square("e5").index()), 'n'}));
    }

    SECTION("T34: no later capture means no label") {
        const chess::Board root("7k/8/3b4/1N2n3/8/8/8/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"b5d6", "h8h7"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }
}

TEST_CASE("removal-of-defender labels reject near misses", "[unit]") {
    SECTION("the target remains defended after d is removed") {
        const chess::Board root("4r2k/8/3b4/1N2n3/8/8/8/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"b5d6", "h8h7", "e1e5"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }

    SECTION("a later capture by Y does not count") {
        const chess::Board root("7k/6b1/3b1r2/1N2n3/8/8/8/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line =
            parse_line(board, {"b5d6", "e5f7", "a1b1", "f6f5", "e1e5", "g7e5"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }

    SECTION("an en-passant capture on the target square does not count") {
        const chess::Board root("7k/3p4/2b5/1B1nP3/8/8/8/K2R4 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line =
            parse_line(board, {"b5c6", "d5f4", "a1b1", "d7d5", "e5d6"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }

    SECTION("a later capture with SEE below one does not count") {
        const chess::Board root("7k/6b1/3b1r2/1N2n3/8/8/8/K3R3 w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, {"b5d6", "f6f7", "e1e5"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }
    SECTION("TAC-5: the original target leaves before a queen arrives on its square") {
        const chess::Board root("7k/8/3b4/q1P1n3/8/8/7N/4R2K w - - 0 1");
        chess::Board board = root;
        const std::vector<chess::Move> line =
            parse_line(board, {"c5d6", "e5g6", "h2g4", "a5e5", "g4e5"});
        CHECK(chesswhy::tactics::tactic_labels(root, line).empty());
    }
}

TEST_CASE("tactic fixtures exercise contributions and labels", "[unit]") {
    const auto path =
        std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / "tactics.json";
    std::ifstream input(path);
    REQUIRE(input.is_open());
    const json root = json::parse(input);
    REQUIRE(root.is_object());
    REQUIRE(root.contains("comment"));
    REQUIRE(root["comment"].is_array());
    REQUIRE(root.contains("fixtures"));
    REQUIRE(root["fixtures"].is_array());

    for (const json& fixture : root["fixtures"]) {
        REQUIRE(fixture.is_object());
        REQUIRE(fixture.size() == 9);
        for (const std::string_view field : {"id", "title", "source", "fen"}) {
            REQUIRE(fixture.contains(field));
            REQUIRE(fixture[std::string(field)].is_string());
        }
        INFO("fixture " << fixture["id"].get<std::string>());
        REQUIRE(fixture.contains("line_uci"));
        REQUIRE(fixture["line_uci"].is_array());
        REQUIRE(fixture.contains("margin"));
        REQUIRE((fixture["margin"].is_null() || fixture["margin"].is_number()));
        REQUIRE(fixture.contains("contributions"));
        REQUIRE(fixture["contributions"].is_array());
        REQUIRE(fixture.contains("labels_include"));
        REQUIRE(fixture["labels_include"].is_array());
        REQUIRE(fixture.contains("labels_exclude"));
        REQUIRE(fixture["labels_exclude"].is_array());

        const chess::Board root_board(fixture["fen"].get<std::string>());
        const std::vector<chess::Move> line = parse_fixture_line(root_board, fixture["line_uci"]);
        std::vector<Motif> motifs = chesswhy::tactics::position_motifs(root_board);
        const std::vector<Motif> line_motifs = chesswhy::tactics::line_motifs(root_board, line);
        motifs.insert(motifs.end(), line_motifs.begin(), line_motifs.end());
        chesswhy::tactics::judge_contributions(root_board, line, motifs);
        const std::vector<chesswhy::TacticLabel> labels =
            chesswhy::tactics::tactic_labels(root_board, line);
        const Side tactic_side =
            root_board.sideToMove() == chess::Color::WHITE ? Side::White : Side::Black;
        const std::string tactic_side_name = chesswhy::to_string(tactic_side);

        std::vector<bool> matched(motifs.size(), false);
        for (const json& expected : fixture["contributions"]) {
            REQUIRE(valid_contribution_fixture_instance(expected));
            REQUIRE(expected["side"] == tactic_side_name);
            REQUIRE(expected["ply"].get<int>() <= static_cast<int>(line.size()));
            const auto found =
                std::ranges::find_if(motifs, [&expected, &matched, &motifs](const Motif& actual) {
                    const auto index = static_cast<std::size_t>(&actual - motifs.data());
                    return !matched[index] && contribution_matches(actual, expected);
                });
            if (found == motifs.end()) {
                INFO("fixture " << fixture["id"].get<std::string>() << " differing motif "
                                << expected.dump());
                CHECK(false);
            } else {
                matched[static_cast<std::size_t>(found - motifs.begin())] = true;
            }
        }
        for (std::size_t index = 0; index < motifs.size(); ++index) {
            const Motif& motif = motifs[index];
            if (motif.side != tactic_side) {
                if (motif.contribution != chesswhy::MotifContribution::None) {
                    INFO("fixture " << fixture["id"].get<std::string>() << " differing motif "
                                    << contribution_fixture_instance(motif).dump());
                    CHECK(motif.contribution == chesswhy::MotifContribution::None);
                }
                continue;
            }
            if (motif.tag == MotifTag::PinTactical) {
                continue;
            }
            if (!matched[index]) {
                INFO("fixture " << fixture["id"].get<std::string>() << " differing motif "
                                << contribution_fixture_instance(motif).dump());
                CHECK(false);
            }
        }

        for (const json& expected : fixture["labels_include"]) {
            REQUIRE(valid_label_fixture_instance(expected, true));
            REQUIRE(expected["ply"].get<int>() <= static_cast<int>(line.size()));
            if (!std::ranges::any_of(labels, [&expected](const chesswhy::TacticLabel& label) {
                    return label_matches_include(label, expected);
                })) {
                INFO("fixture " << fixture["id"].get<std::string>() << " differing label "
                                << expected.dump());
                CHECK(false);
            }
        }
        for (const json& forbidden : fixture["labels_exclude"]) {
            REQUIRE(valid_label_fixture_instance(forbidden, false));
            REQUIRE((!forbidden.contains("ply") ||
                     forbidden["ply"].get<int>() <= static_cast<int>(line.size())));
            const auto found =
                std::ranges::find_if(labels, [&forbidden](const chesswhy::TacticLabel& label) {
                    return label_matches_exclude(label, forbidden);
                });
            if (found != labels.end()) {
                INFO("fixture " << fixture["id"].get<std::string>() << " differing label "
                                << label_fixture_instance(*found).dump());
                CHECK(false);
            }
        }
    }
}
