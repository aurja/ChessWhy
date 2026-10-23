// Opening names by position (lichess-org/chess-openings). Frozen by
// Planning/milestones/M1.md §Openings.
#pragma once

#include "chesswhy/report.hpp"

#include <array>
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
// SHA-256 of a.tsv ... e.tsv at that commit (PostM3_EM2 §4.5); tools/fetch_assets.sh keeps
// the same values in OPENINGS_SHA256. If any loaded file differs, the dataset reports
// "custom@<first 12 hex of the SHA-256 over the five files concatenated in a..e order>".
inline constexpr std::array<const char*, 5> kDatasetSha256 = {
    "3282e4c9155289a29224f9a85fba0decb46efa35c2fa5e39662d2ac48fb0f793",
    "1d5ed134ebbd87915ead5683416e37583bb3e5ae3582b4d8cb5cb4aa7ef4f623",
    "45e843c7b7e79a0d04d5bda879c9bd23f5a10974488bd8615e4f4c5d0749986f",
    "1cf4057a8aece94f27bb3f278ad410ee2844831a642364eeceaafa091dbcceda",
    "48c6f6645b031a5937c8386839debab0e2e48837251687ba1afb6369677f8546",
};

// Missing/unreadable file, malformed row, illegal move in a row, duplicate position key.
class OpeningsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Piece placement, side to move, castling rights, and an en-passant square only when at
// least one legal en-passant capture exists (no move counters).
[[nodiscard]] std::string position_key(const chess::Board& board);

class Table {
public:
    // Reads a.tsv..e.tsv (header "eco\tname\tpgn"), replays each pgn from the start position.
    [[nodiscard]] static Table load(const std::filesystem::path& dir);

    [[nodiscard]] std::optional<OpeningName> find(const chess::Board& board) const;
    [[nodiscard]] std::size_t size() const;
    // kDataset when every loaded file matches kDatasetSha256, else "custom@<12 hex>"
    // (PostM3_EM2 §4.5). `describe` copies it into OpeningInfo::dataset.
    [[nodiscard]] const std::string& dataset() const;

private:
    std::unordered_map<std::string, OpeningName> by_key_;
    std::string dataset_ = kDataset;
};

// current: name of `board`, if named. last_named: for PGN input (non-empty mainline),
// the last named position among mainline[0..ply-1] with its ply; never guessed.
[[nodiscard]] OpeningInfo describe(const Table& table, const chess::Board& board,
                                   std::span<const chess::Board> mainline);

} // namespace chesswhy::openings
