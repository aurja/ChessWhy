#include "chesswhy/positional.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "progress.hpp"
#include "reference.hpp"

namespace {

using chesswhy::PositionalChange;
using chesswhy::PositionalChangeKind;
using chesswhy::PositionalFeature;
using chesswhy::PositionalTag;
using chesswhy::Side;
using Fields = std::vector<std::string>;

constexpr std::array<PositionalTag, 25> kTags{
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

[[nodiscard]] std::string_view tag_name(PositionalTag tag) noexcept {
    constexpr std::array<std::string_view, kTags.size()> names{
        "isolated_pawn",
        "doubled_pawns",
        "tripled_pawns",
        "backward_pawn",
        "passed_pawn",
        "connected_passed_pawns",
        "pawn_islands",
        "pawn_chain",
        "castling_kingside",
        "castling_queenside",
        "king_shield_gap",
        "open_file_near_king",
        "half_open_file_near_king",
        "king_zone_attacker",
        "bishop_pair",
        "rook_open_file",
        "rook_half_open_file",
        "rook_seventh",
        "outpost",
        "bad_bishop",
        "mobility_lead",
        "space_lead",
        "development_lead",
        "material_ahead",
        "exchange_up",
    };
    return names[static_cast<std::size_t>(tag)];
}

[[nodiscard]] Side report_side(chess::Color color) noexcept {
    return color == chess::Color::WHITE ? Side::White : Side::Black;
}

[[nodiscard]] std::string_view side_name(Side side) noexcept {
    return side == Side::White ? "white" : "black";
}

[[nodiscard]] std::string_view change_name(PositionalChangeKind change) noexcept {
    return change == PositionalChangeKind::Appears ? "appears" : "disappears";
}

[[nodiscard]] std::string square_name(std::uint8_t square) {
    std::string name(2, ' ');
    name[0] = static_cast<char>('a' + (square % 8));
    name[1] = static_cast<char>('1' + (square / 8));
    return name;
}

[[nodiscard]] bool same_piece(const chesswhy::PieceOnSquare& left,
                              const chesswhy::PieceOnSquare& right) noexcept {
    return left.square == right.square && left.piece == right.piece;
}

[[nodiscard]] bool same_feature(const PositionalFeature& left, const PositionalFeature& right) {
    return left.tag == right.tag && left.side == right.side && left.file == right.file &&
           left.value == right.value && left.pieces.size() == right.pieces.size() &&
           std::ranges::equal(left.pieces, right.pieces, same_piece);
}

[[nodiscard]] bool same_change(const PositionalChange& left, const PositionalChange& right) {
    return left.ply == right.ply && left.change == right.change &&
           left.holds_at_end == right.holds_at_end && same_feature(left.feature, right.feature);
}

[[nodiscard]] std::string feature_string(const PositionalFeature& feature) {
    std::string result = "{";
    result += tag_name(feature.tag);
    result += ",side=";
    result += side_name(feature.side);
    result += ",file=";
    result += feature.file.has_value()
                  ? std::string(1, static_cast<char>('a' + feature.file.value()))
                  : "null";
    result += ",value=";
    result += feature.value.has_value() ? std::to_string(feature.value.value()) : "null";
    result += ",pieces=[";
    for (std::size_t index = 0; index < feature.pieces.size(); ++index) {
        if (index != 0) {
            result += ',';
        }
        result += square_name(feature.pieces[index].square);
        result += ':';
        result += feature.pieces[index].piece;
    }
    result += "]}";
    return result;
}

[[nodiscard]] std::string feature_list_string(const std::vector<PositionalFeature>& features) {
    if (features.empty()) {
        return "[]";
    }
    std::string result = "[";
    for (std::size_t index = 0; index < features.size(); ++index) {
        if (index != 0) {
            result += ';';
        }
        result += feature_string(features[index]);
    }
    result += ']';
    return result;
}

[[nodiscard]] std::string change_string(const PositionalChange& change) {
    return "{ply=" + std::to_string(change.ply) +
           ",change=" + std::string(change_name(change.change)) +
           ",holds_at_end=" + (change.holds_at_end ? "true" : "false") +
           ",feature=" + feature_string(change.feature) + "}";
}

[[nodiscard]] std::string change_list_string(const std::vector<PositionalChange>& changes) {
    if (changes.empty()) {
        return "[]";
    }
    std::string result = "[";
    for (std::size_t index = 0; index < changes.size(); ++index) {
        if (index != 0) {
            result += ';';
        }
        result += change_string(changes[index]);
    }
    result += ']';
    return result;
}

[[nodiscard]] std::vector<PositionalFeature> for_tag(const std::vector<PositionalFeature>& features,
                                                     PositionalTag tag) {
    std::vector<PositionalFeature> result;
    for (const PositionalFeature& feature : features) {
        if (feature.tag == tag) {
            result.push_back(feature);
        }
    }
    return result;
}

[[nodiscard]] std::vector<PositionalChange> for_tag(const std::vector<PositionalChange>& changes,
                                                    PositionalTag tag) {
    std::vector<PositionalChange> result;
    for (const PositionalChange& change : changes) {
        if (change.feature.tag == tag) {
            result.push_back(change);
        }
    }
    return result;
}

[[nodiscard]] bool same_feature_list(const std::vector<PositionalFeature>& left,
                                     const std::vector<PositionalFeature>& right) {
    return left.size() == right.size() && std::ranges::equal(left, right, same_feature);
}

[[nodiscard]] bool same_change_list(const std::vector<PositionalChange>& left,
                                    const std::vector<PositionalChange>& right) {
    return left.size() == right.size() && std::ranges::equal(left, right, same_change);
}

struct MismatchExample {
    std::string kind;
    std::string fen;
    std::string detector;
    std::string reference;
};

struct MismatchRecord {
    std::size_t count = 0;
    std::optional<MismatchExample> example;
};

struct Statistics {
    std::size_t puzzles = 0;
    std::size_t boards = 0;
    std::size_t feature_lists = 0;
    std::size_t mismatching_feature_lists = 0;
    std::size_t lines = 0;
    std::size_t mismatching_lines = 0;
    std::size_t count_values = 0;
    std::size_t mismatching_count_values = 0;
    std::array<std::size_t, kTags.size()> boards_with_tag{};
    std::map<std::string, MismatchRecord> mismatches;
};

void record_mismatch(Statistics& statistics, std::string tag, std::string kind,
                     const std::string& fen, std::string detector, std::string reference) {
    MismatchRecord& record = statistics.mismatches[std::move(tag)];
    ++record.count;
    if (!record.example.has_value()) {
        record.example = MismatchExample{.kind = std::move(kind),
                                         .fen = fen,
                                         .detector = std::move(detector),
                                         .reference = std::move(reference)};
    }
}

void compare_features(const std::string& fen, const std::vector<PositionalFeature>& detector,
                      const std::vector<PositionalFeature>& reference, Statistics& statistics) {
    ++statistics.feature_lists;
    bool tag_difference = false;
    for (std::size_t index = 0; index < kTags.size(); ++index) {
        const PositionalTag tag = kTags[index];
        const auto reference_tag = for_tag(reference, tag);
        if (!reference_tag.empty()) {
            ++statistics.boards_with_tag[index];
        }
        const auto detector_tag = for_tag(detector, tag);
        if (!same_feature_list(detector_tag, reference_tag)) {
            tag_difference = true;
            record_mismatch(statistics, std::string(tag_name(tag)), "features", fen,
                            feature_list_string(detector_tag), feature_list_string(reference_tag));
        }
    }
    if (!same_feature_list(detector, reference)) {
        ++statistics.mismatching_feature_lists;
        if (!tag_difference) {
            record_mismatch(statistics, "tag_order", "feature ordering", fen,
                            feature_list_string(detector), feature_list_string(reference));
        }
    }
}

void compare_changes(const std::string& fen, const std::vector<PositionalChange>& detector,
                     const std::vector<PositionalChange>& reference, Statistics& statistics) {
    ++statistics.lines;
    bool tag_difference = false;
    for (const PositionalTag tag : kTags) {
        const auto detector_tag = for_tag(detector, tag);
        const auto reference_tag = for_tag(reference, tag);
        if (!same_change_list(detector_tag, reference_tag)) {
            tag_difference = true;
            record_mismatch(statistics, std::string(tag_name(tag)), "line_changes", fen,
                            change_list_string(detector_tag), change_list_string(reference_tag));
        }
    }
    if (!same_change_list(detector, reference)) {
        ++statistics.mismatching_lines;
        if (!tag_difference) {
            record_mismatch(statistics, "tag_order", "line-change ordering", fen,
                            change_list_string(detector), change_list_string(reference));
        }
    }
}

void compare_count(const std::string& fen, PositionalTag tag, std::string_view label, Side side,
                   int detector, int reference, Statistics& statistics) {
    ++statistics.count_values;
    if (detector != reference) {
        ++statistics.mismatching_count_values;
        record_mismatch(statistics, std::string(tag_name(tag)),
                        std::string(label) + "/" + std::string(side_name(side)), fen,
                        std::to_string(detector), std::to_string(reference));
    }
}

[[nodiscard]] std::vector<std::string> parse_csv_record(std::string_view line) {
    Fields fields;
    std::string field;
    bool quoted = false;
    bool after_quote = false;
    bool at_field_start = true;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (quoted) {
            if (character == '"') {
                if (index + 1 < line.size() && line[index + 1] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                    after_quote = true;
                }
            } else {
                field.push_back(character);
            }
        } else if (after_quote) {
            if (character != ',') {
                throw std::invalid_argument("unexpected character after quoted CSV field");
            }
            fields.push_back(std::move(field));
            field.clear();
            at_field_start = true;
            after_quote = false;
        } else if (character == ',') {
            fields.push_back(std::move(field));
            field.clear();
            at_field_start = true;
        } else if (character == '"') {
            if (!at_field_start) {
                throw std::invalid_argument("quote inside an unquoted CSV field");
            }
            quoted = true;
            at_field_start = false;
        } else {
            field.push_back(character);
            at_field_start = false;
        }
    }
    if (quoted) {
        throw std::invalid_argument("unterminated quoted CSV field");
    }
    fields.push_back(std::move(field));
    return fields;
}

[[nodiscard]] std::vector<std::string_view> words(std::string_view text) {
    std::vector<std::string_view> result;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        while (cursor < text.size() &&
               std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
            ++cursor;
        }
        const std::size_t start = cursor;
        while (cursor < text.size() &&
               std::isspace(static_cast<unsigned char>(text[cursor])) == 0) {
            ++cursor;
        }
        if (start != cursor) {
            result.push_back(text.substr(start, cursor - start));
        }
    }
    return result;
}

[[nodiscard]] std::vector<chess::Move> parse_line(chess::Board& board,
                                                  std::span<const std::string_view> tokens) {
    std::vector<chess::Move> moves;
    moves.reserve(tokens.size());
    for (const std::string_view token : tokens) {
        if (!chess::uci::isUciMove(token)) {
            throw std::invalid_argument("malformed UCI move " + std::string(token));
        }
        const chess::Move move = chess::uci::uciToMove(board, token);
        chess::Movelist legal_moves;
        chess::movegen::legalmoves(legal_moves, board);
        if (move == chess::Move::NO_MOVE ||
            std::ranges::find(legal_moves, move) == legal_moves.end()) {
            throw std::invalid_argument("illegal UCI move " + std::string(token));
        }
        moves.push_back(move);
        board.makeMove(move);
    }
    return moves;
}

void compare_board(chess::Board& board, Statistics& statistics) {
    ++statistics.boards;
    const std::string fen = board.getFen();
    const auto detector = chesswhy::positional::features(board);
    const auto reference = chesswhy::positional_oracle::reference_features(board);
    compare_features(fen, detector, reference, statistics);
    for (const chess::Color color : {chess::Color::WHITE, chess::Color::BLACK}) {
        const Side side = report_side(color);
        compare_count(fen, PositionalTag::MobilityLead, "mobility", side,
                      chesswhy::positional::mobility(board, color),
                      chesswhy::positional_oracle::reference_mobility(board, color), statistics);
        compare_count(fen, PositionalTag::SpaceLead, "space", side,
                      chesswhy::positional::space(board, color),
                      chesswhy::positional_oracle::reference_space(board, color), statistics);
        compare_count(fen, PositionalTag::DevelopmentLead, "undeveloped", side,
                      chesswhy::positional::undeveloped(board, color),
                      chesswhy::positional_oracle::reference_undeveloped(board, color), statistics);
    }
}

void compare_line(const std::string& fen, std::span<const std::string_view> uci_moves,
                  Statistics& statistics) {
    chess::Board root;
    if (!root.setFen(fen)) {
        throw std::invalid_argument("invalid FEN");
    }
    chess::Board walked = root;
    const std::vector<chess::Move> moves = parse_line(walked, uci_moves);
    walked = root;
    for (std::size_t ply = 0; ply <= moves.size(); ++ply) {
        compare_board(walked, statistics);
        if (ply < moves.size()) {
            walked.makeMove(moves[ply]);
        }
    }
    const auto detector_changes = chesswhy::positional::line_changes(root, moves);
    const auto reference_changes = chesswhy::positional_oracle::reference_line_changes(root, moves);
    compare_changes(root.getFen(), detector_changes, reference_changes, statistics);
}

void process_csv(const std::string& path, Statistics& statistics) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open puzzle CSV " + path);
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty puzzle CSV " + path);
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (parse_csv_record(line) !=
        std::vector<std::string>{"PuzzleId", "FEN", "Moves", "Themes", "Stratum"}) {
        throw std::runtime_error("unexpected puzzle CSV header in " + path);
    }
    const std::size_t puzzle_total = chesswhy::test::count_csv_data_rows(path);
    chesswhy::test::Progress progress("positional_oracle puzzle", puzzle_total);
    std::size_t puzzle_index = 0;

    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        try {
            const std::vector<std::string> fields = parse_csv_record(line);
            if (fields.size() != 5 || fields[0].empty()) {
                throw std::invalid_argument("expected five CSV fields and a non-empty PuzzleId");
            }
            const std::vector<std::string_view> tokens = words(fields[2]);
            compare_line(fields[1], tokens, statistics);
            ++statistics.puzzles;
            ++puzzle_index;
            progress.item_completed(puzzle_index, fields[0]);
        } catch (const std::exception& error) {
            throw std::runtime_error("CSV row " + std::to_string(line_number) + ": " +
                                     error.what());
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while reading puzzle CSV " + path);
    }
}
void process_positional_fixtures(const std::string& path, Statistics& statistics) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open positional fixtures " + path);
    }
    const nlohmann::json fixtures = nlohmann::json::parse(input);
    if (!fixtures.contains("features") || !fixtures.at("features").is_array() ||
        !fixtures.contains("lines") || !fixtures.at("lines").is_array()) {
        throw std::runtime_error("positional fixtures must contain feature and line arrays");
    }
    const auto& feature_fixtures = fixtures.at("features");
    const auto& line_fixtures = fixtures.at("lines");
    chesswhy::test::Progress progress("positional_oracle fixture",
                                      feature_fixtures.size() + line_fixtures.size());
    std::size_t entry_index = 0;

    for (const auto& fixture : feature_fixtures) {
        const std::string id = fixture.at("id").get<std::string>();
        try {
            const std::string fen = fixture.at("fen").get<std::string>();
            chess::Board board;
            if (!board.setFen(fen)) {
                throw std::invalid_argument("invalid FEN");
            }
            compare_board(board, statistics);
        } catch (const std::exception& error) {
            throw std::runtime_error("feature fixture " + id + ": " + error.what());
        }
        ++entry_index;
        progress.item_completed(entry_index, id);
    }

    for (const auto& fixture : line_fixtures) {
        const std::string id = fixture.at("id").get<std::string>();
        try {
            const std::string fen = fixture.at("fen").get<std::string>();
            const std::vector<std::string> owned_tokens =
                fixture.at("line_uci").get<std::vector<std::string>>();
            std::vector<std::string_view> tokens;
            tokens.reserve(owned_tokens.size());
            for (const std::string& token : owned_tokens) {
                tokens.push_back(token);
            }
            compare_line(fen, tokens, statistics);
        } catch (const std::exception& error) {
            throw std::runtime_error("line fixture " + id + ": " + error.what());
        }
        ++entry_index;
        progress.item_completed(entry_index, id);
    }
}

void print_statistics(const Statistics& statistics) {
    std::cout << "puzzles=" << statistics.puzzles << ", boards=" << statistics.boards
              << ", feature lists=" << statistics.feature_lists
              << ", feature-list mismatches=" << statistics.mismatching_feature_lists
              << ", lines=" << statistics.lines
              << ", line-change mismatches=" << statistics.mismatching_lines
              << ", §4.4 count values=" << statistics.count_values
              << ", count mismatches=" << statistics.mismatching_count_values << '\n';
    std::cout << "boards carrying each tag:\n";
    for (std::size_t index = 0; index < kTags.size(); ++index) {
        std::cout << "  " << tag_name(kTags[index]) << ": " << statistics.boards_with_tag[index]
                  << '\n';
    }
    std::cout << "mismatches by tag:\n";
    if (statistics.mismatches.empty()) {
        std::cout << "  none\n";
        return;
    }
    for (const auto& [tag, record] : statistics.mismatches) {
        if (!record.example.has_value()) {
            throw std::logic_error("mismatch tag has no example");
        }
        const MismatchExample& example = record.example.value();
        std::cout << "  " << tag << ": " << record.count << "; " << example.kind
                  << "; FEN=" << example.fen << "; detector=" << example.detector
                  << "; reference=" << example.reference << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: positional_oracle <puzzle-csv> <positional-json>\n";
        return 2;
    }
    try {
        Statistics statistics;
        process_csv(argv[1], statistics);
        process_positional_fixtures(argv[2], statistics);
        print_statistics(statistics);
        return statistics.mismatches.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "positional_oracle: " << error.what() << '\n';
        return 2;
    }
}
