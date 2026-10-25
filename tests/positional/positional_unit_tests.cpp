#include "chesswhy/positional.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace {

using chesswhy::PieceOnSquare;
using chesswhy::PositionalChange;
using chesswhy::PositionalChangeKind;
using chesswhy::PositionalFeature;
using chesswhy::PositionalTag;
using chesswhy::Side;
using nlohmann::json;

[[nodiscard]] chess::Square square(std::string_view name) {
    return {name};
}

[[nodiscard]] const PositionalFeature*
find_feature(const std::vector<PositionalFeature>& actual, PositionalTag tag, Side side,
             std::optional<std::uint8_t> file = std::nullopt) {
    const auto found = std::ranges::find_if(actual, [=](const PositionalFeature& feature) {
        return feature.tag == tag && feature.side == side && (!file || feature.file == file);
    });
    return found == actual.end() ? nullptr : &*found;
}

[[nodiscard]] std::size_t count_features(const std::vector<PositionalFeature>& actual,
                                         PositionalTag tag, Side side) {
    return static_cast<std::size_t>(
        std::ranges::count_if(actual, [=](const PositionalFeature& feature) {
            return feature.tag == tag && feature.side == side;
        }));
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

[[nodiscard]] std::string square_name(std::uint8_t index) {
    return static_cast<std::string>(chess::Square(index));
}

[[nodiscard]] bool square_name_is_valid(const json& value) {
    if (!value.is_string()) {
        return false;
    }
    return chess::Square::is_valid_string_sq(value.get<std::string>());
}

[[nodiscard]] bool valid_tag(const json& value) {
    if (!value.is_string()) {
        return false;
    }
    constexpr std::array tags{
        PositionalTag::IsolatedPawn,
        PositionalTag::DoubledPawns,
        PositionalTag::TripledPawns,
        PositionalTag::BackwardPawn,
        PositionalTag::PassedPawn,
        PositionalTag::ConnectedPassedPawns,
        PositionalTag::PawnIslands,
        PositionalTag::PawnChain,
        PositionalTag::CastlingKingside,
        PositionalTag::CastlingQueenside,
        PositionalTag::KingShieldGap,
        PositionalTag::OpenFileNearKing,
        PositionalTag::HalfOpenFileNearKing,
        PositionalTag::KingZoneAttacker,
        PositionalTag::BishopPair,
        PositionalTag::RookOpenFile,
        PositionalTag::RookHalfOpenFile,
        PositionalTag::RookSeventh,
        PositionalTag::Outpost,
        PositionalTag::BadBishop,
        PositionalTag::MobilityLead,
        PositionalTag::SpaceLead,
        PositionalTag::DevelopmentLead,
        PositionalTag::MaterialAhead,
        PositionalTag::ExchangeUp,
    };
    const std::string tag = value.get<std::string>();
    return std::ranges::any_of(
        tags, [tag](PositionalTag candidate) { return chesswhy::to_string(candidate) == tag; });
}

[[nodiscard]] bool valid_piece(const json& expected) {
    if (!expected.is_object() || expected.size() != 2 || !expected.contains("square") ||
        !square_name_is_valid(expected["square"]) || !expected.contains("piece") ||
        !expected["piece"].is_string()) {
        return false;
    }
    const std::string piece = expected["piece"].get<std::string>();
    return piece.size() == 1 &&
           std::string_view("PNBRQKpnbrqk").find(piece.front()) != std::string_view::npos;
}

void validate_partial_feature(const json& expected, bool is_change) {
    REQUIRE(expected.is_object());
    for (auto iterator = expected.begin(); iterator != expected.end(); ++iterator) {
        const std::string& key = iterator.key();
        if (is_change && key == "ply") {
            REQUIRE(iterator->is_number_integer());
            REQUIRE(iterator->get<int>() >= 1);
        } else if (is_change && key == "change") {
            REQUIRE(iterator->is_string());
            REQUIRE((iterator->get<std::string>() == "appears" ||
                     iterator->get<std::string>() == "disappears"));
        } else if (is_change && key == "holds_at_end") {
            REQUIRE(iterator->is_boolean());
        } else if (key == "tag") {
            REQUIRE(valid_tag(*iterator));
        } else if (key == "side") {
            REQUIRE(iterator->is_string());
            REQUIRE((iterator->get<std::string>() == "white" ||
                     iterator->get<std::string>() == "black"));
        } else if (key == "file") {
            if (!iterator->is_null()) {
                REQUIRE(iterator->is_string());
                const std::string file = iterator->get<std::string>();
                REQUIRE(file.size() == 1);
                REQUIRE((file.front() >= 'a' && file.front() <= 'h'));
            }
        } else if (key == "value") {
            REQUIRE((iterator->is_null() || iterator->is_number_integer()));
        } else if (key == "pieces") {
            REQUIRE(iterator->is_array());
            for (const json& piece : *iterator) {
                REQUIRE(valid_piece(piece));
            }
        } else {
            FAIL("unknown positional fixture field: " << key);
        }
    }
}

[[nodiscard]] bool feature_matches(const PositionalFeature& actual, const json& expected) {
    if (expected.contains("tag") &&
        expected["tag"].get<std::string>() != chesswhy::to_string(actual.tag)) {
        return false;
    }
    if (expected.contains("side") &&
        expected["side"].get<std::string>() != chesswhy::to_string(actual.side)) {
        return false;
    }
    if (expected.contains("file")) {
        if (expected["file"].is_null()) {
            if (actual.file) {
                return false;
            }
        } else if (!actual.file || expected["file"].get<std::string>() !=
                                       std::string(1, static_cast<char>('a' + *actual.file))) {
            return false;
        }
    }
    if (expected.contains("value")) {
        if (expected["value"].is_null()) {
            if (actual.value) {
                return false;
            }
        } else if (!actual.value || expected["value"].get<int>() != *actual.value) {
            return false;
        }
    }
    if (expected.contains("pieces")) {
        const json& pieces = expected["pieces"];
        if (pieces.size() != actual.pieces.size()) {
            return false;
        }
        for (std::size_t index = 0; index < actual.pieces.size(); ++index) {
            const PieceOnSquare& actual_piece = actual.pieces[index];
            const json& expected_piece = pieces[index];
            if (expected_piece["square"].get<std::string>() != square_name(actual_piece.square) ||
                expected_piece["piece"].get<std::string>() != std::string(1, actual_piece.piece)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool change_matches(const PositionalChange& actual, const json& expected) {
    if (expected.contains("ply") && expected["ply"].get<int>() != actual.ply) {
        return false;
    }
    if (expected.contains("change") &&
        expected["change"].get<std::string>() != chesswhy::to_string(actual.change)) {
        return false;
    }
    if (expected.contains("holds_at_end") &&
        expected["holds_at_end"].get<bool>() != actual.holds_at_end) {
        return false;
    }
    return feature_matches(actual.feature, expected);
}

[[nodiscard]] json feature_summary(const PositionalFeature& feature) {
    json pieces = json::array();
    for (const PieceOnSquare& piece : feature.pieces) {
        pieces.push_back(
            json{{"square", square_name(piece.square)}, {"piece", std::string(1, piece.piece)}});
    }
    const json file =
        feature.file ? json(std::string(1, static_cast<char>('a' + *feature.file))) : json(nullptr);
    const json value = feature.value ? json(*feature.value) : json(nullptr);
    return json{{"tag", chesswhy::to_string(feature.tag)},
                {"side", chesswhy::to_string(feature.side)},
                {"file", file},
                {"value", value},
                {"pieces", pieces}};
}

[[nodiscard]] json change_summary(const PositionalChange& change) {
    json result = feature_summary(change.feature);
    result["ply"] = change.ply;
    result["change"] = chesswhy::to_string(change.change);
    result["holds_at_end"] = change.holds_at_end;
    result["captured"] = change.captured;
    return result;
}

void validate_use_change(const json& expected) {
    REQUIRE(expected.is_object());
    REQUIRE((expected.size() == 4 || expected.size() == 5));
    for (const std::string_view key : {"tag", "side", "change", "ply"}) {
        REQUIRE(expected.contains(key));
    }
    REQUIRE_FALSE((expected.contains("file") && expected.contains("square")));
    for (auto iterator = expected.begin(); iterator != expected.end(); ++iterator) {
        if (iterator.key() == "tag") {
            REQUIRE(valid_tag(*iterator));
        } else if (iterator.key() == "side") {
            REQUIRE(iterator->is_string());
            REQUIRE((iterator->get<std::string>() == "white" ||
                     iterator->get<std::string>() == "black"));
        } else if (iterator.key() == "change") {
            REQUIRE(iterator->is_string());
            REQUIRE((iterator->get<std::string>() == "appears" ||
                     iterator->get<std::string>() == "disappears"));
        } else if (iterator.key() == "ply") {
            REQUIRE(iterator->is_number_integer());
            REQUIRE(iterator->get<int>() >= 1);
        } else if (iterator.key() == "file") {
            REQUIRE(iterator->is_string());
            const std::string file = iterator->get<std::string>();
            REQUIRE(file.size() == 1);
            REQUIRE((file.front() >= 'a' && file.front() <= 'h'));
        } else if (iterator.key() == "square") {
            REQUIRE(square_name_is_valid(*iterator));
        } else {
            FAIL("unknown positional use fixture field: " << iterator.key());
        }
    }
}

[[nodiscard]] bool use_change_matches(const PositionalChange& actual, const json& expected) {
    if (expected["tag"].get<std::string>() != chesswhy::to_string(actual.feature.tag) ||
        expected["side"].get<std::string>() != chesswhy::to_string(actual.feature.side) ||
        expected["change"].get<std::string>() != chesswhy::to_string(actual.change) ||
        expected["ply"].get<int>() != actual.ply) {
        return false;
    }
    if (expected.contains("file")) {
        return actual.feature.file &&
               expected["file"].get<std::string>() ==
                   std::string(1, static_cast<char>('a' + *actual.feature.file));
    }
    if (expected.contains("square")) {
        return !actual.feature.pieces.empty() &&
               expected["square"].get<std::string>() ==
                   square_name(actual.feature.pieces.front().square);
    }
    return true;
}

[[nodiscard]] json marked_change_summary(const PositionalChange& change) {
    json result = change_summary(change);
    result["net_change"] = change.net_change;
    result["in_use"] = change.in_use ? json(*change.in_use) : json(nullptr);
    return result;
}

void run_use_fixture(const json& fixture) {
    REQUIRE(fixture.is_object());
    REQUIRE(fixture.size() == 8);
    for (const std::string_view key : {"id", "source", "fen"}) {
        REQUIRE(fixture.contains(key));
        REQUIRE(fixture[std::string(key)].is_string());
    }
    REQUIRE(fixture.contains("tempo_dependent"));
    REQUIRE(fixture["tempo_dependent"].is_boolean());
    REQUIRE(fixture.contains("line_uci"));
    REQUIRE(fixture["line_uci"].is_array());
    for (const std::string_view key : {"kept", "cut", "passing"}) {
        REQUIRE(fixture.contains(key));
        REQUIRE(fixture[std::string(key)].is_array());
    }

    INFO("positional use fixture " << fixture["id"].get<std::string>() << " from "
                                   << fixture["source"].get<std::string>());
    chess::Board board(fixture["fen"].get<std::string>());
    const chess::Board root = board;
    std::vector<chess::Move> line;
    line.reserve(fixture["line_uci"].size());
    for (const json& move_json : fixture["line_uci"]) {
        REQUIRE(move_json.is_string());
        const std::string uci = move_json.get<std::string>();
        REQUIRE((uci.size() == 4 || uci.size() == 5));
        const chess::Move move = chess::uci::uciToMove(board, uci);
        REQUIRE(move != chess::Move::NO_MOVE);
        REQUIRE(board.isLegal(move));
        line.push_back(move);
        board.makeMove(move);
    }
    std::vector<PositionalChange> actual = chesswhy::positional::line_changes(root, line);
    chesswhy::positional::mark_use(actual, root, line, fixture["tempo_dependent"].get<bool>());

    for (const auto& [key, net, used] :
         {std::tuple<std::string_view, bool, bool>{"kept", true, true},
          std::tuple<std::string_view, bool, bool>{"cut", true, false},
          std::tuple<std::string_view, bool, bool>{"passing", false, false}}) {
        for (const json& expected : fixture[std::string(key)]) {
            validate_use_change(expected);
            INFO(key << " " << expected.dump());
            const bool matched = std::ranges::any_of(actual, [&](const PositionalChange& change) {
                const bool use_matches = net ? change.in_use.has_value() && *change.in_use == used
                                             : !change.in_use.has_value();
                return use_change_matches(change, expected) && change.net_change == net &&
                       use_matches;
            });
            if (!matched) {
                json summary = json::array();
                for (const PositionalChange& change : actual) {
                    summary.push_back(marked_change_summary(change));
                }
                INFO("fixture FEN " << fixture["fen"].get<std::string>());
                INFO("fixture line " << fixture["line_uci"].dump());
                INFO("actual marked changes " << summary.dump());
            }
            CHECK(matched);
        }
    }
}

void validate_fixture_row(const json& fixture, bool line_fixture) {
    REQUIRE(fixture.is_object());
    REQUIRE(fixture.size() == (line_fixture ? 8 : 7));
    for (const std::string_view key : {"id", "title", "source", "fen", "reason"}) {
        REQUIRE(fixture.contains(key));
        REQUIRE(fixture[std::string(key)].is_string());
    }
    if (line_fixture) {
        REQUIRE(fixture.contains("line_uci"));
        REQUIRE(fixture["line_uci"].is_array());
        for (const json& move : fixture["line_uci"]) {
            REQUIRE(move.is_string());
        }
    }
    REQUIRE(fixture.contains("include"));
    REQUIRE(fixture["include"].is_array());
    REQUIRE(fixture.contains("exclude"));
    REQUIRE(fixture["exclude"].is_array());
}

void run_fixture_document(const json& root) {
    REQUIRE(root.is_object());
    REQUIRE(root.contains("features"));
    REQUIRE(root["features"].is_array());
    REQUIRE(root.contains("lines"));
    REQUIRE(root["lines"].is_array());
    REQUIRE(root.contains("uses"));
    REQUIRE(root["uses"].is_array());

    for (const json& fixture : root["features"]) {
        validate_fixture_row(fixture, false);
        INFO("positional feature fixture " << fixture["id"].get<std::string>());
        const chess::Board board(fixture["fen"].get<std::string>());
        const std::vector<PositionalFeature> actual = chesswhy::positional::features(board);
        for (const json& expected : fixture["include"]) {
            validate_partial_feature(expected, false);
            INFO("include " << expected.dump());
            const bool matched =
                std::ranges::any_of(actual, [&expected](const PositionalFeature& feature) {
                    return feature_matches(feature, expected);
                });
            if (!matched) {
                INFO("fixture FEN " << fixture["fen"].get<std::string>());
                json summary = json::array();
                for (const PositionalFeature& feature : actual) {
                    summary.push_back(feature_summary(feature));
                }
                INFO("actual features " << summary.dump());
            }
            CHECK(matched);
        }
        for (const json& forbidden : fixture["exclude"]) {
            validate_partial_feature(forbidden, false);
            INFO("exclude " << forbidden.dump());
            CHECK(std::ranges::none_of(actual, [&forbidden](const PositionalFeature& feature) {
                return feature_matches(feature, forbidden);
            }));
        }
    }

    for (const json& fixture : root["lines"]) {
        validate_fixture_row(fixture, true);
        INFO("positional line fixture " << fixture["id"].get<std::string>());
        chess::Board board(fixture["fen"].get<std::string>());
        const chess::Board root_board = board;
        std::vector<chess::Move> line;
        line.reserve(fixture["line_uci"].size());
        for (const json& move_json : fixture["line_uci"]) {
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
        const std::vector<PositionalChange> actual =
            chesswhy::positional::line_changes(root_board, line);
        for (const json& expected : fixture["include"]) {
            validate_partial_feature(expected, true);
            INFO("include " << expected.dump());
            const bool matched =
                std::ranges::any_of(actual, [&expected](const PositionalChange& change) {
                    return change_matches(change, expected);
                });
            if (!matched) {
                INFO("fixture FEN " << fixture["fen"].get<std::string>());
                INFO("fixture line " << fixture["line_uci"].dump());
                json summary = json::array();
                for (const PositionalChange& change : actual) {
                    summary.push_back(change_summary(change));
                }
                INFO("actual changes " << summary.dump());
            }
            CHECK(matched);
        }
        for (const json& forbidden : fixture["exclude"]) {
            validate_partial_feature(forbidden, true);
            INFO("exclude " << forbidden.dump());
            CHECK(std::ranges::none_of(actual, [&forbidden](const PositionalChange& change) {
                return change_matches(change, forbidden);
            }));
        }
    }
    for (const json& fixture : root["uses"]) {
        run_use_fixture(fixture);
    }
}

void run_fixture_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    REQUIRE(input.is_open());
    run_fixture_document(json::parse(input));
}

} // namespace

TEST_CASE("pawn structure handles edge files and both relative directions", "[unit]") {
    const chess::Board board("7k/8/8/8/8/P6P/P6P/4K3 w - - 0 1");
    const auto actual = chesswhy::positional::features(board);
    for (const std::uint8_t file : {std::uint8_t{0}, std::uint8_t{7}}) {
        const PositionalFeature* isolated =
            find_feature(actual, PositionalTag::IsolatedPawn, Side::White, file);
        REQUIRE(isolated != nullptr);
        CHECK(isolated->pieces.size() == 2);
        CHECK(isolated->file == file);
        CHECK(find_feature(actual, PositionalTag::DoubledPawns, Side::White, file) != nullptr);
    }
    const PositionalFeature* white_islands =
        find_feature(actual, PositionalTag::PawnIslands, Side::White);
    REQUIRE(white_islands != nullptr);
    CHECK(white_islands->value == 2);

    const chess::Board black_board("4k3/p6p/p6p/8/8/8/8/7K b - - 0 1");
    const auto black_actual = chesswhy::positional::features(black_board);
    for (const std::uint8_t file : {std::uint8_t{0}, std::uint8_t{7}}) {
        const PositionalFeature* isolated =
            find_feature(black_actual, PositionalTag::IsolatedPawn, Side::Black, file);
        REQUIRE(isolated != nullptr);
        CHECK(isolated->pieces.size() == 2);
        CHECK(find_feature(black_actual, PositionalTag::DoubledPawns, Side::Black, file) !=
              nullptr);
    }
    const PositionalFeature* black_islands =
        find_feature(black_actual, PositionalTag::PawnIslands, Side::Black);
    REQUIRE(black_islands != nullptr);
    CHECK(black_islands->value == 2);
}

TEST_CASE("passed pawns, connected runs, backward pawns and chains follow their boundaries",
          "[unit]") {
    const chess::Board white_chain("7k/8/8/8/4P3/3P4/2P5/K7 w - - 0 1");
    const auto white_features = chesswhy::positional::features(white_chain);
    const PositionalFeature* chain =
        find_feature(white_features, PositionalTag::PawnChain, Side::White);
    REQUIRE(chain != nullptr);
    CHECK(chain->pieces.size() == 3);
    CHECK(chain->pieces[0].square == square("c2").index());
    CHECK(chain->pieces[1].square == square("d3").index());
    CHECK(chain->pieces[2].square == square("e4").index());
    const PositionalFeature* connected =
        find_feature(white_features, PositionalTag::ConnectedPassedPawns, Side::White);
    REQUIRE(connected != nullptr);
    CHECK(connected->pieces.size() == 3);
    const PositionalFeature* islands =
        find_feature(white_features, PositionalTag::PawnIslands, Side::White);
    REQUIRE(islands != nullptr);
    CHECK(islands->value == 1);

    const chess::Board black_chain("k7/2p5/3p4/4p3/8/8/8/7K b - - 0 1");
    const auto black_features = chesswhy::positional::features(black_chain);
    const PositionalFeature* black_chain_feature =
        find_feature(black_features, PositionalTag::PawnChain, Side::Black);
    REQUIRE(black_chain_feature != nullptr);
    CHECK(black_chain_feature->pieces.size() == 3);
    CHECK(black_chain_feature->pieces[0].square == square("c7").index());
    CHECK(black_chain_feature->pieces[1].square == square("d6").index());
    CHECK(black_chain_feature->pieces[2].square == square("e5").index());

    const chess::Board two_pawns("7k/8/8/8/8/3P4/2P5/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(two_pawns), PositionalTag::PawnChain,
                       Side::White) == nullptr);

    const chess::Board front_pawn("7k/8/8/8/P7/8/P7/K7 w - - 0 1");
    const auto front_features = chesswhy::positional::features(front_pawn);
    const PositionalFeature* passed =
        find_feature(front_features, PositionalTag::PassedPawn, Side::White, 0);
    REQUIRE(passed != nullptr);
    const std::vector<PieceOnSquare> expected_passed{
        PieceOnSquare{.square = static_cast<std::uint8_t>(square("a4").index()), .piece = 'P'}};
    CHECK(passed->pieces == expected_passed);

    const chess::Board blocked_passed("7k/8/1p6/8/P7/8/P7/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(blocked_passed), PositionalTag::PassedPawn,
                       Side::White, 0) == nullptr);

    const chess::Board white_backward("7k/8/3p4/1P6/2P5/8/8/K7 w - - 0 1");
    const auto backward_features = chesswhy::positional::features(white_backward);
    const PositionalFeature* backward =
        find_feature(backward_features, PositionalTag::BackwardPawn, Side::White, 2);
    REQUIRE(backward != nullptr);
    CHECK(backward->pieces.size() == 1);
    CHECK(backward->pieces.front().square == square("c4").index());
    const chess::Board white_backward_near_miss("7k/8/8/1P6/2P5/8/8/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(white_backward_near_miss),
                       PositionalTag::BackwardPawn, Side::White, 2) == nullptr);

    const chess::Board black_backward("k7/8/8/2p5/1p6/3P4/8/7K b - - 0 1");
    const auto black_backward_features = chesswhy::positional::features(black_backward);
    const PositionalFeature* black_backward_feature =
        find_feature(black_backward_features, PositionalTag::BackwardPawn, Side::Black, 2);
    REQUIRE(black_backward_feature != nullptr);
    CHECK(black_backward_feature->pieces.front().square == square("c5").index());
}

TEST_CASE("king files use both edges and relative shield ranks", "[unit]") {
    const chess::Board white_edge("7k/8/1p6/8/8/6P1/8/K7 w - - 0 1");
    const auto white_features = chesswhy::positional::features(white_edge);
    CHECK(find_feature(white_features, PositionalTag::KingShieldGap, Side::White, 0) != nullptr);
    CHECK(find_feature(white_features, PositionalTag::KingShieldGap, Side::White, 1) != nullptr);
    CHECK(find_feature(white_features, PositionalTag::OpenFileNearKing, Side::White, 0) != nullptr);
    CHECK(find_feature(white_features, PositionalTag::HalfOpenFileNearKing, Side::White, 1) !=
          nullptr);

    CHECK(find_feature(white_features, PositionalTag::KingShieldGap, Side::Black, 6) != nullptr);
    CHECK(find_feature(white_features, PositionalTag::KingShieldGap, Side::Black, 7) != nullptr);
    CHECK(find_feature(white_features, PositionalTag::HalfOpenFileNearKing, Side::Black, 6) !=
          nullptr);
    CHECK(find_feature(white_features, PositionalTag::OpenFileNearKing, Side::Black, 7) != nullptr);

    const chess::Board shielded("7k/8/8/8/8/8/1P6/K7 w - - 0 1");
    const auto shielded_features = chesswhy::positional::features(shielded);
    CHECK(find_feature(shielded_features, PositionalTag::KingShieldGap, Side::White, 0) != nullptr);
    CHECK(find_feature(shielded_features, PositionalTag::KingShieldGap, Side::White, 1) == nullptr);

    const chess::Board black_shielded("7k/6p1/8/8/8/8/8/K7 b - - 0 1");
    const auto black_shielded_features = chesswhy::positional::features(black_shielded);
    CHECK(find_feature(black_shielded_features, PositionalTag::KingShieldGap, Side::Black, 6) ==
          nullptr);
    CHECK(find_feature(black_shielded_features, PositionalTag::KingShieldGap, Side::Black, 7) !=
          nullptr);

    const chess::Board king_on_third("7k/8/8/8/8/K7/8/8 w - - 0 1");
    CHECK(count_features(chesswhy::positional::features(king_on_third),
                         PositionalTag::KingShieldGap, Side::White) == 0);
    const chess::Board white_king_second_rank("7k/8/8/8/8/8/K7/8 w - - 0 1");
    CHECK(count_features(chesswhy::positional::features(white_king_second_rank),
                         PositionalTag::KingShieldGap, Side::White) == 2);
    const chess::Board black_king_second_rank("8/7k/8/8/8/8/8/K7 b - - 0 1");
    CHECK(count_features(chesswhy::positional::features(black_king_second_rank),
                         PositionalTag::KingShieldGap, Side::Black) == 2);
}

TEST_CASE("castling, king-zone attackers and rook captures are tracked", "[unit]") {
    const chess::Board rights("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    const auto right_features = chesswhy::positional::features(rights);
    CHECK(find_feature(right_features, PositionalTag::CastlingKingside, Side::White) != nullptr);
    CHECK(find_feature(right_features, PositionalTag::CastlingQueenside, Side::White) != nullptr);
    CHECK(find_feature(right_features, PositionalTag::CastlingKingside, Side::Black) != nullptr);
    CHECK(find_feature(right_features, PositionalTag::CastlingQueenside, Side::Black) != nullptr);

    const chess::Board rook_attacks_king("r6k/8/8/8/8/8/8/K7 w - - 0 1");
    const auto attack_features = chesswhy::positional::features(rook_attacks_king);
    const PositionalFeature* attacker =
        find_feature(attack_features, PositionalTag::KingZoneAttacker, Side::White);
    REQUIRE(attacker != nullptr);
    CHECK(attacker->pieces.size() == 1);
    CHECK(attacker->pieces.front().square == square("a8").index());
    CHECK(attacker->pieces.front().piece == 'r');

    const chess::Board pinned_knight("4k3/4n3/8/6K1/8/8/8/4R3 w - - 0 1");
    const auto pinned_features = chesswhy::positional::features(pinned_knight);
    const PositionalFeature* pinned_attacker =
        find_feature(pinned_features, PositionalTag::KingZoneAttacker, Side::White);
    REQUIRE(pinned_attacker != nullptr);
    CHECK(pinned_attacker->pieces.size() == 1);
    CHECK(pinned_attacker->pieces.front().square == square("e7").index());
    CHECK(pinned_attacker->pieces.front().piece == 'n');

    const chess::Board capture_root("k7/8/8/8/8/8/7r/4K2R b K - 0 1");
    chess::Board after = capture_root;
    const std::vector<chess::Move> capture_line = parse_line(after, {"h2h1"});
    const std::vector<PositionalChange> changes =
        chesswhy::positional::line_changes(capture_root, capture_line);
    const auto lost_right = std::ranges::find_if(changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::CastlingKingside &&
               change.feature.side == Side::White;
    });
    REQUIRE(lost_right != changes.end());
    CHECK(lost_right->change == PositionalChangeKind::Disappears);
    CHECK(lost_right->ply == 1);
    CHECK(lost_right->holds_at_end);
}

TEST_CASE("piece features use their square, file, and colour rules", "[unit]") {
    const chess::Board pieces("7k/8/8/8/8/8/8/1BB1K3 w - - 0 1");
    const auto piece_features = chesswhy::positional::features(pieces);
    const PositionalFeature* pair =
        find_feature(piece_features, PositionalTag::BishopPair, Side::White);
    REQUIRE(pair != nullptr);
    CHECK(pair->pieces.size() == 2);
    CHECK(pair->pieces[0].square == square("b1").index());
    CHECK(pair->pieces[1].square == square("c1").index());

    const chess::Board rooks_open("7k/R7/8/8/8/8/8/K7 w - - 0 1");
    const auto open_features = chesswhy::positional::features(rooks_open);
    CHECK(find_feature(open_features, PositionalTag::RookOpenFile, Side::White, 0) != nullptr);
    CHECK(find_feature(open_features, PositionalTag::RookSeventh, Side::White) != nullptr);

    const chess::Board rooks_half_open("7k/R7/8/8/p7/8/8/K7 w - - 0 1");
    const auto half_open_features = chesswhy::positional::features(rooks_half_open);
    CHECK(find_feature(half_open_features, PositionalTag::RookHalfOpenFile, Side::White, 0) !=
          nullptr);
    CHECK(find_feature(half_open_features, PositionalTag::RookOpenFile, Side::White, 0) == nullptr);

    const chess::Board black_seventh("k7/8/8/8/8/8/r7/7K b - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(black_seventh), PositionalTag::RookSeventh,
                       Side::Black) != nullptr);
    const chess::Board rook_seventh_for_pawn("8/Rp5k/8/8/8/8/8/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(rook_seventh_for_pawn),
                       PositionalTag::RookSeventh, Side::White) != nullptr);
    const chess::Board white_rook_sixth("7k/8/R7/8/8/8/8/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(white_rook_sixth), PositionalTag::RookSeventh,
                       Side::White) == nullptr);

    const chess::Board outposts("7k/4N3/3PN3/3P4/4N3/3PN3/3P4/K7 w - - 0 1");
    const auto outpost_features = chesswhy::positional::features(outposts);
    CHECK(find_feature(outpost_features, PositionalTag::Outpost, Side::White) != nullptr);
    CHECK(count_features(outpost_features, PositionalTag::Outpost, Side::White) == 2);

    const chess::Board challenged_outpost("7k/8/8/3p4/4N3/3P4/8/K7 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(challenged_outpost), PositionalTag::Outpost,
                       Side::White) == nullptr);

    const chess::Board black_outposts("7k/3p4/3pn3/4n3/3p4/3pn3/4n3/K7 b - - 0 1");
    const auto black_outpost_features = chesswhy::positional::features(black_outposts);
    CHECK(count_features(black_outpost_features, PositionalTag::Outpost, Side::Black) == 2);
    const chess::Board bad_bishop("7k/8/8/8/8/p1p5/P1P5/1B2K3 w - - 0 1");
    const auto bad_bishop_features = chesswhy::positional::features(bad_bishop);
    const PositionalFeature* bad =
        find_feature(bad_bishop_features, PositionalTag::BadBishop, Side::White);
    REQUIRE(bad != nullptr);
    CHECK(bad->pieces.size() == 3);
    CHECK(bad->pieces[0].square == square("b1").index());
    CHECK(bad->pieces[1].square == square("a2").index());
    CHECK(bad->pieces[2].square == square("c2").index());

    const chess::Board one_blocked_pawn("7k/8/8/8/8/p7/P1P5/1B2K3 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(one_blocked_pawn), PositionalTag::BadBishop,
                       Side::White) == nullptr);
    const chess::Board tie_color_count("7k/8/8/8/8/p1p5/PPPP4/1B2K3 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(tie_color_count), PositionalTag::BadBishop,
                       Side::White) == nullptr);

    const chess::Board exchange_up("r3k3/1n6/8/8/8/8/8/RR2K3 w - - 0 1");
    CHECK(find_feature(chesswhy::positional::features(exchange_up), PositionalTag::ExchangeUp,
                       Side::White) != nullptr);
}

TEST_CASE("positional identity follows each tag's identity column", "[unit]") {
    const auto piece = [](std::string_view square_value, char letter) {
        return PieceOnSquare{.square = static_cast<std::uint8_t>(square(square_value).index()),
                             .piece = letter};
    };
    const auto make_feature = [](PositionalTag tag, Side side,
                                 const std::vector<PieceOnSquare>& pieces,
                                 std::optional<std::uint8_t> file, std::optional<int> value) {
        PositionalFeature result;
        result.tag = tag;
        result.side = side;
        result.pieces = pieces;
        result.file = file;
        result.value = value;
        return result;
    };

    CHECK(chesswhy::positional::same_identity(
        make_feature(PositionalTag::IsolatedPawn, Side::White, {piece("a2", 'P')}, 0, std::nullopt),
        make_feature(PositionalTag::IsolatedPawn, Side::White, {piece("a4", 'P')}, 0,
                     std::nullopt)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::IsolatedPawn, Side::White, {piece("a2", 'P')}, 0, std::nullopt),
        make_feature(PositionalTag::IsolatedPawn, Side::Black, {piece("a4", 'p')}, 0,
                     std::nullopt)));
    CHECK(chesswhy::positional::same_identity(
        make_feature(PositionalTag::ConnectedPassedPawns, Side::White,
                     {piece("c2", 'P'), piece("d3", 'P')}, std::nullopt, std::nullopt),
        make_feature(PositionalTag::ConnectedPassedPawns, Side::White,
                     {piece("c4", 'P'), piece("d5", 'P')}, std::nullopt, std::nullopt)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::ConnectedPassedPawns, Side::White,
                     {piece("c2", 'P'), piece("d3", 'P')}, std::nullopt, std::nullopt),
        make_feature(PositionalTag::ConnectedPassedPawns, Side::White,
                     {piece("c4", 'P'), piece("e5", 'P')}, std::nullopt, std::nullopt)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::PawnIslands, Side::White, {}, std::nullopt, 1),
        make_feature(PositionalTag::PawnIslands, Side::White, {}, std::nullopt, 2)));
    CHECK(chesswhy::positional::same_identity(
        make_feature(PositionalTag::PawnChain, Side::White,
                     {piece("c2", 'P'), piece("d3", 'P'), piece("e4", 'P')}, std::nullopt,
                     std::nullopt),
        make_feature(PositionalTag::PawnChain, Side::White,
                     {piece("c2", 'Q'), piece("d3", 'Q'), piece("e4", 'Q')}, std::nullopt,
                     std::nullopt)));
    CHECK(chesswhy::positional::same_identity(
        make_feature(PositionalTag::BadBishop, Side::White, {piece("a1", 'B'), piece("a2", 'P')},
                     std::nullopt, std::nullopt),
        make_feature(PositionalTag::BadBishop, Side::White, {piece("c3", 'B'), piece("b4", 'P')},
                     std::nullopt, std::nullopt)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::BadBishop, Side::White, {piece("a1", 'B')}, std::nullopt,
                     std::nullopt),
        make_feature(PositionalTag::BadBishop, Side::White, {piece("b1", 'B')}, std::nullopt,
                     std::nullopt)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::MaterialAhead, Side::White, {}, std::nullopt, 1),
        make_feature(PositionalTag::MaterialAhead, Side::White, {}, std::nullopt, 2)));
    CHECK(chesswhy::positional::same_identity(
        make_feature(PositionalTag::MobilityLead, Side::White, {}, std::nullopt, 10),
        make_feature(PositionalTag::MobilityLead, Side::White, {}, std::nullopt, 15)));
    CHECK_FALSE(chesswhy::positional::same_identity(
        make_feature(PositionalTag::Outpost, Side::White, {piece("e4", 'N')}, std::nullopt,
                     std::nullopt),
        make_feature(PositionalTag::Outpost, Side::White, {piece("e4", 'B')}, std::nullopt,
                     std::nullopt)));
}

TEST_CASE("mobility, space, and development leads include thresholds but not one below", "[unit]") {
    const chess::Board white_mobility_10("7k/8/8/8/8/8/2p1p3/R6K w - - 0 1");
    const chess::Board white_mobility_9("7k/8/8/8/8/1p6/2p1p3/R6K w - - 0 1");
    CHECK(chesswhy::positional::mobility(white_mobility_10, chess::Color::WHITE) == 10);
    CHECK(chesswhy::positional::mobility(white_mobility_9, chess::Color::WHITE) == 9);
    const auto white_mobility_features = chesswhy::positional::features(white_mobility_10);
    const PositionalFeature* white_mobility_lead =
        find_feature(white_mobility_features, PositionalTag::MobilityLead, Side::White);
    REQUIRE(white_mobility_lead != nullptr);
    CHECK(white_mobility_lead->value == 10);
    CHECK(find_feature(chesswhy::positional::features(white_mobility_9),
                       PositionalTag::MobilityLead, Side::White) == nullptr);

    const chess::Board black_mobility_10("r6k/2P1P3/8/8/8/8/8/7K b - - 0 1");
    const chess::Board black_mobility_9("r6k/2P1P3/1P6/8/8/8/8/7K b - - 0 1");
    CHECK(chesswhy::positional::mobility(black_mobility_10, chess::Color::BLACK) == 10);
    CHECK(chesswhy::positional::mobility(black_mobility_9, chess::Color::BLACK) == 9);
    CHECK(find_feature(chesswhy::positional::features(black_mobility_10),
                       PositionalTag::MobilityLead, Side::Black) != nullptr);
    CHECK(find_feature(chesswhy::positional::features(black_mobility_9),
                       PositionalTag::MobilityLead, Side::Black) == nullptr);

    const chess::Board white_space_3("7k/8/8/2P5/8/8/8/4K3 w - - 0 1");
    const chess::Board white_space_2("7k/8/8/2P5/8/1p6/8/4K3 w - - 0 1");
    CHECK(chesswhy::positional::space(white_space_3, chess::Color::WHITE) == 3);
    CHECK(chesswhy::positional::space(white_space_2, chess::Color::WHITE) == 2);
    const auto white_space_features = chesswhy::positional::features(white_space_3);
    const PositionalFeature* white_space_lead =
        find_feature(white_space_features, PositionalTag::SpaceLead, Side::White);
    CHECK(white_space_lead->value == 3);
    CHECK(find_feature(chesswhy::positional::features(white_space_2), PositionalTag::SpaceLead,
                       Side::White) == nullptr);

    const chess::Board black_space_3("4k3/8/8/8/2p5/8/8/7K b - - 0 1");
    const chess::Board black_space_2("4k3/8/1P6/8/2p5/8/8/7K b - - 0 1");
    CHECK(chesswhy::positional::space(black_space_3, chess::Color::BLACK) == 3);
    CHECK(chesswhy::positional::space(black_space_2, chess::Color::BLACK) == 2);
    CHECK(find_feature(chesswhy::positional::features(black_space_3), PositionalTag::SpaceLead,
                       Side::Black) != nullptr);
    CHECK(find_feature(chesswhy::positional::features(black_space_2), PositionalTag::SpaceLead,
                       Side::Black) == nullptr);

    const chess::Board white_development_2("1n2k1n1/8/8/8/8/8/8/4K3 w - - 0 1");
    const chess::Board white_development_1("1n2k3/8/8/8/8/8/8/4K3 w - - 0 1");
    CHECK(chesswhy::positional::undeveloped(white_development_2, chess::Color::BLACK) == 2);
    CHECK(chesswhy::positional::undeveloped(white_development_1, chess::Color::BLACK) == 1);
    const auto white_development_features = chesswhy::positional::features(white_development_2);
    const PositionalFeature* white_development_lead =
        find_feature(white_development_features, PositionalTag::DevelopmentLead, Side::White);
    REQUIRE(white_development_lead != nullptr);
    CHECK(white_development_lead->value == 2);
    CHECK(find_feature(chesswhy::positional::features(white_development_1),
                       PositionalTag::DevelopmentLead, Side::White) == nullptr);

    const chess::Board black_development_2("4k3/8/8/8/8/8/8/1N2K1N1 b - - 0 1");
    const chess::Board black_development_1("4k3/8/8/8/8/8/8/1N2K3 b - - 0 1");
    CHECK(chesswhy::positional::undeveloped(black_development_2, chess::Color::WHITE) == 2);
    CHECK(chesswhy::positional::undeveloped(black_development_1, chess::Color::WHITE) == 1);
    CHECK(find_feature(chesswhy::positional::features(black_development_2),
                       PositionalTag::DevelopmentLead, Side::Black) != nullptr);
    CHECK(find_feature(chesswhy::positional::features(black_development_1),
                       PositionalTag::DevelopmentLead, Side::Black) == nullptr);
}

TEST_CASE("promotions, en passant and identity persistence are reflected along a line", "[unit]") {
    const chess::Board promotion_root("7k/P7/8/8/8/8/8/7K w - - 0 1");
    chess::Board promoted = promotion_root;
    const std::vector<chess::Move> promotion_line = parse_line(promoted, {"a7a8q"});
    const auto promoted_features = chesswhy::positional::features(promoted);
    const PositionalFeature* material_ahead =
        find_feature(promoted_features, PositionalTag::MaterialAhead, Side::White);
    REQUIRE(material_ahead != nullptr);
    CHECK(material_ahead->value == 9);
    CHECK(find_feature(promoted_features, PositionalTag::PassedPawn, Side::White) == nullptr);
    const auto promotion_changes =
        chesswhy::positional::line_changes(promotion_root, promotion_line);
    CHECK(std::ranges::any_of(promotion_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White &&
               change.change == PositionalChangeKind::Disappears;
    }));
    CHECK(std::ranges::any_of(promotion_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::MaterialAhead &&
               change.feature.side == Side::White && change.feature.value == 9 &&
               change.change == PositionalChangeKind::Appears;
    }));

    const chess::Board en_passant_root("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    chess::Board en_passant_board = en_passant_root;
    const std::vector<chess::Move> en_passant_line = parse_line(en_passant_board, {"e5d6"});
    const auto en_passant_changes =
        chesswhy::positional::line_changes(en_passant_root, en_passant_line);
    CHECK(std::ranges::any_of(en_passant_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::Black &&
               change.change == PositionalChangeKind::Disappears;
    }));
    CHECK(std::ranges::any_of(en_passant_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White && change.feature.file == 3 &&
               change.change == PositionalChangeKind::Appears;
    }));

    const chess::Board advance_root("7k/8/8/8/8/8/P7/7K w - - 0 1");
    chess::Board advancing = advance_root;
    const std::vector<chess::Move> advance_line = parse_line(advancing, {"a2a3", "h8g8", "a3a4"});
    const auto advance_changes = chesswhy::positional::line_changes(advance_root, advance_line);
    CHECK(std::ranges::none_of(advance_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White;
    }));

    const chess::Board mobility_root("7k/8/8/1p6/1p6/1p1N2p1/2p5/R1P4K b - - 0 1");
    chess::Board mobility_after = mobility_root;
    const std::vector<chess::Move> mobility_line = parse_line(mobility_after, {"b3b2"});
    CHECK(chesswhy::positional::mobility(mobility_root, chess::Color::WHITE) == 10);
    CHECK(chesswhy::positional::mobility(mobility_after, chess::Color::WHITE) == 11);
    const auto mobility_changes = chesswhy::positional::line_changes(mobility_root, mobility_line);
    CHECK(std::ranges::none_of(mobility_changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::MobilityLead &&
               change.feature.side == Side::White;
    }));
}

TEST_CASE("line changes compare identities and hold at the final position", "[unit]") {
    const chess::Board root("7k/8/8/8/8/1p1p1p2/P1P1P3/7K w - - 0 1");
    chess::Board walked = root;
    const std::vector<chess::Move> line = parse_line(walked, {"a2b3", "d3c2", "e2f3"});
    const std::vector<PositionalChange> changes = chesswhy::positional::line_changes(root, line);
    std::vector<const PositionalChange*> material_changes;
    for (const PositionalChange& change : changes) {
        if (change.feature.tag == PositionalTag::MaterialAhead &&
            change.feature.side == Side::White && change.feature.value == 1) {
            material_changes.push_back(&change);
        }
    }
    REQUIRE(material_changes.size() == 3);
    CHECK(material_changes[0]->ply == 1);
    CHECK(material_changes[0]->change == PositionalChangeKind::Appears);
    CHECK(material_changes[0]->holds_at_end);
    CHECK(material_changes[1]->ply == 2);
    CHECK(material_changes[1]->change == PositionalChangeKind::Disappears);
    CHECK_FALSE(material_changes[1]->holds_at_end);
    CHECK(material_changes[2]->ply == 3);
    CHECK(material_changes[2]->change == PositionalChangeKind::Appears);
    CHECK(material_changes[2]->holds_at_end);

    for (std::size_t index = 1; index < changes.size(); ++index) {
        const PositionalChange& previous = changes[index - 1];
        const PositionalChange& current = changes[index];
        CHECK(previous.ply <= current.ply);
        if (previous.ply == current.ply) {
            CHECK((previous.change != PositionalChangeKind::Disappears ||
                   current.change != PositionalChangeKind::Appears));
        }
    }
}

TEST_CASE("positional P fixtures exercise every include and exclude", "[unit]") {
    const auto path =
        std::filesystem::path(CHESSWHY_SOURCE_DIR) / "tests" / "fixtures" / "positional.json";
    run_fixture_file(path);
}

TEST_CASE("positional fixture matching handles locally constructed rows", "[unit]") {
    const json feature_fixture{
        {"id", "TEMP-P01"},
        {"title", "single edge pawn"},
        {"source", "constructed"},
        {"fen", "7k/8/8/8/8/8/P7/K7 w - - 0 1"},
        {"include",
         json::array({json{{"tag", "isolated_pawn"},
                           {"side", "white"},
                           {"file", "a"},
                           {"pieces", json::array({json{{"square", "a2"}, {"piece", "P"}}})}}})},
        {"exclude",
         json::array({json{{"tag", "doubled_pawns"}, {"side", "white"}, {"file", "a"}}})},
        {"reason", "the single pawn is isolated but not doubled"},
    };
    const json line_fixture{
        {"id", "TEMP-PL01"},
        {"title", "capturing the rook removes castling rights"},
        {"source", "constructed"},
        {"fen", "k7/8/8/8/8/8/7r/4K2R b K - 0 1"},
        {"line_uci", json::array({"h2h1"})},
        {"include", json::array({json{{"ply", 1},
                                      {"change", "disappears"},
                                      {"holds_at_end", true},
                                      {"tag", "castling_kingside"},
                                      {"side", "white"}}})},
        {"exclude",
         json::array({json{
             {"ply", 1}, {"change", "appears"}, {"tag", "castling_kingside"}, {"side", "white"}}})},
        {"reason", "the captured rook no longer supports the castling right"},
    };
    run_fixture_document(json{{"features", json::array({feature_fixture})},
                              {"lines", json::array({line_fixture})},
                              {"uses", json::array()}});
}

TEST_CASE("captured positional features record capture and favour semantics", "[unit]") {
    const chess::Board isolated_root("3r2k1/8/2n5/8/3P4/2N5/8/R5K1 b - - 0 1");
    chess::Board isolated_board = isolated_root;
    const std::vector<chess::Move> isolated_line = parse_line(isolated_board, {"d8d4"});
    const auto isolated_changes = chesswhy::positional::line_changes(isolated_root, isolated_line);
    const auto isolated =
        std::ranges::find_if(isolated_changes, [](const PositionalChange& change) {
            return change.ply == 1 && change.change == PositionalChangeKind::Disappears &&
                   change.feature.tag == PositionalTag::IsolatedPawn &&
                   change.feature.side == Side::White && change.feature.file == 3;
        });
    REQUIRE(isolated != isolated_changes.end());
    CHECK(isolated->captured);
    CHECK_FALSE(chesswhy::positional::favours(*isolated).has_value());

    const chess::Board passer_root("7k/8/8/3p4/8/8/8/3R2K1 w - - 0 1");
    chess::Board passer_board = passer_root;
    const std::vector<chess::Move> passer_line = parse_line(passer_board, {"d1d5"});
    const auto passer_changes = chesswhy::positional::line_changes(passer_root, passer_line);
    const auto captured_passer =
        std::ranges::find_if(passer_changes, [](const PositionalChange& change) {
            return change.ply == 1 && change.change == PositionalChangeKind::Disappears &&
                   change.feature.tag == PositionalTag::PassedPawn &&
                   change.feature.side == Side::Black && change.feature.file == 3;
        });
    REQUIRE(captured_passer != passer_changes.end());
    CHECK(captured_passer->captured);
    CHECK(chesswhy::positional::favours(*captured_passer) == Side::White);

    const chess::Board en_passant_root("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    chess::Board en_passant_board = en_passant_root;
    const std::vector<chess::Move> en_passant_line = parse_line(en_passant_board, {"e5d6"});
    const auto en_passant_changes =
        chesswhy::positional::line_changes(en_passant_root, en_passant_line);
    const auto en_passant_passer =
        std::ranges::find_if(en_passant_changes, [](const PositionalChange& change) {
            return change.ply == 1 && change.change == PositionalChangeKind::Disappears &&
                   change.feature.tag == PositionalTag::PassedPawn &&
                   change.feature.side == Side::Black && change.feature.file == 3;
        });
    REQUIRE(en_passant_passer != en_passant_changes.end());
    CHECK(en_passant_passer->captured);
    CHECK(chesswhy::positional::favours(*en_passant_passer) == Side::White);
}

TEST_CASE("favoured side follows change direction and weakness polarity", "[unit]") {
    constexpr std::array weak_tags{
        PositionalTag::IsolatedPawn,
        PositionalTag::DoubledPawns,
        PositionalTag::TripledPawns,
        PositionalTag::BackwardPawn,
        PositionalTag::KingShieldGap,
        PositionalTag::OpenFileNearKing,
        PositionalTag::HalfOpenFileNearKing,
        PositionalTag::KingZoneAttacker,
        PositionalTag::BadBishop,
    };
    constexpr std::array neutral_tags{PositionalTag::PawnIslands, PositionalTag::PawnChain};
    constexpr std::array other_tags{
        PositionalTag::PassedPawn,       PositionalTag::ConnectedPassedPawns,
        PositionalTag::CastlingKingside, PositionalTag::CastlingQueenside,
        PositionalTag::BishopPair,       PositionalTag::RookOpenFile,
        PositionalTag::RookHalfOpenFile, PositionalTag::RookSeventh,
        PositionalTag::Outpost,          PositionalTag::MobilityLead,
        PositionalTag::SpaceLead,        PositionalTag::DevelopmentLead,
        PositionalTag::MaterialAhead,    PositionalTag::ExchangeUp,
    };

    for (const PositionalTag tag : weak_tags) {
        PositionalChange change;
        change.feature.tag = tag;
        change.feature.side = Side::Black;
        change.change = PositionalChangeKind::Appears;
        CHECK(chesswhy::positional::favours(change) == Side::White);
        change.change = PositionalChangeKind::Disappears;
        CHECK(chesswhy::positional::favours(change) == Side::Black);
        change.captured = true;
        CHECK_FALSE(chesswhy::positional::favours(change).has_value());
    }
    for (const PositionalTag tag : other_tags) {
        PositionalChange change;
        change.feature.tag = tag;
        change.feature.side = Side::Black;
        change.change = PositionalChangeKind::Appears;
        CHECK(chesswhy::positional::favours(change) == Side::Black);
        change.change = PositionalChangeKind::Disappears;
        CHECK(chesswhy::positional::favours(change) == Side::White);
    }
    for (const PositionalTag tag : neutral_tags) {
        PositionalChange change;
        change.feature.tag = tag;
        change.feature.side = Side::Black;
        CHECK_FALSE(chesswhy::positional::favours(change).has_value());
        change.change = PositionalChangeKind::Disappears;
        CHECK_FALSE(chesswhy::positional::favours(change).has_value());
    }
}

TEST_CASE("static scale groups and dynamic tags follow the contracted tables", "[unit]") {
    constexpr std::array king_safety{
        PositionalTag::CastlingKingside,     PositionalTag::CastlingQueenside,
        PositionalTag::KingShieldGap,        PositionalTag::OpenFileNearKing,
        PositionalTag::HalfOpenFileNearKing, PositionalTag::KingZoneAttacker,
    };
    constexpr std::array material_tags{
        PositionalTag::MaterialAhead,
        PositionalTag::ExchangeUp,
        PositionalTag::BishopPair,
    };
    constexpr std::array pawn_structure{
        PositionalTag::IsolatedPawn, PositionalTag::DoubledPawns,
        PositionalTag::TripledPawns, PositionalTag::BackwardPawn,
        PositionalTag::PassedPawn,   PositionalTag::ConnectedPassedPawns,
        PositionalTag::PawnIslands,  PositionalTag::PawnChain,
    };
    constexpr std::array pieces{
        PositionalTag::RookOpenFile, PositionalTag::RookHalfOpenFile, PositionalTag::RookSeventh,
        PositionalTag::Outpost,      PositionalTag::BadBishop,
    };
    constexpr std::array space_mobility_development{
        PositionalTag::MobilityLead,
        PositionalTag::SpaceLead,
        PositionalTag::DevelopmentLead,
    };
    constexpr std::array all_tags{
        PositionalTag::IsolatedPawn,
        PositionalTag::DoubledPawns,
        PositionalTag::TripledPawns,
        PositionalTag::BackwardPawn,
        PositionalTag::PassedPawn,
        PositionalTag::ConnectedPassedPawns,
        PositionalTag::PawnIslands,
        PositionalTag::PawnChain,
        PositionalTag::CastlingKingside,
        PositionalTag::CastlingQueenside,
        PositionalTag::KingShieldGap,
        PositionalTag::OpenFileNearKing,
        PositionalTag::HalfOpenFileNearKing,
        PositionalTag::KingZoneAttacker,
        PositionalTag::BishopPair,
        PositionalTag::RookOpenFile,
        PositionalTag::RookHalfOpenFile,
        PositionalTag::RookSeventh,
        PositionalTag::Outpost,
        PositionalTag::BadBishop,
        PositionalTag::MobilityLead,
        PositionalTag::SpaceLead,
        PositionalTag::DevelopmentLead,
        PositionalTag::MaterialAhead,
        PositionalTag::ExchangeUp,
    };
    for (const PositionalTag tag : king_safety) {
        CHECK(chesswhy::positional::static_scale_group(tag) ==
              chesswhy::positional::StaticScaleGroup::KingSafety);
    }
    for (const PositionalTag tag : material_tags) {
        CHECK(chesswhy::positional::static_scale_group(tag) ==
              chesswhy::positional::StaticScaleGroup::Material);
    }
    for (const PositionalTag tag : pawn_structure) {
        CHECK(chesswhy::positional::static_scale_group(tag) ==
              chesswhy::positional::StaticScaleGroup::PawnStructure);
    }
    for (const PositionalTag tag : pieces) {
        CHECK(chesswhy::positional::static_scale_group(tag) ==
              chesswhy::positional::StaticScaleGroup::Pieces);
    }
    for (const PositionalTag tag : space_mobility_development) {
        CHECK(chesswhy::positional::static_scale_group(tag) ==
              chesswhy::positional::StaticScaleGroup::SpaceMobilityDevelopment);
    }
    constexpr std::array dynamic_tags{
        PositionalTag::CastlingKingside, PositionalTag::CastlingQueenside,
        PositionalTag::KingZoneAttacker, PositionalTag::MobilityLead,
        PositionalTag::DevelopmentLead,
    };
    for (const PositionalTag tag : all_tags) {
        CHECK(chesswhy::positional::is_dynamic(tag) ==
              (std::ranges::find(dynamic_tags, tag) != dynamic_tags.end()));
    }
}

TEST_CASE("positional impact order uses both mode orders and preserves ties", "[unit]") {
    const auto make_change = [](PositionalTag tag) {
        PositionalChange change;
        change.feature.tag = tag;
        return change;
    };
    const auto tags_of = [](const std::vector<PositionalChange>& changes) {
        std::vector<PositionalTag> tags;
        tags.reserve(changes.size());
        for (const PositionalChange& change : changes) {
            tags.push_back(change.feature.tag);
        }
        return tags;
    };
    const std::vector<PositionalChange> original{
        make_change(PositionalTag::MobilityLead),  make_change(PositionalTag::KingZoneAttacker),
        make_change(PositionalTag::MaterialAhead), make_change(PositionalTag::DoubledPawns),
        make_change(PositionalTag::BishopPair),    make_change(PositionalTag::CastlingKingside),
        make_change(PositionalTag::Outpost),       make_change(PositionalTag::DevelopmentLead),
        make_change(PositionalTag::SpaceLead),
    };
    std::vector<PositionalChange> tempo = original;
    chesswhy::positional::impact_order(tempo, true);
    CHECK((tags_of(tempo) ==
           std::vector<PositionalTag>{PositionalTag::KingZoneAttacker,
                                      PositionalTag::CastlingKingside, PositionalTag::MobilityLead,
                                      PositionalTag::DevelopmentLead, PositionalTag::MaterialAhead,
                                      PositionalTag::BishopPair, PositionalTag::DoubledPawns,
                                      PositionalTag::Outpost, PositionalTag::SpaceLead}));

    std::vector<PositionalChange> non_tempo = original;
    chesswhy::positional::impact_order(non_tempo, false);
    CHECK((tags_of(non_tempo) ==
           std::vector<PositionalTag>{PositionalTag::MaterialAhead, PositionalTag::BishopPair,
                                      PositionalTag::DoubledPawns, PositionalTag::Outpost,
                                      PositionalTag::SpaceLead, PositionalTag::KingZoneAttacker,
                                      PositionalTag::CastlingKingside, PositionalTag::MobilityLead,
                                      PositionalTag::DevelopmentLead}));
}
TEST_CASE("king-file pressure requires an on-file shelter witness", "[unit]") {
    const auto check_king_file = [](std::string_view fen,
                                    std::initializer_list<std::string_view> uci_line,
                                    int change_ply, bool expected_use) {
        const chess::Board root(fen);
        chess::Board board = root;
        const std::vector<chess::Move> line = parse_line(board, uci_line);
        std::vector<PositionalChange> changes = chesswhy::positional::line_changes(root, line);
        chesswhy::positional::mark_use(changes, root, line, false);
        const auto change =
            std::ranges::find_if(changes, [change_ply](const PositionalChange& item) {
                return item.feature.tag == PositionalTag::KingShieldGap &&
                       item.feature.side == Side::White && item.feature.file == 4 &&
                       item.ply == change_ply && item.change == PositionalChangeKind::Appears;
            });
        REQUIRE(change != changes.end());
        CHECK(change->net_change);
        REQUIRE(change->in_use.has_value());
        CHECK(*change->in_use == expected_use);
    };

    check_king_file("k3r3/8/8/8/8/8/4P3/4K2R w K - 0 1", {"e2e4", "e8e4"}, 1, true);
    check_king_file("k2r4/8/8/8/8/8/3NP3/4K2R w K - 0 1", {"e2e4", "d8d2"}, 1, false);
    check_king_file("k7/8/8/8/8/2n3n1/4P3/4K2R w K - 0 1", {"e2e4", "a8b8"}, 1, true);
    check_king_file("k7/8/8/7q/8/4P3/P7/4K3 w - - 0 1", {"a2a3", "a8b8", "e3e4", "h5e2"}, 3, true);
    check_king_file("k7/8/8/8/5n2/4P3/P3N3/4K3 w - - 0 1", {"a2a3", "a8b8", "e3e4", "f4e2"}, 3,
                    true);
}

TEST_CASE("early passer and rook-file changes pass within two plies", "[unit]") {
    const chess::Board root("7k/8/2p5/3P4/8/8/8/1R2K3 w - - 0 1");
    chess::Board board = root;
    const std::vector<chess::Move> line = parse_line(board, {"b1a1", "c6c5"});
    std::vector<PositionalChange> changes = chesswhy::positional::line_changes(root, line);
    chesswhy::positional::mark_use(changes, root, line, false);

    for (const PositionalTag tag : {PositionalTag::PassedPawn, PositionalTag::RookOpenFile}) {
        const auto found = std::ranges::find_if(changes, [tag](const PositionalChange& change) {
            return change.feature.tag == tag && change.net_change && change.in_use == true;
        });
        CHECK(found != changes.end());
    }
    CHECK(std::ranges::any_of(changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::White && change.feature.file == 3 && change.ply == 2 &&
               change.change == PositionalChangeKind::Appears && change.net_change &&
               change.in_use == true;
    }));
    CHECK(std::ranges::any_of(changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::PassedPawn &&
               change.feature.side == Side::Black && change.feature.file == 2 && change.ply == 2 &&
               change.change == PositionalChangeKind::Appears && change.net_change &&
               change.in_use == true;
    }));
    CHECK(std::ranges::any_of(changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::RookOpenFile &&
               change.feature.side == Side::White && change.feature.file == 0 && change.ply == 1 &&
               change.change == PositionalChangeKind::Appears && change.net_change &&
               change.in_use == true;
    }));
    CHECK(std::ranges::any_of(changes, [](const PositionalChange& change) {
        return change.feature.tag == PositionalTag::RookOpenFile &&
               change.feature.side == Side::White && change.feature.file == 1 && change.ply == 1 &&
               change.change == PositionalChangeKind::Disappears && change.net_change &&
               change.in_use == true;
    }));
}
