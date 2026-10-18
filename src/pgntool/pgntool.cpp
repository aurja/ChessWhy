#include "pgntool.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <chesswhy/input.hpp>

namespace chesswhy::pgntool {
namespace {

constexpr std::string_view kUsage = "usage: chesswhy-pgn [--game N] FILE\n"
                                    "       chesswhy-pgn --games FILE\n"
                                    "       chesswhy-pgn --help\n";

std::string_view tag_value(const input::PgnGame& game, std::string_view name) {
    for (const auto& [key, value] : game.headers) {
        if (key == name) {
            return value;
        }
    }
    return {};
}

bool parse_game_number(std::string_view text, int& game) {
    if (text.empty()) {
        return false;
    }
    const auto* first = text.data();
    const auto* last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, game);
    return error == std::errc{} && end == last;
}

int report_error(std::string_view message, std::ostream& err) {
    err << "chesswhy-pgn: error: " << message << '\n';
    return 2;
}

void print_games(const std::vector<input::PgnGame>& games, std::ostream& output) {
    output << "game\twhite\tblack\tevent\tdate\tresult\tplies\n";
    for (const auto& game : games) {
        output << game.game << '\t' << tag_value(game, "White") << '\t' << tag_value(game, "Black")
               << '\t' << tag_value(game, "Event") << '\t' << tag_value(game, "Date") << '\t'
               << tag_value(game, "Result") << '\t' << game.moves.size() << '\n';
    }
}

void print_moves(const input::PgnGame& game, std::ostream& output) {
    auto board = game.start;
    for (const auto& move : game.moves) {
        output << game.game << '\t' << move.ply << '\t' << board.fullMoveNumber() << '\t'
               << (board.sideToMove() == chess::Color::WHITE ? "white" : "black") << '\t'
               << move.san << '\t' << chess::uci::moveToUci(move.move) << '\t';
        for (std::size_t index = 0; index < move.nags.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output << move.nags[index];
        }
        output << '\t' << board.getFen();
        board.makeMove(move.move);
        output << '\t' << board.getFen() << '\n';
    }
}

} // namespace

int run(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (std::ranges::find(args, std::string_view{"--help"}) != args.end()) {
        out << kUsage;
        return 0;
    }

    bool list_games = false;
    bool game_seen = false;
    std::optional<int> requested_game;
    std::string_view path;
    bool have_path = false;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto arg = args[index];
        if (arg == "--games") {
            if (list_games) {
                return report_error("--games given twice", err);
            }
            if (game_seen) {
                return report_error("--games cannot be used with --game", err);
            }
            list_games = true;
        } else if (arg == "--game") {
            if (game_seen) {
                return report_error("--game given twice", err);
            }
            if (list_games) {
                return report_error("--games cannot be used with --game", err);
            }
            game_seen = true;
            if (index + 1 == args.size() || args[index + 1] == "--game" ||
                args[index + 1] == "--games") {
                return report_error("--game requires a value", err);
            }
            const auto value = args[++index];
            if (value.starts_with("--")) {
                return report_error("unknown flag " + std::string(value), err);
            }
            int game = 0;
            if (!parse_game_number(value, game)) {
                return report_error("invalid game number " + std::string(value), err);
            }
            if (game < 1) {
                return report_error("game " + std::to_string(game) + " must be 1-based", err);
            }
            requested_game = game;
        } else if (arg.starts_with("--")) {
            return report_error("unknown flag " + std::string(arg), err);
        } else if (have_path) {
            return report_error("unexpected positional argument " + std::string(arg), err);
        } else {
            path = arg;
            have_path = true;
        }
    }

    if (!have_path) {
        return report_error("missing FILE", err);
    }

    try {
        const auto games = input::read_games(std::filesystem::path(path));
        std::ostringstream output;
        if (list_games) {
            print_games(games, output);
        } else {
            output << "game\tply\tmove_number\tside\tsan\tuci\tnags\tfen_before\tfen_after\n";
            if (requested_game.has_value()) {
                if (static_cast<std::size_t>(*requested_game) > games.size()) {
                    return report_error("game " + std::to_string(*requested_game) +
                                            " not found in " + std::filesystem::path(path).string(),
                                        err);
                }
                print_moves(games[static_cast<std::size_t>(*requested_game - 1)], output);
            } else {
                for (const auto& game : games) {
                    print_moves(game, output);
                }
            }
        }
        out << output.str();
        return 0;
    } catch (const input::InputError& error) {
        return report_error(error.what(), err);
    } catch (const std::exception& error) {
        return report_error(error.what(), err);
    }
}

} // namespace chesswhy::pgntool
