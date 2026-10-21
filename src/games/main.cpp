#include "chesswhy/games.hpp"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int parse_integer(std::string_view value, std::string_view option) {
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || error != std::errc{} || end != value.data() + value.size()) {
        throw chesswhy::games::GamesError("invalid value for " + std::string(option));
    }
    return parsed;
}

chesswhy::games::BuildOptions parse_options(int argc, char* argv[]) {
    chesswhy::games::BuildOptions options;
    options.months = "2020-01..2026-09";
    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (index + 1 == argc) {
            throw chesswhy::games::GamesError("missing value for " + std::string(option));
        }
        const std::string_view value(argv[++index]);
        if (option == "--max-ply") {
            options.max_ply = parse_integer(value, option);
        } else if (option == "--min-games") {
            options.min_games = parse_integer(value, option);
        } else if (option == "--months") {
            options.months = value;
        } else {
            throw chesswhy::games::GamesError("unknown option " + std::string(option));
        }
    }
    if (options.max_ply < 0 || options.min_games < 1) {
        throw chesswhy::games::GamesError(
            "max-ply must be non-negative and min-games must be positive");
    }
    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        chesswhy::games::build_table(std::cin, std::cout, std::cerr, options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "chesswhy-games: error: " << error.what() << '\n';
        return 1;
    }
}
