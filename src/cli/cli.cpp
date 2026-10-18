#include "chesswhy/cli.hpp"

#include "chesswhy/analysis.hpp"
#include "chesswhy/engine.hpp"
#include "chesswhy/input.hpp"
#include "chesswhy/openings.hpp"
#include "chesswhy/render.hpp"

#include <charconv>
#include <exception>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#ifndef CHESSWHY_ASSETS_DIR
#error "CHESSWHY_ASSETS_DIR must be provided by chesswhy_paths"
#endif

namespace chesswhy::cli {
namespace {

[[nodiscard]] std::filesystem::path assets_path(std::string_view suffix) {
    return std::filesystem::path(CHESSWHY_ASSETS_DIR) / suffix;
}

[[nodiscard]] std::string usage() {
    return "Usage: chesswhy (--fen FEN | --pgn FILE [--game N] [--ply N]) [--move MOVE]\n"
           "                [--level beginner|club|advanced] [--budget quick|standard|deep]\n"
           "                [--json] [--no-llm] [--threads N] [--engine PATH] [--openings DIR] "
           "[--help]\n";
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

[[nodiscard]] int parse_integer(std::string_view token, std::string_view flag, int minimum) {
    int value = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size() ||
        value < minimum) {
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

    bool seen_game = false;
    bool seen_json = false;
    bool seen_no_llm = false;
    bool seen_threads = false;
    bool seen_engine = false;
    bool seen_openings = false;

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
            options.game = parse_integer(value_after(args, index, flag), flag, 1);
        } else if (flag == "--ply") {
            if (options.ply) {
                usage_error("duplicate flag --ply");
            }
            options.ply = parse_integer(value_after(args, index, flag), flag, 0);
        } else if (flag == "--move") {
            if (options.move) {
                usage_error("duplicate flag --move");
            }
            options.move = std::string(value_after(args, index, flag));
        } else if (flag == "--level") {
            const std::string_view value = value_after(args, index, flag);
            if (value == "beginner") {
                options.level = Level::Beginner;
            } else if (value == "club") {
                options.level = Level::Club;
            } else if (value == "advanced") {
                options.level = Level::Advanced;
            } else {
                usage_error("invalid value for --level: " + std::string(value));
            }
        } else if (flag == "--budget") {
            const std::string_view value = value_after(args, index, flag);
            if (value == "quick") {
                options.budget = Budget::Quick;
            } else if (value == "standard") {
                options.budget = Budget::Standard;
            } else if (value == "deep") {
                options.budget = Budget::Deep;
            } else {
                usage_error("invalid value for --budget: " + std::string(value));
            }
        } else if (flag == "--json") {
            reject_duplicate(seen_json, flag);
            options.json = true;
        } else if (flag == "--no-llm") {
            reject_duplicate(seen_no_llm, flag);
            options.no_llm = true;
        } else if (flag == "--threads") {
            reject_duplicate(seen_threads, flag);
            options.threads = parse_integer(value_after(args, index, flag), flag, 1);
        } else if (flag == "--engine") {
            reject_duplicate(seen_engine, flag);
            options.engine = std::filesystem::path(value_after(args, index, flag));
        } else if (flag == "--openings") {
            reject_duplicate(seen_openings, flag);
            options.openings = std::filesystem::path(value_after(args, index, flag));
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

int run(const Options& options, std::ostream& out, std::ostream& err) {
    if (options.help) {
        out << usage();
        return kExitOk;
    }

    try {
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

        std::optional<chess::Move> asked_move;
        if (options.move) {
            chess::Movelist legal_moves;
            chess::movegen::legalmoves(legal_moves, board);
            if (legal_moves.empty()) {
                throw input::InputError("illegal move " + *options.move);
            }
            asked_move = input::parse_move(board, *options.move);
        }

        report.input.fen = board.getFen();
        report.input.move = options.move;
        report.input.level = options.level;
        report.input.budget = options.budget;

        const openings::Table opening_table = openings::Table::load(options.openings);
        engine::Engine engine(
            {.binary = options.engine, .threads = options.threads, .hash_mb = 64});
        report.engine.name = engine.name();
        report.engine.threads = options.threads;
        report.engine.hash_mb = 64;
        report.engine.reproducible = options.threads == 1;
        analysis::analyse(
            engine,
            analysis::Request{.board = board, .asked = asked_move, .budget = options.budget},
            report);
        report.opening = openings::describe(opening_table, board, mainline);

        if (options.json) {
            out << to_json(report).dump(2) << '\n';
        } else {
            out << render::text(report);
        }
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
    } catch (const std::exception& error) {
        write_error(err, "internal error: " + std::string(error.what()));
        return kExitEngine;
    }
}

} // namespace chesswhy::cli
