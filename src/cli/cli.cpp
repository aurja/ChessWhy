#include "chesswhy/cli.hpp"

#include "chesswhy/analysis.hpp"
#include "chesswhy/engine.hpp"
#include "chesswhy/games.hpp"
#include "chesswhy/input.hpp"
#include "chesswhy/openings.hpp"

#include <charconv>
#include <exception>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "selection.hpp"

#ifndef CHESSWHY_ASSETS_DIR
#error "CHESSWHY_ASSETS_DIR must be provided by chesswhy_paths"
#endif

namespace chesswhy::cli {
namespace {

[[nodiscard]] std::filesystem::path assets_path(std::string_view suffix) {
    return std::filesystem::path(CHESSWHY_ASSETS_DIR) / suffix;
}

[[nodiscard]] std::string usage() {
    return "Usage: chesswhy (--fen FEN | --pgn FILE [--game N] [--ply N]) [--moves LIST]\n"
           "                [--search-budget N (1-60; default 18)]\n"
           "                [--explanation-budget N (1-60; default 14)]\n"
           "                [--json] [--full-report] [--no-llm] [--threads N] [--engine PATH]\n"
           "                [--openings DIR] [--games FILE] [--help]\n"
           "--moves items (comma-separated, no spaces): SAN/UCI move, best, topN,\n"
           "              playable, inaccuracies, mistakes, blunders, unplayable, all. Each "
           "whole-board keyword searches every legal move and takes "
           "several times the default time.\n";
}

[[noreturn]] void usage_error(std::string_view message) {
    throw input::InputError(std::string(message));
}

[[nodiscard]] std::string_view value_after(std::span<const std::string_view> args,
                                           std::size_t& index, std::string_view flag) {
    if (index + 1 >= args.size() || args[index + 1].starts_with("--")) {
        usage_error("missing value for " + std::string(flag));
    }
    ++index;
    return args[index];
}

[[nodiscard]] int parse_integer(std::string_view token, std::string_view flag, int minimum,
                                int maximum) {
    int value = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
        value < minimum || value > maximum) {
        usage_error("invalid integer for " + std::string(flag) + ": " + std::string(token));
    }
    return value;
}

void reject_duplicate(bool& seen, std::string_view flag) {
    if (seen) {
        usage_error("duplicate flag " + std::string(flag));
    }
    seen = true;
}

void write_error(std::ostream& err, std::string_view message) {
    err << "chesswhy: error: ";
    for (const char character : message) {
        if (character == '\n' || character == '\r') {
            err << ' ';
        } else {
            err << character;
        }
    }
    err << '\n';
}

void validate_options(const Options& options) {
    if (options.fen.has_value() == options.pgn.has_value()) {
        throw input::InputError("exactly one of --fen or --pgn must be supplied");
    }
    if (!options.pgn && (options.ply || options.game != 1)) {
        throw input::InputError("--game and --ply may only be used with --pgn");
    }
    if (options.threads < 1) {
        throw input::InputError("--threads must be a positive integer");
    }
}

} // namespace

Options parse_args(std::span<const std::string_view> args) {
    Options options;
    options.engine = assets_path("engines/stockfish");
    options.openings = assets_path("data/openings");
    options.games = assets_path("data/games/lichess_broadcast_2200.tsv");

    bool seen_game = false;
    bool seen_json = false;
    bool seen_moves = false;
    bool seen_full_report = false;
    bool seen_no_llm = false;
    bool seen_threads = false;
    bool seen_engine = false;
    bool seen_openings = false;
    bool seen_games = false;
    bool seen_search_budget = false;
    bool seen_explanation_budget = false;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string_view flag = args[index];
        if (flag == "--fen") {
            if (options.fen) {
                usage_error("duplicate flag --fen");
            }
            options.fen = std::string(value_after(args, index, flag));
        } else if (flag == "--pgn") {
            if (options.pgn) {
                usage_error("duplicate flag --pgn");
            }
            options.pgn = std::filesystem::path(value_after(args, index, flag));
        } else if (flag == "--game") {
            reject_duplicate(seen_game, flag);
            options.game = parse_integer(value_after(args, index, flag), flag, 1,
                                         std::numeric_limits<int>::max());
        } else if (flag == "--ply") {
            if (options.ply) {
                usage_error("duplicate flag --ply");
            }
            options.ply = parse_integer(value_after(args, index, flag), flag, 0,
                                        std::numeric_limits<int>::max());
        } else if (flag == "--moves") {
            reject_duplicate(seen_moves, flag);
            options.moves = std::string(value_after(args, index, flag));
        } else if (flag == "--search-budget") {
            reject_duplicate(seen_search_budget, flag);
            options.search_budget =
                parse_integer(value_after(args, index, flag), flag, 1, analysis::kMaxBudget);
        } else if (flag == "--explanation-budget") {
            reject_duplicate(seen_explanation_budget, flag);
            options.explanation_budget =
                parse_integer(value_after(args, index, flag), flag, 1, analysis::kMaxBudget);
        } else if (flag == "--json") {
            reject_duplicate(seen_json, flag);
            options.json = true;
        } else if (flag == "--full-report") {
            reject_duplicate(seen_full_report, flag);
            options.full_report = true;
        } else if (flag == "--no-llm") {
            reject_duplicate(seen_no_llm, flag);
            options.no_llm = true;
        } else if (flag == "--threads") {
            reject_duplicate(seen_threads, flag);
            options.threads = parse_integer(value_after(args, index, flag), flag, 1,
                                            std::numeric_limits<int>::max());
        } else if (flag == "--engine") {
            reject_duplicate(seen_engine, flag);
            options.engine = std::filesystem::path(value_after(args, index, flag));
        } else if (flag == "--openings") {
            reject_duplicate(seen_openings, flag);
            options.openings = std::filesystem::path(value_after(args, index, flag));
        } else if (flag == "--games") {
            reject_duplicate(seen_games, flag);
            options.games = std::filesystem::path(value_after(args, index, flag));
        } else if (flag == "--help") {
            if (options.help) {
                usage_error("duplicate flag --help");
            }
            options.help = true;
        } else {
            usage_error("unknown flag " + std::string(flag));
        }
    }

    if (!options.pgn && (seen_game || options.ply)) {
        throw input::InputError("--game and --ply may only be used with --pgn");
    }
    if (!options.help) {
        validate_options(options);
    }
    return options;
}

Report build_report(const Options& options) {
    validate_options(options);

    chess::Board board;
    std::vector<chess::Board> mainline;
    Report report;
    if (options.fen.has_value()) {
        board = input::parse_fen(options.fen.value());
        report.input.source = InputSource::Fen;
    } else if (options.pgn.has_value()) {
        const input::PgnPosition position =
            input::load_pgn(options.pgn.value(), options.game, options.ply);
        board = position.board;
        mainline = position.mainline;
        report.input.source = InputSource::Pgn;
        report.input.pgn_game = position.game;
        report.input.pgn_ply = position.ply;
    } else {
        throw input::InputError("exactly one of --fen or --pgn must be supplied");
    }

    const std::optional<std::string_view> typed_moves =
        options.moves ? std::optional<std::string_view>(*options.moves) : std::nullopt;
    auto resolved_moves = detail::resolve_moves(board, typed_moves);
    report.input.selection = std::move(resolved_moves.selection);
    report.input.moves = options.moves;

    report.input.fen = board.getFen();
    report.input.search_budget = options.search_budget;
    report.input.explanation_budget = options.explanation_budget;

    const openings::Table opening_table = openings::Table::load(options.openings);
    const games::Table game_table = games::Table::load(options.games);
    report.games.by_source.push_back(game_table.find(board));
    engine::Engine engine({.binary = options.engine, .threads = options.threads, .hash_mb = 64});
    report.engine.name = engine.name();
    report.engine.threads = options.threads;
    report.engine.hash_mb = 64;
    report.engine.reproducible = options.threads == 1;
    analysis::analyse(engine,
                      analysis::Request{.board = board,
                                        .selection = report.input.selection,
                                        .asked = std::move(resolved_moves.asked),
                                        .search_budget = options.search_budget,
                                        .explanation_budget = options.explanation_budget},
                      report);
    report.opening = openings::describe(opening_table, board, mainline);
    return report;
}

int run(const Options& options, std::ostream& out, std::ostream& err) {
    if (options.help) {
        out << usage();
        return kExitOk;
    }

    try {
        const Report report = build_report(options);
        write_output(options, report, out);
        return kExitOk;
    } catch (const input::InputError& error) {
        write_error(err, error.what());
        return kExitInput;
    } catch (const engine::EngineError& error) {
        write_error(err, error.what());
        return kExitEngine;
    } catch (const openings::OpeningsError& error) {
        write_error(err, error.what());
        return kExitEngine;
    } catch (const games::GamesError& error) {
        write_error(err, error.what());
        return kExitEngine;
    } catch (const std::exception& error) {
        write_error(err, "internal error: " + std::string(error.what()));
        return kExitEngine;
    }
}

} // namespace chesswhy::cli
