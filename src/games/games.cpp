#include "chesswhy/games.hpp"

#include "chesswhy/input.hpp"
#include "chesswhy/openings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace chesswhy::games {
namespace {

struct Exclusions {
    int variant = 0;
    int setup = 0;
    int rating = 0;
    int result = 0;
    int illegal = 0;
};

struct PositionCounts {
    int games = 0;
    int white = 0;
    int draws = 0;
    int black = 0;
    std::unordered_map<std::string, int> moves;
};

std::optional<std::string_view> header_value(const input::PgnGame& game, std::string_view name) {
    for (const auto& [key, value] : game.headers) {
        if (key == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool has_header(const input::PgnGame& game, std::string_view name) {
    return header_value(game, name).has_value();
}

bool rated_2200(const input::PgnGame& game) {
    const auto white = header_value(game, "WhiteElo");
    const auto black = header_value(game, "BlackElo");
    if (!white.has_value() || !black.has_value()) {
        return false;
    }
    int white_rating = 0;
    int black_rating = 0;
    const auto [white_end, white_error] =
        std::from_chars(white->data(), white->data() + white->size(), white_rating);
    const auto [black_end, black_error] =
        std::from_chars(black->data(), black->data() + black->size(), black_rating);
    return white_error == std::errc{} && white_end == white->data() + white->size() &&
           black_error == std::errc{} && black_end == black->data() + black->size() &&
           white_rating >= kRatingFloor && black_rating >= kRatingFloor;
}

int parse_count(std::string_view text, std::string_view field) {
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() || value < 0) {
        throw GamesError("invalid " + std::string(field) + " count");
    }
    return value;
}

int require_header_count(std::istream& input, std::string_view prefix) {
    std::string line;
    if (!std::getline(input, line) || !line.starts_with(prefix)) {
        throw GamesError("malformed games table header");
    }
    return parse_count(std::string_view(line).substr(prefix.size()), prefix);
}

std::string require_header_value(std::istream& input, std::string_view prefix) {
    std::string line;
    if (!std::getline(input, line) || !line.starts_with(prefix) ||
        line.find('\r') != std::string::npos) {
        throw GamesError("malformed games table header");
    }
    return line.substr(prefix.size());
}

void require_header_line(std::istream& input, std::string_view expected) {
    std::string line;
    if (!std::getline(input, line) || line != expected) {
        throw GamesError("malformed games table header");
    }
}

Exclusions parse_exclusions(std::string_view text) {
    std::istringstream fields{std::string(text)};
    Exclusions result;
    std::array<std::pair<std::string_view, int*>, 5> values{{
        {"variant=", &result.variant},
        {"setup=", &result.setup},
        {"rating=", &result.rating},
        {"result=", &result.result},
        {"illegal=", &result.illegal},
    }};
    for (const auto& [prefix, value] : values) {
        std::string field;
        if (!(fields >> field) || !field.starts_with(prefix)) {
            throw GamesError("malformed games table exclusion counts");
        }
        *value = parse_count(std::string_view(field).substr(prefix.size()), "excluded game");
    }
    std::string extra;
    if (fields >> extra) {
        throw GamesError("malformed games table exclusion counts");
    }
    return result;
}

bool is_uci_token(std::string_view text) {
    const auto is_file = [](char value) {
        return value >= 'a' && value <= 'h';
    };
    const auto is_rank = [](char value) {
        return value >= '1' && value <= '8';
    };
    if ((text.size() != 4 && text.size() != 5) || !is_file(text[0]) || !is_rank(text[1]) ||
        !is_file(text[2]) || !is_rank(text[3])) {
        return false;
    }
    return text.size() == 4 || text[4] == 'q' || text[4] == 'r' || text[4] == 'b' || text[4] == 'n';
}

std::array<std::string_view, 6> split_row(std::string_view line) {
    std::array<std::string_view, 6> fields{};
    std::size_t start = 0;
    for (std::size_t index = 0; index + 1 < fields.size(); ++index) {
        const auto tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            throw GamesError("malformed games table row");
        }
        fields[index] = line.substr(start, tab - start);
        start = tab + 1;
    }
    if (line.find('\t', start) != std::string_view::npos) {
        throw GamesError("malformed games table row");
    }
    fields.back() = line.substr(start);
    return fields;
}

std::vector<std::pair<std::string, int>> parse_moves(std::string_view text, int games) {
    if (text.empty()) {
        throw GamesError("games table row has no moves");
    }
    std::vector<std::pair<std::string, int>> moves;
    std::string_view previous_uci;
    int previous_count = 0;
    long long move_total = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto comma = text.find(',', start);
        const auto end = comma == std::string_view::npos ? text.size() : comma;
        const auto pair = text.substr(start, end - start);
        const auto colon = pair.find(':');
        if (pair.empty() || colon == std::string_view::npos ||
            pair.find(':', colon + 1) != std::string_view::npos) {
            throw GamesError("malformed games table move count");
        }
        const auto uci = pair.substr(0, colon);
        if (!is_uci_token(uci)) {
            throw GamesError("malformed UCI move in games table row");
        }
        const int count = parse_count(pair.substr(colon + 1), "move");
        if (count == 0 || (!moves.empty() && (count > previous_count ||
                                              (count == previous_count && uci <= previous_uci)))) {
            throw GamesError("unsorted or duplicate games table moves");
        }
        for (const auto& previous_move : moves) {
            if (previous_move.first == uci) {
                throw GamesError("duplicate games table move");
            }
        }
        move_total += count;
        moves.emplace_back(uci, count);
        previous_uci = moves.back().first;
        previous_count = count;
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    if (move_total != games) {
        throw GamesError("games table move counts do not sum to games");
    }
    return moves;
}

} // namespace

void build_table(std::istream& in, std::ostream& out, std::ostream& log,
                 const BuildOptions& options) {
    if (options.max_ply < 0 || options.min_games < 1) {
        throw GamesError("max-ply must be non-negative and min-games must be positive");
    }

    input::PgnReader reader(in, {.max_plies = options.max_ply});
    std::map<std::string, PositionCounts> positions;
    Exclusions excluded;
    int games_read = 0;
    int games_kept = 0;

    while (auto result = reader.next()) {
        ++games_read;
        const auto& game = result->game;
        if (const auto variant = header_value(game, "Variant");
            variant.has_value() && *variant != "Standard") {
            ++excluded.variant;
            continue;
        }
        if (has_header(game, "FEN") || has_header(game, "SetUp")) {
            ++excluded.setup;
            continue;
        }
        if (!rated_2200(game)) {
            ++excluded.rating;
            continue;
        }
        const auto game_result = header_value(game, "Result");
        if (!game_result.has_value() ||
            (*game_result != "1-0" && *game_result != "0-1" && *game_result != "1/2-1/2")) {
            ++excluded.result;
            continue;
        }
        if (result->error.has_value()) {
            ++excluded.illegal;
            continue;
        }

        ++games_kept;
        int result_index = 0;
        if (*game_result == "1-0") {
            result_index = 1;
        } else if (*game_result == "1/2-1/2") {
            result_index = 2;
        } else {
            result_index = 3;
        }

        chess::Board board = game.start;
        std::unordered_set<std::string> seen;
        seen.reserve(game.moves.size());
        for (const auto& move : game.moves) {
            std::string key = openings::position_key(board);
            if (seen.insert(key).second) {
                auto& position = positions[std::move(key)];
                ++position.games;
                if (result_index == 1) {
                    ++position.white;
                } else if (result_index == 2) {
                    ++position.draws;
                } else {
                    ++position.black;
                }
                ++position.moves[chess::uci::moveToUci(move.move)];
            }
            board.makeMove(move.move);
        }
    }

    std::vector<std::pair<std::string, const PositionCounts*>> kept_positions;
    kept_positions.reserve(positions.size());
    for (const auto& [key, position] : positions) {
        if (position.games >= options.min_games) {
            kept_positions.emplace_back(key, &position);
        }
    }

    out << "# source " << kSourceId << '\n'
        << "# platform " << kPlatform << '\n'
        << "# players " << kPlayers << '\n'
        << "# months " << options.months << '\n'
        << "# credit " << kCredit << '\n'
        << "# max_ply " << options.max_ply << '\n'
        << "# min_games " << options.min_games << '\n'
        << "# games_read " << games_read << '\n'
        << "# excluded variant=" << excluded.variant << " setup=" << excluded.setup
        << " rating=" << excluded.rating << " result=" << excluded.result
        << " illegal=" << excluded.illegal << '\n'
        << "# games_kept " << games_kept << '\n'
        << "# positions " << kept_positions.size() << '\n';

    for (const auto& [key, position] : kept_positions) {
        out << key << '\t' << position->games << '\t' << position->white << '\t' << position->draws
            << '\t' << position->black << '\t';
        std::vector<std::pair<std::string, int>> moves(position->moves.begin(),
                                                       position->moves.end());
        std::sort(moves.begin(), moves.end(), [](const auto& left, const auto& right) {
            return left.second != right.second ? left.second > right.second
                                               : left.first < right.first;
        });
        for (std::size_t index = 0; index < moves.size(); ++index) {
            if (index != 0) {
                out << ',';
            }
            out << moves[index].first << ':' << moves[index].second;
        }
        out << '\n';
    }
    if (!out) {
        throw GamesError("cannot write games table");
    }

    log << "games_read " << games_read << '\n'
        << "excluded variant=" << excluded.variant << " setup=" << excluded.setup
        << " rating=" << excluded.rating << " result=" << excluded.result
        << " illegal=" << excluded.illegal << '\n'
        << "games_kept " << games_kept << '\n'
        << "positions " << kept_positions.size() << '\n';
    if (!log) {
        throw GamesError("cannot write games summary");
    }
}

Table Table::load(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open()) {
        throw GamesError("cannot read games table " + file.string());
    }

    require_header_line(input, std::string("# source ") + kSourceId);
    require_header_line(input, std::string("# platform ") + kPlatform);
    require_header_line(input, std::string("# players ") + kPlayers);
    const auto months = require_header_value(input, "# months ");
    require_header_line(input, std::string("# credit ") + kCredit);
    const int max_ply = require_header_count(input, "# max_ply ");
    const int min_games = require_header_count(input, "# min_games ");
    const int games_read = require_header_count(input, "# games_read ");
    const auto exclusions_text = require_header_value(input, "# excluded ");
    const auto excluded = parse_exclusions(exclusions_text);
    const int games_kept = require_header_count(input, "# games_kept ");
    const int positions = require_header_count(input, "# positions ");
    if (min_games < 1 || static_cast<long long>(excluded.variant) + excluded.setup +
                                 excluded.rating + excluded.result + excluded.illegal +
                                 games_kept !=
                             games_read) {
        throw GamesError("inconsistent games table header counts");
    }

    Table table;
    table.header_ = {.id = kSourceId,
                     .table = kTable,
                     .platform = kPlatform,
                     .players = kPlayers,
                     .months = months,
                     .credit = kCredit,
                     .max_ply = max_ply,
                     .min_games = min_games,
                     .found = false,
                     .games = std::nullopt,
                     .white = std::nullopt,
                     .draws = std::nullopt,
                     .black = std::nullopt,
                     .moves = {}};
    table.rows_.reserve(static_cast<std::size_t>(positions));
    std::string line;
    std::string_view previous_key;
    int loaded_positions = 0;
    while (std::getline(input, line)) {
        const auto fields = split_row(line);
        const auto key = fields[0];
        if (key.empty() || (!previous_key.empty() && key <= previous_key)) {
            throw GamesError("unsorted or duplicate games table key");
        }
        const int games = parse_count(fields[1], "games");
        const int white = parse_count(fields[2], "white");
        const int draws = parse_count(fields[3], "draws");
        const int black = parse_count(fields[4], "black");
        if (games < min_games || games == 0 || games > games_kept ||
            static_cast<long long>(white) + draws + black != games) {
            throw GamesError("inconsistent games table result counts");
        }
        auto moves = parse_moves(fields[5], games);
        auto [row, inserted] =
            table.rows_.emplace(std::string(key), Row{.games = games,
                                                      .white = white,
                                                      .draws = draws,
                                                      .black = black,
                                                      .moves = std::move(moves)});
        if (!inserted) {
            throw GamesError("duplicate games table key");
        }
        previous_key = row->first;
        ++loaded_positions;
    }
    if (input.bad()) {
        throw GamesError("cannot read games table " + file.string());
    }
    if (loaded_positions != positions) {
        throw GamesError("games table position count does not match header");
    }
    return table;
}

GameSourceCounts Table::find(const chess::Board& board) const {
    GameSourceCounts result = header_;
    const auto found = rows_.find(openings::position_key(board));
    if (found == rows_.end()) {
        return result;
    }

    result.found = true;
    result.games = found->second.games;
    result.white = found->second.white;
    result.draws = found->second.draws;
    result.black = found->second.black;
    result.moves.reserve(found->second.moves.size());
    for (const auto& [uci, count] : found->second.moves) {
        chess::Move move;
        try {
            move = input::parse_move(board, uci);
        } catch (const input::InputError&) {
            throw GamesError("illegal move in games table row");
        }
        result.moves.push_back(
            {.san = chess::uci::moveToSan(board, move), .uci = uci, .games = count});
    }
    return result;
}

} // namespace chesswhy::games
