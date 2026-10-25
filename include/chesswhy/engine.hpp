// UCI engine wrapper. Stockfish runs as a separate process (GPL-3.0: never linked).
// Frozen by Planning/milestones/M1.md §Engine protocol.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chesswhy::engine {

// Process start failure, unexpected exit, protocol error or missing WDL output.
class EngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Options {
    std::filesystem::path binary;
    int threads = 1;
    int hash_mb = 64;
};

struct SearchRequest {
    std::string fen;
    int depth = 0; // go depth; >= 1
    int multipv = 1;
    std::vector<std::string> searchmoves_uci; // empty: all legal moves
};

// One principal variation. Scores are from the point of view of the side to move
// in SearchRequest::fen (UCI convention).
struct Line {
    int multipv = 1;
    int depth = 0;
    std::optional<int> cp;
    std::optional<int> mate;
    int win = 0; // permille
    int draw = 0;
    int loss = 0;
    std::vector<std::string> pv_uci;
};

struct SearchResult {
    // The engine's final MultiPV order; every first move at most once; lines[i].multipv
    // == i + 1 (k <= legal moves). Built by MultiPvCollector.
    std::vector<Line> lines;
    std::string bestmove_uci;
    std::uint64_t nodes = 0; // "nodes" of the last info line
};

// Parses one "info ... multipv ... score ... wdl ... pv ..." line. Returns nullopt for
// info lines without pv, without wdl, or with lowerbound/upperbound.
[[nodiscard]] std::optional<Line> parse_info_line(std::string_view text);

// Turns one search's "info" output into SearchResult::lines. A node-limited search
// can stop mid-iteration, so the last batch the engine prints (the run of lines that
// starts at multipv 1) may contain lowerbound/upperbound lines, and an older exact
// line in the same slot may name a move that has since moved to another slot.
// Rules, applied to the last batch's slots in multipv order:
//   1. The slot's move is the first pv move of its line in the last batch, exact or bound.
//   2. Its Line is the newest exact line (any slot, any depth) whose first move is that move.
//   3. No exact line for that move: the newest exact line of the same slot, if its move
//      is not already selected; otherwise the slot is omitted.
// Selected lines are renumbered 1..k. Info lines without pv are ignored.
class MultiPvCollector {
public:
    void add(std::string_view info_line);
    // Throws EngineError when no exact line was seen.
    [[nodiscard]] std::vector<Line> finish() &&;

private:
    std::vector<std::pair<int, std::string>> last_batch_; // (multipv, first move), sorted by slot
    std::vector<Line> exact_lines_;                       // exact lines in arrival order
    bool has_last_batch_ = false;
};

class Engine {
public:
    // Starts the process, runs uci/uciok, sets Threads, Hash, UCI_ShowWDL=true,
    // waits for readyok. Throws EngineError.
    explicit Engine(const Options& options);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    [[nodiscard]] const std::string& name() const; // "id name" value

    // ucinewgame, setoption MultiPV, isready/readyok, position fen, go depth [searchmoves],
    // read until bestmove. Lines are selected by MultiPvCollector.
    [[nodiscard]] SearchResult search(const SearchRequest& request);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chesswhy::engine
