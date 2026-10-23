// Command line. Frozen by Planning/milestones/M1.md §CLI; --games by M3b §7.
#pragma once

#include "chesswhy/report.hpp"

#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace chesswhy::cli {

inline constexpr int kExitOk = 0;
inline constexpr int kExitInput = 2;  // usage, FEN, PGN or move error
inline constexpr int kExitEngine = 3; // engine or asset error

struct Options {
    std::optional<std::string> fen;
    std::optional<std::filesystem::path> pgn;
    int game = 1;
    std::optional<int> ply;
    std::optional<std::string> moves;
    Level level = Level::Club;
    Budget budget = Budget::Standard;
    bool json = false;
    bool full_report = false;
    bool no_llm = false; // accepted; M1 always renders the template
    int threads = 1;
    std::filesystem::path engine;   // default: <assets>/engines/stockfish
    std::filesystem::path openings; // default: <assets>/data/openings
    std::filesystem::path games;    // default: <assets>/data/games/lichess_broadcast_2200.tsv
    bool help = false;
};

// Throws input::InputError on bad usage.
[[nodiscard]] Options parse_args(std::span<const std::string_view> args);

// Builds the report and prints JSON or text to `out`; errors go to `err` as
// "chesswhy: error: <message>\n". Returns an exit code above.
[[nodiscard]] int run(const Options& options, std::ostream& out, std::ostream& err);

} // namespace chesswhy::cli
