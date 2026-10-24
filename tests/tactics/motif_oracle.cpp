#include "chesswhy/report.hpp"
#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <chess.hpp>
#include <nlohmann/json.hpp>

#include "oracle/reference.hpp"
#include "progress.hpp"

namespace {

using chesswhy::Motif;
using chesswhy::MotifTag;
using Json = nlohmann::json;

constexpr std::array<MotifTag, 14> kTags{
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

struct Options {
    std::string csv_path;
    std::optional<std::string> fixtures_path;
    std::optional<std::string> mismatches_path;
    std::size_t mismatch_limit = 0;
    std::size_t stride = 1;
};

struct Mismatch {
    std::string id;
    std::string ply;
    std::string fen;
    std::string moves;
    std::vector<Motif> detector;
    std::vector<Motif> reference;
};

struct ClassRecord {
    std::size_t count = 0;
    std::optional<Mismatch> example;
};

struct Statistics {
    std::size_t compared = 0;
    std::size_t mismatching_lists = 0;
    std::size_t sampled_puzzles = 0;
    std::size_t pin_tactical_instances_compared = 0;
    std::array<std::size_t, kTags.size()> mismatches_by_tag{};
    std::map<std::string, ClassRecord> classes;
};

[[nodiscard]] Options parse_options(int argc, char** argv) {
    if (argc < 2) {
        throw std::invalid_argument("usage: tactics_motif_oracle <csv> [--stride N] "
                                    "[--fixtures <motifs.json>] [--mismatches N <out.tsv>]");
    }
    Options options;
    options.csv_path = argv[1];
    bool stride_set = false;
    for (int index = 2; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--stride") {
            if (stride_set || index + 1 >= argc) {
                throw std::invalid_argument("--stride requires one positive integer and may appear "
                                            "once");
            }
            const std::string_view stride_text(argv[++index]);
            const auto [end, error] = std::from_chars(
                stride_text.data(), stride_text.data() + stride_text.size(), options.stride);
            if (error != std::errc{} || end != stride_text.data() + stride_text.size() ||
                options.stride == 0) {
                throw std::invalid_argument("--stride requires a positive integer");
            }
            stride_set = true;
        } else if (option == "--fixtures") {
            if (options.fixtures_path || index + 1 >= argc) {
                throw std::invalid_argument("--fixtures requires one path and may appear once");
            }
            options.fixtures_path = argv[++index];
        } else if (option == "--mismatches") {
            if (options.mismatches_path || index + 2 >= argc) {
                throw std::invalid_argument("--mismatches requires N and an output path");
            }
            const std::string_view count_text(argv[++index]);
            const auto [end, error] = std::from_chars(
                count_text.data(), count_text.data() + count_text.size(), options.mismatch_limit);
            if (error != std::errc{} || end != count_text.data() + count_text.size()) {
                throw std::invalid_argument("--mismatches N requires a non-negative integer");
            }
            options.mismatches_path = argv[++index];
        } else {
            throw std::invalid_argument("unknown option " + std::string(option));
        }
    }
    return options;
}

[[nodiscard]] std::vector<std::string> parse_csv_record(std::string_view line) {
    std::vector<std::string> fields;
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

[[nodiscard]] chess::Move parse_legal_uci(const chess::Board& board, std::string_view uci) {
    if (!chess::uci::isUciMove(uci)) {
        throw std::invalid_argument("malformed UCI move " + std::string(uci));
    }
    const chess::Move move = chess::uci::uciToMove(board, uci);
    if (move == chess::Move::NO_MOVE) {
        throw std::invalid_argument("malformed UCI move " + std::string(uci));
    }
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board);
    if (std::ranges::find(legal_moves, move) == legal_moves.end()) {
        throw std::invalid_argument("illegal UCI move " + std::string(uci));
    }
    return move;
}

[[nodiscard]] std::vector<chess::Move> parse_line(chess::Board& board,
                                                  std::span<const std::string_view> uci_moves) {
    std::vector<chess::Move> result;
    result.reserve(uci_moves.size());
    for (const std::string_view uci : uci_moves) {
        const chess::Move move = parse_legal_uci(board, uci);
        result.push_back(move);
        board.makeMove(move);
    }
    return result;
}

[[nodiscard]] std::string square_name(std::uint8_t square) {
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + (square % 8));
    result[1] = static_cast<char>('1' + (square / 8));
    return result;
}

[[nodiscard]] bool same_piece(const chesswhy::PieceOnSquare& left,
                              const chesswhy::PieceOnSquare& right) noexcept {
    return left.square == right.square && left.piece == right.piece;
}

[[nodiscard]] bool same_motif(const Motif& left, const Motif& right) {
    return left.tag == right.tag && left.side == right.side && left.ply == right.ply &&
           same_piece(left.actor, right.actor) && left.targets.size() == right.targets.size() &&
           std::ranges::equal(left.targets, right.targets, same_piece) && left.gain == right.gain &&
           left.threat == right.threat && left.restriction == right.restriction &&
           left.contribution == right.contribution;
}

[[nodiscard]] bool same_list(const std::vector<Motif>& left, const std::vector<Motif>& right) {
    return left.size() == right.size() && std::ranges::equal(left, right, same_motif);
}

[[nodiscard]] std::string motif_string(const Motif& motif) {
    std::ostringstream output;
    output << '{' << chesswhy::to_string(motif.tag) << ',' << chesswhy::to_string(motif.side) << ','
           << motif.ply << ",actor=" << square_name(motif.actor.square) << ':' << motif.actor.piece
           << ",targets=[";
    for (std::size_t index = 0; index < motif.targets.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << square_name(motif.targets[index].square) << ':' << motif.targets[index].piece;
    }
    output << "],threat=";
    if (motif.threat.has_value()) {
        output << chesswhy::to_string(motif.threat->kind) << ':'
               << square_name(motif.threat->square);
    } else {
        output << "null";
    }
    output << ",gain=";
    if (motif.gain.has_value()) {
        output << *motif.gain;
    } else {
        output << "null";
    }
    output << ",restriction=";
    if (motif.restriction.has_value()) {
        const auto& restriction = *motif.restriction;
        output << "{piece=" << square_name(restriction.piece.square) << ':'
               << restriction.piece.piece << ",restricted=[";
        for (std::size_t index = 0; index < restriction.restricted.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << restriction.restricted[index];
        }
        output << "],free=[";
        for (std::size_t index = 0; index < restriction.free.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << restriction.free[index];
        }
        output << "],held_to=" << chesswhy::to_string(restriction.held_to.kind) << ':'
               << restriction.held_to.name << '}';
    } else {
        output << "null";
    }
    output << ",contribution=";
    if (motif.contribution.has_value()) {
        output << chesswhy::to_string(*motif.contribution);
    } else {
        output << "null";
    }
    output << '}';
    return output.str();
}

[[nodiscard]] std::string list_string(const std::vector<Motif>& motifs) {
    if (motifs.empty()) {
        return "[]";
    }
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < motifs.size(); ++index) {
        if (index != 0) {
            output << ';';
        }
        output << motif_string(motifs[index]);
    }
    output << ']';
    return output.str();
}

[[nodiscard]] std::string tsv_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    return result;
}

[[nodiscard]] std::vector<Motif> for_tag(const std::vector<Motif>& motifs, MotifTag tag) {
    std::vector<Motif> result;
    for (const Motif& motif : motifs) {
        if (motif.tag == tag) {
            result.push_back(motif);
        }
    }
    return result;
}

[[nodiscard]] std::string tag_name(MotifTag tag) {
    return chesswhy::to_string(tag);
}

void add_class(Statistics& statistics, const std::string& name, const Mismatch& mismatch) {
    ClassRecord& record = statistics.classes[name];
    ++record.count;
    if (!record.example.has_value()) {
        record.example = mismatch;
    }
}

void classify_tag_mismatch(Statistics& statistics, MotifTag tag, const std::vector<Motif>& detector,
                           const std::vector<Motif>& reference, const Mismatch& mismatch) {
    const std::string prefix = tag_name(tag) + "/";
    if (detector.size() != reference.size()) {
        add_class(statistics, prefix + "count", mismatch);
    }
    const std::size_t common = std::min(detector.size(), reference.size());
    bool found_field_difference = false;
    for (std::size_t index = 0; index < common; ++index) {
        const Motif& left = detector[index];
        const Motif& right = reference[index];
        if (left.side != right.side) {
            add_class(statistics, prefix + "side", mismatch);
            found_field_difference = true;
        }
        if (left.ply != right.ply) {
            add_class(statistics, prefix + "ply", mismatch);
            found_field_difference = true;
        }
        if (left.actor.square != right.actor.square) {
            add_class(statistics, prefix + "actor_square", mismatch);
            found_field_difference = true;
        }
        if (left.actor.piece != right.actor.piece) {
            add_class(statistics, prefix + "actor_piece", mismatch);
            found_field_difference = true;
        }
        if (left.targets.size() != right.targets.size()) {
            add_class(statistics, prefix + "targets", mismatch);
            found_field_difference = true;
        } else {
            for (std::size_t target_index = 0; target_index < left.targets.size(); ++target_index) {
                if (!same_piece(left.targets[target_index], right.targets[target_index])) {
                    add_class(statistics, prefix + "targets", mismatch);
                    found_field_difference = true;
                    break;
                }
            }
        }
        if (left.gain != right.gain) {
            add_class(statistics, prefix + "gain", mismatch);
            found_field_difference = true;
        }
        if (left.contribution != right.contribution) {
            add_class(statistics, prefix + "contribution", mismatch);
            found_field_difference = true;
        }
        if (left.threat != right.threat) {
            add_class(statistics, prefix + "threat", mismatch);
            found_field_difference = true;
        }
        if (left.restriction != right.restriction) {
            add_class(statistics, prefix + "restriction", mismatch);
            found_field_difference = true;
        }
    }
    if (detector.size() == reference.size() &&
        std::ranges::is_permutation(detector, reference, same_motif)) {
        add_class(statistics, prefix + "order", mismatch);
    } else if (!found_field_difference && detector.size() == reference.size()) {
        add_class(statistics, prefix + "identity", mismatch);
    }
}

void compare_lists(Statistics& statistics, std::ostream* mismatch_output,
                   std::size_t mismatch_limit, std::size_t& mismatch_rows, std::string id,
                   std::string ply, std::string fen, std::string moves,
                   const std::vector<Motif>& detector, const std::vector<Motif>& reference) {
    statistics.pin_tactical_instances_compared += for_tag(detector, MotifTag::PinTactical).size() +
                                                  for_tag(reference, MotifTag::PinTactical).size();
    ++statistics.compared;
    if (same_list(detector, reference)) {
        return;
    }
    ++statistics.mismatching_lists;
    const Mismatch mismatch{.id = std::move(id),
                            .ply = std::move(ply),
                            .fen = std::move(fen),
                            .moves = std::move(moves),
                            .detector = detector,
                            .reference = reference};

    bool any_tag_differs = false;
    for (std::size_t index = 0; index < kTags.size(); ++index) {
        const MotifTag tag = kTags[index];
        const auto detector_tag = for_tag(detector, tag);
        const auto reference_tag = for_tag(reference, tag);
        if (!same_list(detector_tag, reference_tag)) {
            ++statistics.mismatches_by_tag[index];
            classify_tag_mismatch(statistics, tag, detector_tag, reference_tag, mismatch);
            any_tag_differs = true;
        }
    }
    if (!any_tag_differs) {
        add_class(statistics, "cross_tag_order", mismatch);
    }

    if (mismatch_output != nullptr && mismatch_rows < mismatch_limit) {
        *mismatch_output << tsv_escape(mismatch.id) << '\t' << tsv_escape(mismatch.ply) << '\t'
                         << tsv_escape(mismatch.fen) << '\t' << tsv_escape(mismatch.moves) << '\t'
                         << tsv_escape(list_string(mismatch.detector)) << '\t'
                         << tsv_escape(list_string(mismatch.reference)) << '\n';
        ++mismatch_rows;
    }
}

void process_position(const std::string& id, const std::string& fen,
                      std::span<const std::string_view> uci_moves, const std::string& moves_text,
                      Statistics& statistics, std::ostream* mismatch_output,
                      std::size_t mismatch_limit, std::size_t& mismatch_rows,
                      bool include_pin_tactical) {
    chess::Board root;
    if (!root.setFen(fen)) {
        throw std::invalid_argument("invalid FEN");
    }
    chess::Board walked = root;
    const std::vector<chess::Move> moves = parse_line(walked, uci_moves);
    walked = root;
    const chesswhy::tactics::MotifOptions motif_options{.pin_tactical = include_pin_tactical};
    for (std::size_t ply = 0; ply <= moves.size(); ++ply) {
        const std::vector<Motif> detector =
            chesswhy::tactics::position_motifs(walked, motif_options);
        const std::vector<Motif> reference =
            chesswhy::oracle::reference_position_motifs(walked, motif_options);
        compare_lists(statistics, mismatch_output, mismatch_limit, mismatch_rows, id,
                      std::to_string(ply), walked.getFen(), moves_text, detector, reference);
        if (ply < moves.size()) {
            walked.makeMove(moves[ply]);
        }
    }
    const std::vector<Motif> detector = chesswhy::tactics::line_motifs(root, moves, motif_options);
    const std::vector<Motif> reference = chesswhy::oracle::reference_line_motifs(
        root, std::span<const chess::Move>(moves), motif_options);
    compare_lists(statistics, mismatch_output, mismatch_limit, mismatch_rows, id, "line",
                  root.getFen(), moves_text, detector, reference);
}

std::size_t process_csv(const std::string& path, Statistics& statistics,
                        std::ostream* mismatch_output, std::size_t mismatch_limit,
                        std::size_t& mismatch_rows, std::size_t stride) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open CSV " + path);
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty CSV " + path);
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (parse_csv_record(line) !=
        std::vector<std::string>{"PuzzleId", "FEN", "Moves", "Themes", "Stratum"}) {
        throw std::runtime_error("unexpected CSV header in " + path);
    }
    const std::size_t puzzle_total = chesswhy::test::count_csv_data_rows(path);
    const std::size_t compared_total =
        (puzzle_total / stride) + static_cast<std::size_t>(puzzle_total % stride != 0);
    const std::string progress_label = "tactics_oracle puzzle (stride " + std::to_string(stride) +
                                       ", compared " + std::to_string(compared_total) + ")";
    chesswhy::test::Progress progress(progress_label, compared_total);

    std::size_t line_number = 1;
    std::size_t puzzle_index = 0;
    std::size_t puzzles_compared = 0;
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
            if (puzzle_index % stride == 0) {
                const std::vector<std::string_view> tokens = words(fields[2]);
                const bool include_pin_tactical = puzzles_compared % 16 == 0;
                if (include_pin_tactical) {
                    ++statistics.sampled_puzzles;
                }
                process_position(fields[0], fields[1], tokens, fields[2], statistics,
                                 mismatch_output, mismatch_limit, mismatch_rows,
                                 include_pin_tactical);
                ++puzzles_compared;
                progress.item_completed(puzzles_compared, fields[0]);
            }
            ++puzzle_index;
        } catch (const std::exception& error) {
            throw std::runtime_error("CSV row " + std::to_string(line_number) + ": " +
                                     error.what());
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while reading CSV " + path);
    }
    return puzzles_compared;
}

void process_fixtures(const std::string& path, Statistics& statistics,
                      std::ostream* mismatch_output, std::size_t mismatch_limit,
                      std::size_t& mismatch_rows) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open fixture file " + path);
    }
    const Json data = Json::parse(input);
    const Json& fixtures = data.at("motif_fixtures");
    if (!fixtures.is_array()) {
        throw std::runtime_error("motif_fixtures is not an array in " + path);
    }
    chesswhy::test::Progress progress("tactics_oracle fixture", fixtures.size());
    std::size_t fixture_index = 0;
    for (const Json& fixture : fixtures) {
        const std::string id = fixture.at("id").get<std::string>();
        const std::string fen = fixture.at("fen").get<std::string>();
        const Json& line_json = fixture.at("line_uci");
        if (!line_json.is_array()) {
            throw std::runtime_error(id + ": line_uci is not an array");
        }
        std::vector<std::string> owned_tokens;
        owned_tokens.reserve(line_json.size());
        for (const Json& token : line_json) {
            owned_tokens.push_back(token.get<std::string>());
        }
        std::vector<std::string_view> tokens;
        tokens.reserve(owned_tokens.size());
        for (const std::string& token : owned_tokens) {
            tokens.push_back(token);
        }
        std::string moves;
        for (const std::string& token : owned_tokens) {
            if (!moves.empty()) {
                moves.push_back(' ');
            }
            moves += token;
        }
        try {
            process_position(id, fen, tokens, moves, statistics, mismatch_output, mismatch_limit,
                             mismatch_rows, true);
        } catch (const std::exception& error) {
            throw std::runtime_error(id + ": " + error.what());
        }
        ++fixture_index;
        progress.item_completed(fixture_index, id);
    }
}

void print_statistics(
    std::string_view label, const Statistics& statistics,
    std::optional<std::pair<std::size_t, std::size_t>> csv_sample = std::nullopt) {
    std::cout << label << ": ";
    if (csv_sample.has_value()) {
        std::cout << "stride=" << csv_sample->first
                  << ", CSV puzzles compared=" << csv_sample->second << ", ";
    }
    std::cout << "lists compared=" << statistics.compared
              << ", mismatching lists=" << statistics.mismatching_lists
              << ", sampled puzzles=" << statistics.sampled_puzzles
              << ", pin_tactical instances compared (detector+reference)="
              << statistics.pin_tactical_instances_compared << '\n';
    for (std::size_t index = 0; index < kTags.size(); ++index) {
        std::cout << "  " << tag_name(kTags[index]) << ": " << statistics.mismatches_by_tag[index]
                  << '\n';
    }
    std::cout << "  mismatch classes:\n";
    if (statistics.classes.empty()) {
        std::cout << "    none\n";
        return;
    }
    for (const auto& [name, record] : statistics.classes) {
        if (!record.example.has_value()) {
            throw std::logic_error("mismatch class has no example");
        }
        const Mismatch& example = record.example.value();
        std::cout << "    " << name << ": " << record.count << "; example id=" << example.id;
        if (name.ends_with("/restriction")) {
            std::cout << " plan=PostM3_EM2.md §4.1 lines 97-122";
        }
        std::cout << " ply=" << example.ply << " FEN=" << example.fen
                  << " detector=" << list_string(example.detector)
                  << " reference=" << list_string(example.reference) << '\n';
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);
        std::ofstream mismatch_file;
        std::ostream* mismatch_output = nullptr;
        if (options.mismatches_path.has_value()) {
            mismatch_file.open(*options.mismatches_path);
            if (!mismatch_file) {
                throw std::runtime_error("cannot open mismatch output " + *options.mismatches_path);
            }
            mismatch_file << "id\tply\tfen\tmoves\tdetector\treference\n";
            mismatch_output = &mismatch_file;
        }
        std::size_t mismatch_rows = 0;
        Statistics csv_statistics;

        const std::size_t csv_puzzles_compared =
            process_csv(options.csv_path, csv_statistics, mismatch_output, options.mismatch_limit,
                        mismatch_rows, options.stride);
        print_statistics("CSV " + options.csv_path, csv_statistics,
                         std::pair{options.stride, csv_puzzles_compared});

        Statistics fixture_statistics;
        if (options.fixtures_path.has_value()) {
            process_fixtures(*options.fixtures_path, fixture_statistics, mismatch_output,
                             options.mismatch_limit, mismatch_rows);
            print_statistics("fixtures " + *options.fixtures_path, fixture_statistics);
        }
        if (options.mismatches_path.has_value() && !mismatch_file) {
            throw std::runtime_error("failed while writing mismatch output " +
                                     *options.mismatches_path);
        }
        return csv_statistics.mismatching_lists == 0 && fixture_statistics.mismatching_lists == 0
                   ? 0
                   : 1;
    } catch (const std::exception& error) {
        std::cerr << "tactics_motif_oracle: " << error.what() << '\n';
        return 2;
    }
}
