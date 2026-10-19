#include "chesswhy/analysis.hpp"
#include "chesswhy/engine.hpp"
#include "chesswhy/tactics.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <chess.hpp>

#ifndef CHESSWHY_ASSETS_DIR
#error "CHESSWHY_ASSETS_DIR must be provided by chesswhy_paths"
#endif

namespace {

using chesswhy::Motif;
using chesswhy::MotifTag;
using chesswhy::Side;

constexpr std::size_t kClaimCount = 7;
constexpr std::uint64_t kSearchNodes = 200'000;
constexpr std::size_t kDefaultPerTag = 200;
constexpr std::array<std::string_view, 11> kKnownStrata{"hangingPiece",
                                                        "fork",
                                                        "pin",
                                                        "skewer",
                                                        "discoveredAttack",
                                                        "discoveredCheck",
                                                        "doubleCheck",
                                                        "backRankMate",
                                                        "capturingDefender",
                                                        "trappedPiece",
                                                        "promotion"};

struct ClaimTag {
    std::string_view name;
    MotifTag motif_tag;
    std::optional<double> precision_target;
};

// Claim order and targets are frozen by M2 §7.4 / §8.2.
constexpr std::array<ClaimTag, kClaimCount> kClaims{{
    {.name = "hanging_piece",
     .motif_tag = MotifTag::HangingPiece,
     .precision_target = std::nullopt},
    {.name = "fork", .motif_tag = MotifTag::Fork, .precision_target = std::nullopt},
    {.name = "skewer", .motif_tag = MotifTag::Skewer, .precision_target = std::nullopt},
    {.name = "pin_relative", .motif_tag = MotifTag::PinRelative, .precision_target = std::nullopt},
    {.name = "discovered_attack",
     .motif_tag = MotifTag::DiscoveredAttack,
     .precision_target = std::nullopt},
    {.name = "removal_of_defender",
     .motif_tag = MotifTag::RemovalOfDefender,
     .precision_target = std::nullopt},
    {.name = "trapped_piece",
     .motif_tag = MotifTag::TrappedPiece,
     .precision_target = std::nullopt},
}};

constexpr std::array<std::string_view, 5> kCsvColumns{"PuzzleId", "FEN", "Moves", "Themes",
                                                      "Stratum"};

struct Options {
    std::string csv_path;
    std::size_t per_tag = kDefaultPerTag;
    std::size_t jobs = 1;
    std::optional<std::string> details_path;
};

class RowError : public std::runtime_error {
public:
    RowError(std::string puzzle_id, const std::string& message)
        : std::runtime_error(message), puzzle_id_(std::move(puzzle_id)) {}

    [[nodiscard]] const std::string& puzzle_id() const { return puzzle_id_; }

private:
    std::string puzzle_id_;
};

[[nodiscard]] std::size_t parse_size(std::string_view text, std::string_view flag,
                                     bool allow_zero) {
    std::size_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        (!allow_zero && value == 0)) {
        throw std::invalid_argument("invalid value for " + std::string(flag) + ": " +
                                    std::string(text));
    }
    return value;
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    if (argc < 2) {
        throw std::invalid_argument(
            "usage: tactics_motif_confirm <csv> [--per-tag N] [--jobs J] [--details <out.tsv>]");
    }

    Options options;
    options.csv_path = argv[1];
    bool per_tag_seen = false;
    bool jobs_seen = false;
    bool details_seen = false;
    for (int index = 2; index < argc; ++index) {
        const std::string_view flag(argv[index]);
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--")) {
            throw std::invalid_argument("missing value for " + std::string(flag));
        }
        const std::string_view value(argv[++index]);
        if (flag == "--per-tag") {
            if (per_tag_seen) {
                throw std::invalid_argument("duplicate flag --per-tag");
            }
            per_tag_seen = true;
            options.per_tag = parse_size(value, flag, true);
        } else if (flag == "--jobs") {
            if (jobs_seen) {
                throw std::invalid_argument("duplicate flag --jobs");
            }
            jobs_seen = true;
            options.jobs = parse_size(value, flag, false);
        } else if (flag == "--details") {
            if (details_seen) {
                throw std::invalid_argument("duplicate flag --details");
            }
            details_seen = true;
            options.details_path = value;
        } else {
            throw std::invalid_argument("unknown option " + std::string(flag));
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

[[nodiscard]] chess::Color color_of(Side side) {
    return side == Side::White ? chess::Color::WHITE : chess::Color::BLACK;
}

[[nodiscard]] chess::Color opposite(chess::Color color) {
    return color == chess::Color::WHITE ? chess::Color::BLACK : chess::Color::WHITE;
}

[[nodiscard]] std::string square_name(std::uint8_t square) {
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + (square % 8));
    result[1] = static_cast<char>('1' + (square / 8));
    return result;
}

[[nodiscard]] std::string instance_name(const Motif& motif) {
    std::ostringstream output;
    output << chesswhy::to_string(motif.tag) << ':' << chesswhy::to_string(motif.side) << ':'
           << motif.ply << ':' << square_name(motif.actor.square) << '>';
    for (std::size_t index = 0; index < motif.targets.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << square_name(motif.targets[index].square);
    }
    return output.str();
}

[[nodiscard]] std::uint64_t fnv1a(std::string_view value) {
    constexpr std::uint64_t kOffsetBasis = 14'695'981'039'346'656'037ULL;
    constexpr std::uint64_t kPrime = 1'099'511'628'211ULL;
    std::uint64_t hash = kOffsetBasis;
    for (const char character : value) {
        hash ^= static_cast<unsigned char>(character);
        hash *= kPrime;
    }
    return hash;
}

[[nodiscard]] std::optional<std::size_t> claim_index(MotifTag tag) {
    for (std::size_t index = 0; index < kClaimCount; ++index) {
        if (kClaims[index].motif_tag == tag) {
            return index;
        }
    }
    return std::nullopt;
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

struct Sample {
    std::uint64_t hash = 0;
    std::string puzzle_id;
    std::string instance;
    chess::Board board;
    Side x_side = Side::White;
    std::vector<chess::Square> tracked;
    std::size_t tag_index = 0;
};

using Population = std::array<std::vector<Sample>, kClaimCount>;

void add_tracked_piece(std::vector<chess::Square>& tracked, const chess::Board& board,
                       std::uint8_t square_index, chess::Color y_color) {
    const chess::Square square{static_cast<int>(square_index)};
    const chess::Piece piece = board.at(square);
    if (piece == chess::Piece::NONE || piece.color() != y_color ||
        std::ranges::find(tracked, square) != tracked.end()) {
        return;
    }
    tracked.push_back(square);
}

void add_motif_sample(const std::string& puzzle_id, const chess::Board& root,
                      std::span<const chess::Move> line_moves, const Motif& motif,
                      Population& population) {
    const auto index = claim_index(motif.tag);
    if (!index) {
        return;
    }
    if (motif.ply < 0 || static_cast<std::size_t>(motif.ply) > line_moves.size()) {
        throw std::invalid_argument("motif ply is outside the puzzle line");
    }

    chess::Board board = root;
    for (int ply = 0; ply < motif.ply; ++ply) {
        board.makeMove(line_moves[static_cast<std::size_t>(ply)]);
    }

    Sample sample;
    sample.puzzle_id = puzzle_id;
    sample.instance = instance_name(motif);
    sample.board = board;
    sample.x_side = motif.side;
    sample.tag_index = *index;
    const chess::Color y_color = opposite(color_of(motif.side));
    add_tracked_piece(sample.tracked, board, motif.actor.square, y_color);
    for (const auto& target : motif.targets) {
        add_tracked_piece(sample.tracked, board, target.square, y_color);
    }

    std::string hash_input = "chesswhy-m2-confirm:";
    hash_input.reserve(hash_input.size() + puzzle_id.size() + sample.instance.size() + 1);
    hash_input += puzzle_id;
    hash_input.push_back(':');
    hash_input += sample.instance;
    sample.hash = fnv1a(hash_input);
    population[*index].push_back(std::move(sample));
}

void process_row(const std::string& line, std::size_t line_number, Population& population) {
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
            std::ranges::find(kKnownStrata, std::string_view(fields[4])) == kKnownStrata.end()) {
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
        std::array<chess::Move, 6> line_moves{};
        std::size_t line_move_count = 0;
        for (std::size_t index = 1; index < move_count; ++index) {
            const chess::Move move = parse_legal_uci(starting_position, move_tokens[index]);
            line_moves[line_move_count++] = move;
            starting_position.makeMove(move);
        }
        if (fields[4] != "background") {
            return;
        }

        std::vector<Motif> motifs = chesswhy::tactics::position_motifs(root);
        auto along_line = chesswhy::tactics::line_motifs(
            root, std::span<const chess::Move>(line_moves.data(), line_move_count));
        motifs.insert(motifs.end(), along_line.begin(), along_line.end());
        const auto line = std::span<const chess::Move>(line_moves.data(), line_move_count);
        for (const Motif& motif : motifs) {
            add_motif_sample(puzzle_id, root, line, motif, population);
        }
    } catch (const std::exception& error) {
        if (puzzle_id.empty()) {
            puzzle_id = "<unknown>";
        }
        throw RowError(std::move(puzzle_id),
                       "CSV row " + std::to_string(line_number) + ": " + error.what());
    }
}

void sort_population(Population& population) {
    for (auto& samples : population) {
        std::ranges::sort(samples, [](const Sample& left, const Sample& right) {
            if (left.hash != right.hash) {
                return left.hash < right.hash;
            }
            if (left.puzzle_id != right.puzzle_id) {
                return left.puzzle_id < right.puzzle_id;
            }
            return left.instance < right.instance;
        });
    }
}

enum class Outcome : std::uint8_t { Confirmed, NoHit, NoGain, Mate, Stalemate };

struct Confirmation {
    Outcome outcome = Outcome::NoHit;
    bool confirmed = false;
    std::string pv;
};

[[nodiscard]] bool captures_tracked_piece(const Sample& sample, const chesswhy::engine::Line& line,
                                          int plies) {
    chess::Board board = sample.board;
    std::vector<chess::Square> tracked = sample.tracked;
    const chess::Color x_color = color_of(sample.x_side);
    bool hit = false;

    for (int index = 0; index < plies; ++index) {
        const chess::Move move =
            parse_legal_uci(board, line.pv_uci[static_cast<std::size_t>(index)]);
        const chess::Color mover = board.sideToMove();
        if (board.isCapture(move)) {
            chess::Square captured_square = move.to();
            if (move.typeOf() == chess::Move::ENPASSANT) {
                captured_square = move.to().ep_square();
            }
            const auto captured = std::ranges::find(tracked, captured_square);
            if (captured != tracked.end()) {
                hit = hit || mover == x_color;
                tracked.erase(captured);
            }
        }
        if (mover != x_color) {
            const auto moved = std::ranges::find(tracked, move.from());
            if (moved != tracked.end()) {
                *moved = move.to();
            }
        }
        board.makeMove(move);
    }
    return hit;
}

[[nodiscard]] std::string pv_string(const chesswhy::engine::Line& line) {
    std::ostringstream output;
    for (std::size_t index = 0; index < line.pv_uci.size(); ++index) {
        if (index != 0) {
            output << ' ';
        }
        output << line.pv_uci[index];
    }
    return output.str();
}

[[nodiscard]] Confirmation confirm(chesswhy::engine::Engine& engine, const Sample& sample,
                                   bool include_pv) {
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, sample.board);
    if (legal_moves.empty()) {
        if (!sample.board.inCheck()) {
            return {.outcome = Outcome::Stalemate, .confirmed = false, .pv = {}};
        }
        const chess::Color y_color = opposite(color_of(sample.x_side));
        if (sample.board.sideToMove() == y_color) {
            return {.outcome = Outcome::Mate, .confirmed = true, .pv = {}};
        }
        return {.outcome = Outcome::NoHit, .confirmed = false, .pv = {}};
    }

    chesswhy::engine::SearchRequest request;
    request.fen = sample.board.getFen();
    request.nodes = kSearchNodes;
    request.multipv = 1;
    const auto searched = engine.search(request);
    if (searched.lines.empty()) {
        throw chesswhy::engine::EngineError("Stockfish returned no principal variation");
    }
    const chesswhy::engine::Line& line = searched.lines.front();
    const int plies = chesswhy::analysis::walked_plies(sample.board, line.pv_uci);
    const auto consequence = chesswhy::analysis::consequence(sample.board, line.pv_uci, line);
    const std::string pv = include_pv ? pv_string(line) : std::string{};
    const bool hit = captures_tracked_piece(sample, line, plies);
    if (!hit) {
        return {.outcome = Outcome::NoHit, .confirmed = false, .pv = pv};
    }

    const chess::Color x_color = color_of(sample.x_side);
    const bool x_to_move = sample.board.sideToMove() == x_color;
    const int x_material = x_to_move ? consequence.material : -consequence.material;
    const bool mate_for_x =
        line.mate.has_value() && *line.mate != 0 && ((*line.mate > 0) == x_to_move);
    if (mate_for_x) {
        return {.outcome = Outcome::Mate, .confirmed = true, .pv = pv};
    }
    if (x_material >= 1) {
        return {.outcome = Outcome::Confirmed, .confirmed = true, .pv = pv};
    }
    return {.outcome = Outcome::NoGain, .confirmed = false, .pv = pv};
}

[[nodiscard]] std::vector<Sample> selected_samples(Population& population, std::size_t per_tag) {
    std::size_t selected_count = 0;
    for (const auto& samples : population) {
        selected_count += std::min(per_tag, samples.size());
    }
    std::vector<Sample> selected;
    selected.reserve(selected_count);
    for (auto& samples : population) {
        const std::size_t count = std::min(per_tag, samples.size());
        for (std::size_t index = 0; index < count; ++index) {
            selected.push_back(std::move(samples[index]));
        }
    }
    return selected;
}

void run_jobs(const std::vector<Sample>& samples, std::size_t jobs, bool include_pv,
              std::vector<Confirmation>& results) {
    const chesswhy::engine::Options engine_options{
        .binary = std::filesystem::path(CHESSWHY_ASSETS_DIR) / "engines" / "stockfish",
        .threads = 1,
        .hash_mb = 64};
    std::atomic<std::size_t> next{0};
    std::atomic<bool> stop{false};
    std::mutex failure_mutex;
    std::exception_ptr failure;
    std::vector<std::jthread> workers;
    workers.reserve(jobs);

    for (std::size_t worker_index = 0; worker_index < jobs; ++worker_index) {
        workers.emplace_back([&] {
            try {
                chesswhy::engine::Engine engine(engine_options);
                while (!stop.load(std::memory_order_relaxed)) {
                    const std::size_t sample_index = next.fetch_add(1, std::memory_order_relaxed);
                    if (sample_index >= samples.size()) {
                        break;
                    }
                    try {
                        results[sample_index] = confirm(engine, samples[sample_index], include_pv);
                    } catch (const std::exception& error) {
                        throw std::runtime_error("PuzzleId " + samples[sample_index].puzzle_id +
                                                 ", instance " + samples[sample_index].instance +
                                                 ": " + error.what());
                    }
                }
            } catch (...) {
                const std::lock_guard lock(failure_mutex);
                if (!failure) {
                    failure = std::current_exception();
                }
                stop.store(true, std::memory_order_relaxed);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

struct Metrics {
    std::size_t sampled = 0;
    std::size_t confirmed = 0;
    std::size_t no_hit = 0;
    std::size_t hit_no_gain = 0;
};

[[nodiscard]] double ratio(std::size_t numerator, std::size_t denominator) {
    if (denominator == 0) {
        return 0.0;
    }
    return static_cast<double>(numerator) / static_cast<double>(denominator);
}

[[nodiscard]] double precision(const Metrics& metrics) {
    return ratio(metrics.confirmed, metrics.sampled);
}

[[nodiscard]] bool targets_met(const std::array<Metrics, kClaimCount>& metrics) {
    for (std::size_t index = 0; index < kClaimCount; ++index) {
        const auto precision_target = kClaims[index].precision_target;
        if (precision_target.has_value() && precision(metrics[index]) < precision_target.value()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::array<Metrics, kClaimCount> measure(const std::vector<Sample>& samples,
                                                       const std::vector<Confirmation>& results) {
    std::array<Metrics, kClaimCount> metrics{};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        Metrics& value = metrics[samples[index].tag_index];
        const Confirmation& result = results[index];
        ++value.sampled;
        value.confirmed += result.confirmed ? 1U : 0U;
        value.no_hit +=
            result.outcome == Outcome::NoHit || result.outcome == Outcome::Stalemate ? 1U : 0U;
        value.hit_no_gain += result.outcome == Outcome::NoGain ? 1U : 0U;
    }
    return metrics;
}

void print_table(const std::array<Metrics, kClaimCount>& metrics) {
    std::cout << std::left << std::setw(22) << "tag" << std::right << std::setw(12) << "sampled"
              << std::setw(12) << "confirmed" << std::setw(12) << "no-hit" << std::setw(15)
              << "hit-no-gain" << std::setw(22) << "confirmed-precision" << '\n';
    std::cout << std::fixed << std::setprecision(3);
    for (std::size_t index = 0; index < kClaimCount; ++index) {
        const Metrics& value = metrics[index];
        std::cout << std::left << std::setw(22) << kClaims[index].name << std::right
                  << std::setw(12) << value.sampled << std::setw(12) << value.confirmed
                  << std::setw(12) << value.no_hit << std::setw(15) << value.hit_no_gain
                  << std::setw(22) << precision(value) << '\n';
    }
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

[[nodiscard]] std::string_view outcome_name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Confirmed:
        return "confirmed";
    case Outcome::NoHit:
        return "no_hit";
    case Outcome::NoGain:
        return "no_gain";
    case Outcome::Mate:
        return "mate";
    case Outcome::Stalemate:
        return "stalemate";
    }
    return "no_hit";
}

void write_details(const std::string& path, const std::vector<Sample>& samples,
                   const std::vector<Confirmation>& results) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot open details output " + path);
    }
    output << "PuzzleId\tinstance\tFEN\tpv\toutcome\n";
    for (std::size_t index = 0; index < samples.size(); ++index) {
        output << tsv_escape(samples[index].puzzle_id) << '\t'
               << tsv_escape(samples[index].instance) << '\t'
               << tsv_escape(samples[index].board.getFen()) << '\t' << tsv_escape(results[index].pv)
               << '\t' << outcome_name(results[index].outcome) << '\n';
    }
    if (!output) {
        throw std::runtime_error("failed while writing details output " + path);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "tactics_motif_confirm: " << error.what() << '\n';
        return 2;
    }

    std::ifstream input(options.csv_path);
    if (!input) {
        std::cerr << "tactics_motif_confirm: cannot open " << options.csv_path << '\n';
        return 2;
    }
    std::string header;
    if (!std::getline(input, header)) {
        std::cerr << "tactics_motif_confirm: empty CSV " << options.csv_path << '\n';
        return 2;
    }
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    try {
        if (parse_csv_record(header) !=
            std::vector<std::string>(kCsvColumns.begin(), kCsvColumns.end())) {
            std::cerr << "tactics_motif_confirm: unexpected CSV header\n";
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "tactics_motif_confirm: invalid CSV header: " << error.what() << '\n';
        return 2;
    }

    Population population;
    std::string line;
    std::size_t line_number = 1;
    try {
        while (std::getline(input, line)) {
            ++line_number;
            process_row(line, line_number, population);
        }
        if (input.bad()) {
            throw std::runtime_error("failed reading " + options.csv_path);
        }
    } catch (const RowError& error) {
        std::cerr << "tactics_motif_confirm: PuzzleId " << error.puzzle_id() << ": " << error.what()
                  << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "tactics_motif_confirm: " << error.what() << '\n';
        return 2;
    }

    sort_population(population);
    const std::vector<Sample> samples = selected_samples(population, options.per_tag);
    std::vector<Confirmation> results(samples.size());
    try {
        run_jobs(samples, options.jobs, options.details_path.has_value(), results);
        const auto metrics = measure(samples, results);
        if (options.details_path) {
            write_details(*options.details_path, samples, results);
        }
        print_table(metrics);
        return targets_met(metrics) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "tactics_motif_confirm: " << error.what() << '\n';
        return 2;
    }
}
