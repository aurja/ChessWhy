#include "chesswhy/report.hpp"
#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
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

namespace {

using chesswhy::Motif;
using chesswhy::MotifTag;
using chesswhy::Side;

constexpr std::size_t kTagCount = 11;

struct MeasuredTag {
    std::string_view lichess_tag;
    std::array<MotifTag, 2> motif_tags;
    std::size_t motif_tag_count;
    std::optional<double> precision_target;
    std::optional<double> recall_target;
};

// Keep mapping order and targets together; freezing targets only changes this table.
constexpr std::array<MeasuredTag, kTagCount> kMeasuredTags{{
    {.lichess_tag = "hangingPiece",
     .motif_tags = {MotifTag::HangingPiece, MotifTag::HangingPiece},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "fork",
     .motif_tags = {MotifTag::Fork, MotifTag::Fork},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "pin",
     .motif_tags = {MotifTag::PinAbsolute, MotifTag::PinRelative},
     .motif_tag_count = 2,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "skewer",
     .motif_tags = {MotifTag::Skewer, MotifTag::Skewer},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "discoveredAttack",
     .motif_tags = {MotifTag::DiscoveredAttack, MotifTag::DiscoveredCheck},
     .motif_tag_count = 2,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "discoveredCheck",
     .motif_tags = {MotifTag::DiscoveredCheck, MotifTag::DiscoveredCheck},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "doubleCheck",
     .motif_tags = {MotifTag::DoubleCheck, MotifTag::DoubleCheck},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "backRankMate",
     .motif_tags = {MotifTag::BackRankMate, MotifTag::BackRankMate},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "capturingDefender",
     .motif_tags = {MotifTag::RemovalOfDefender, MotifTag::RemovalOfDefender},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "trappedPiece",
     .motif_tags = {MotifTag::TrappedPiece, MotifTag::TrappedPiece},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
    {.lichess_tag = "promotion",
     .motif_tags = {MotifTag::Promotion, MotifTag::Promotion},
     .motif_tag_count = 1,
     .precision_target = std::nullopt,
     .recall_target = std::nullopt},
}};

constexpr std::array<std::string_view, 5> kCsvColumns{"PuzzleId", "FEN", "Moves", "Themes",
                                                      "Stratum"};

struct Metrics {
    std::size_t background_tagged = 0;
    std::size_t background_predicted = 0;
    std::size_t background_true_positive = 0;
    std::size_t background_false_positive = 0;
    std::size_t stratum_count = 0;
    std::size_t stratum_true_positive = 0;
};

struct Disagreement {
    std::string puzzle_id;
    std::string fen;
    std::string moves;
    std::string themes;
    std::string instances;
};

struct DisagreementBucket {
    std::vector<Disagreement> false_positives;
    std::vector<Disagreement> false_negatives;
};

struct Options {
    std::string csv_path;
    std::size_t disagreements_per_kind = 0;
    std::optional<std::string> disagreement_path;
};

class RowError : public std::runtime_error {
public:
    RowError(std::string puzzle_id, const std::string& message)
        : std::runtime_error(message), puzzle_id_(std::move(puzzle_id)) {}

    [[nodiscard]] const std::string& puzzle_id() const { return puzzle_id_; }

private:
    std::string puzzle_id_;
};

[[nodiscard]] Options parse_options(int argc, char** argv) {
    if (argc != 2 && argc != 5) {
        throw std::invalid_argument(
            "usage: tactics_puzzle_eval <csv> [--disagreements N <out.tsv>]");
    }

    Options options;
    options.csv_path = argv[1];
    if (argc == 5) {
        if (std::string_view(argv[2]) != "--disagreements") {
            throw std::invalid_argument(
                "usage: tactics_puzzle_eval <csv> [--disagreements N <out.tsv>]");
        }
        const std::string_view count_text(argv[3]);
        const auto [end, error] =
            std::from_chars(count_text.data(), count_text.data() + count_text.size(),
                            options.disagreements_per_kind);
        if (error != std::errc{} || end != count_text.data() + count_text.size()) {
            throw std::invalid_argument("--disagreements N requires a non-negative integer");
        }
        options.disagreement_path = argv[4];
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

[[nodiscard]] std::string first_field(std::string_view line) {
    const auto comma = line.find(',');
    std::string field(line.substr(0, comma));
    if (field.size() >= 2 && field.front() == '"' && field.back() == '"') {
        field = field.substr(1, field.size() - 2);
        std::string unescaped;
        unescaped.reserve(field.size());
        for (std::size_t index = 0; index < field.size(); ++index) {
            unescaped.push_back(field[index]);
            if (field[index] == '"' && index + 1 < field.size() && field[index + 1] == '"') {
                ++index;
            }
        }
        return unescaped;
    }
    return field;
}

[[nodiscard]] std::array<bool, kTagCount> lichess_tags(std::string_view themes) {
    std::array<bool, kTagCount> result{};
    std::size_t cursor = 0;
    while (cursor < themes.size()) {
        while (cursor < themes.size() &&
               std::isspace(static_cast<unsigned char>(themes[cursor])) != 0) {
            ++cursor;
        }
        const std::size_t start = cursor;
        while (cursor < themes.size() &&
               std::isspace(static_cast<unsigned char>(themes[cursor])) == 0) {
            ++cursor;
        }
        const std::string_view theme = themes.substr(start, cursor - start);
        for (std::size_t index = 0; index < kTagCount; ++index) {
            if (theme == kMeasuredTags[index].lichess_tag) {
                result[index] = true;
            }
        }
    }
    return result;
}

[[nodiscard]] std::string square_name(std::uint8_t square) {
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + (square % 8));
    result[1] = static_cast<char>('1' + (square / 8));
    return result;
}

[[nodiscard]] std::string compact_instances(const std::vector<Motif>& motifs) {
    std::ostringstream output;
    for (std::size_t index = 0; index < motifs.size(); ++index) {
        if (index != 0) {
            output << ';';
        }
        const Motif& motif = motifs[index];
        output << chesswhy::to_string(motif.tag) << ':' << chesswhy::to_string(motif.side) << ':'
               << motif.ply << ':' << square_name(motif.actor.square) << '>';
        for (std::size_t target_index = 0; target_index < motif.targets.size(); ++target_index) {
            if (target_index != 0) {
                output << ',';
            }
            output << square_name(motif.targets[target_index].square);
        }
    }
    return motifs.empty() ? "-" : output.str();
}

[[nodiscard]] bool maps_to(const MeasuredTag& tag, MotifTag motif_tag) {
    return std::find(tag.motif_tags.begin(),
                     tag.motif_tags.begin() + static_cast<std::ptrdiff_t>(tag.motif_tag_count),
                     motif_tag) !=
           tag.motif_tags.begin() + static_cast<std::ptrdiff_t>(tag.motif_tag_count);
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

[[nodiscard]] std::string tsv_escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\t':
            escaped += "\\t";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\n':
            escaped += "\\n";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

void emit_disagreement(std::ostream& output, std::string_view tag, std::string_view kind,
                       const Disagreement& disagreement) {
    output << tsv_escape(tag) << '\t' << tsv_escape(kind) << '\t'
           << tsv_escape(disagreement.puzzle_id) << '\t' << tsv_escape(disagreement.fen) << '\t'
           << tsv_escape(disagreement.moves) << '\t' << tsv_escape(disagreement.themes) << '\t'
           << tsv_escape(disagreement.instances) << '\n';
}

void process_row(const std::string& line, std::size_t line_number, std::size_t limit,
                 std::array<Metrics, kTagCount>& metrics,
                 std::array<DisagreementBucket, kTagCount>& disagreements) {
    std::string puzzle_id = first_field(line);
    try {
        std::string_view row(line);
        if (!row.empty() && row.back() == '\r') {
            row.remove_suffix(1);
        }
        const auto fields = parse_csv_record(row);
        if (fields.size() != kCsvColumns.size()) {
            throw std::invalid_argument("expected 5 CSV fields, got " +
                                        std::to_string(fields.size()));
        }
        puzzle_id = fields[0];
        if (puzzle_id.empty()) {
            throw std::invalid_argument("PuzzleId is empty");
        }
        if (fields[4] != "background" &&
            std::ranges::none_of(kMeasuredTags, [&fields](const auto& tag) {
                return fields[4] == tag.lichess_tag;
            })) {
            throw std::invalid_argument("unknown Stratum " + fields[4]);
        }

        chess::Board starting_position;
        if (!starting_position.setFen(fields[1])) {
            throw std::invalid_argument("invalid FEN");
        }
        std::array<std::string_view, 7> move_tokens{};
        std::size_t move_count = 0;
        const std::string_view moves = fields[2];
        std::size_t cursor = 0;
        while (cursor < moves.size()) {
            while (cursor < moves.size() &&
                   std::isspace(static_cast<unsigned char>(moves[cursor])) != 0) {
                ++cursor;
            }
            const std::size_t start = cursor;
            while (cursor < moves.size() &&
                   std::isspace(static_cast<unsigned char>(moves[cursor])) == 0) {
                ++cursor;
            }
            if (start != cursor) {
                if (move_count == move_tokens.size()) {
                    throw std::invalid_argument("Moves has more than seven plies");
                }
                move_tokens[move_count++] = moves.substr(start, cursor - start);
            }
        }
        if (move_count == 0) {
            throw std::invalid_argument("Moves has no first move");
        }

        const chess::Move first_move = parse_legal_uci(starting_position, move_tokens[0]);
        starting_position.makeMove(first_move);
        const chess::Board root = starting_position;
        const Side solver = root.sideToMove() == chess::Color::WHITE ? Side::White : Side::Black;
        std::array<chess::Move, 6> line_moves{};
        std::size_t line_move_count = 0;
        for (std::size_t index = 1; index < move_count; ++index) {
            const chess::Move move = parse_legal_uci(starting_position, move_tokens[index]);
            line_moves[line_move_count++] = move;
            starting_position.makeMove(move);
        }

        std::vector<Motif> motifs = chesswhy::tactics::position_motifs(root);
        auto line_motifs = chesswhy::tactics::line_motifs(
            root, std::span<const chess::Move>(line_moves.data(), line_move_count));
        motifs.insert(motifs.end(), line_motifs.begin(), line_motifs.end());

        const std::array<bool, kTagCount> actual = lichess_tags(fields[3]);
        std::array<bool, kTagCount> predicted{};
        for (const Motif& motif : motifs) {
            if (motif.side != solver) {
                continue;
            }
            for (std::size_t index = 0; index < kTagCount; ++index) {
                if (maps_to(kMeasuredTags[index], motif.tag)) {
                    predicted[index] = true;
                }
            }
        }

        std::optional<std::string> instances;
        for (std::size_t index = 0; index < kTagCount; ++index) {
            Metrics& tag_metrics = metrics[index];
            if (fields[4] == "background") {
                tag_metrics.background_tagged += actual[index] ? 1U : 0U;
                tag_metrics.background_predicted += predicted[index] ? 1U : 0U;
                if (predicted[index] && actual[index]) {
                    ++tag_metrics.background_true_positive;
                } else if (predicted[index]) {
                    ++tag_metrics.background_false_positive;
                    auto& false_positives = disagreements[index].false_positives;
                    if (false_positives.size() < limit) {
                        if (!instances) {
                            instances = compact_instances(motifs);
                        }
                        false_positives.push_back(
                            {puzzle_id, fields[1], fields[2], fields[3], *instances});
                    }
                }
            } else if (fields[4] == kMeasuredTags[index].lichess_tag) {
                ++tag_metrics.stratum_count;
                if (predicted[index]) {
                    ++tag_metrics.stratum_true_positive;
                } else {
                    auto& false_negatives = disagreements[index].false_negatives;
                    if (false_negatives.size() < limit) {
                        if (!instances) {
                            instances = compact_instances(motifs);
                        }
                        false_negatives.push_back(
                            {puzzle_id, fields[1], fields[2], fields[3], *instances});
                    }
                }
            }
        }

    } catch (const std::exception& error) {
        if (puzzle_id.empty()) {
            puzzle_id = "<unknown>";
        }
        throw RowError(std::move(puzzle_id),
                       "CSV row " + std::to_string(line_number) + ": " + error.what());
    }
}

[[nodiscard]] double ratio(std::size_t numerator, std::size_t denominator) {
    if (denominator == 0) {
        return 0.0;
    }
    return static_cast<double>(numerator) / static_cast<double>(denominator);
}

[[nodiscard]] double precision(const Metrics& metrics) {
    return ratio(metrics.background_true_positive,
                 metrics.background_true_positive + metrics.background_false_positive);
}

[[nodiscard]] double recall(const Metrics& metrics) {
    return ratio(metrics.stratum_true_positive, metrics.stratum_count);
}

void print_table(const std::array<Metrics, kTagCount>& metrics) {
    std::cout << std::left << std::setw(22) << "tag" << std::right << std::setw(13) << "bg-tagged"
              << std::setw(13) << "bg-predicted" << std::setw(9) << "bg-TP" << std::setw(9)
              << "bg-FP" << std::setw(12) << "precision" << std::setw(12) << "stratum-n"
              << std::setw(12) << "stratum-TP" << std::setw(10) << "recall" << '\n';
    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t index = 0; index < kTagCount; ++index) {
        const Metrics& value = metrics[index];
        std::cout << std::left << std::setw(22) << kMeasuredTags[index].lichess_tag << std::right
                  << std::setw(13) << value.background_tagged << std::setw(13)
                  << value.background_predicted << std::setw(9) << value.background_true_positive
                  << std::setw(9) << value.background_false_positive << std::setw(12)
                  << precision(value) << std::setw(12) << value.stratum_count << std::setw(12)
                  << value.stratum_true_positive << std::setw(10) << recall(value) << '\n';
    }
}

[[nodiscard]] bool targets_met(const std::array<Metrics, kTagCount>& metrics) {
    for (std::size_t index = 0; index < kTagCount; ++index) {
        const auto& tag = kMeasuredTags[index];
        if (tag.precision_target && precision(metrics[index]) < *tag.precision_target) {
            return false;
        }
        if (tag.recall_target && recall(metrics[index]) < *tag.recall_target) {
            return false;
        }
    }
    return true;
}

void write_disagreements(const std::string& path, std::size_t limit,
                         const std::array<DisagreementBucket, kTagCount>& disagreements) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot open disagreement output " + path);
    }
    output << "tag\tkind\tPuzzleId\tFEN\tMoves\tThemes\tour instances\n";
    for (std::size_t index = 0; index < kTagCount; ++index) {
        const auto& bucket = disagreements[index];
        const std::size_t false_positive_count = std::min(limit, bucket.false_positives.size());
        const std::size_t false_negative_count = std::min(limit, bucket.false_negatives.size());
        for (std::size_t row = 0; row < false_positive_count; ++row) {
            emit_disagreement(output, kMeasuredTags[index].lichess_tag, "FP",
                              bucket.false_positives[row]);
        }
        for (std::size_t row = 0; row < false_negative_count; ++row) {
            emit_disagreement(output, kMeasuredTags[index].lichess_tag, "FN",
                              bucket.false_negatives[row]);
        }
    }
    if (!output) {
        throw std::runtime_error("failed while writing disagreement output " + path);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "tactics_puzzle_eval: " << error.what() << '\n';
        return 2;
    }

    std::ifstream input(options.csv_path);
    if (!input) {
        std::cerr << "tactics_puzzle_eval: cannot open " << options.csv_path << '\n';
        return 2;
    }
    std::string header;
    if (!std::getline(input, header)) {
        std::cerr << "tactics_puzzle_eval: empty CSV " << options.csv_path << '\n';
        return 2;
    }
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    try {
        if (parse_csv_record(header) !=
            std::vector<std::string>(kCsvColumns.begin(), kCsvColumns.end())) {
            std::cerr << "tactics_puzzle_eval: unexpected CSV header\n";
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "tactics_puzzle_eval: invalid CSV header: " << error.what() << '\n';
        return 2;
    }

    std::array<Metrics, kTagCount> metrics{};
    std::array<DisagreementBucket, kTagCount> disagreements{};
    std::string line;
    std::size_t line_number = 1;
    try {
        while (std::getline(input, line)) {
            ++line_number;
            process_row(line, line_number, options.disagreements_per_kind, metrics, disagreements);
        }
        if (input.bad()) {
            throw std::runtime_error("failed reading " + options.csv_path);
        }
        print_table(metrics);
        if (options.disagreement_path) {
            write_disagreements(*options.disagreement_path, options.disagreements_per_kind,
                                disagreements);
        }
    } catch (const RowError& error) {
        std::cerr << "tactics_puzzle_eval: PuzzleId " << error.puzzle_id() << ": " << error.what()
                  << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "tactics_puzzle_eval: " << error.what() << '\n';
        return 2;
    }
    return targets_met(metrics) ? 0 : 1;
}
