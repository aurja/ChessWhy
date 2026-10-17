// Opening names by position (lichess-org/chess-openings). Frozen by
// Planning/milestones/M1.md §Openings.
#pragma once

#include "chesswhy/report.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <chess.hpp>

namespace chesswhy::openings {

inline constexpr const char* kDataset = "lichess-org/chess-openings@a6189a3";

// Missing/unreadable file, malformed row, illegal move in a row, duplicate position key.
class OpeningsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Piece placement, side to move, castling rights, en-passant square only when a legal
// capture exists (chess-library already normalises it): board.getFen(false).
[[nodiscard]] std::string position_key(const chess::Board& board);

class Table {
public:
    // Reads a.tsv..e.tsv (header "eco\tname\tpgn"), replays each pgn from the start position.
    [[nodiscard]] static Table load(const std::filesystem::path& dir);

    [[nodiscard]] std::optional<OpeningName> find(const chess::Board& board) const;
    [[nodiscard]] std::size_t size() const;

private:
    std::unordered_map<std::string, OpeningName> by_key_;
};

// current: name of `board`, if named. last_named: for PGN input (non-empty mainline),
// the last named position among mainline[0..ply-1] with its ply; never guessed.
[[nodiscard]] OpeningInfo describe(const Table& table, const chess::Board& board,
                                   std::span<const chess::Board> mainline);

} // namespace chesswhy::openings
