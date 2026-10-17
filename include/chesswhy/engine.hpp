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
    std::uint64_t nodes = 0;
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
    std::vector<Line> lines; // ordered by multipv index, 1..k (k <= legal moves)
    std::string bestmove_uci;
    std::uint64_t nodes = 0; // "nodes" of the last info line
};

// Parses one "info ... multipv ... score ... wdl ... pv ..." line. Returns nullopt for
// info lines without pv, without wdl, or with lowerbound/upperbound.
[[nodiscard]] std::optional<Line> parse_info_line(std::string_view text);

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

    // ucinewgame, setoption MultiPV, isready/readyok, position fen, go nodes [searchmoves],
    // read until bestmove. Keeps the last complete line per multipv index.
    [[nodiscard]] SearchResult search(const SearchRequest& request);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace chesswhy::engine
