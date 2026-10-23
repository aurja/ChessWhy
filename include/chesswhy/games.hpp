// Game counts from the Lichess broadcast database. Frozen by Planning/milestones/M3b.md
// §4 (table), §5 (report) and §6 (API). Source: games. Counts are never move quality.
#pragma once

#include "chesswhy/report.hpp"

#include <filesystem>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <chess.hpp>

namespace chesswhy::games {

// Table name and the first 12 hex digits of the built table's SHA-256 (M3b §4.4). Set
// once by the first full build, recorded as an M3b §13 row; until then the marker below.
inline constexpr const char* kTable = "lichess_broadcast_2200@e0cb7fab3212";
// Full SHA-256 of the pinned table file (PostM3_EM2 §4.5); tools/fetch_assets.sh keeps the
// same value in GAMES_TABLE_SHA256. A loaded table with another digest reports
// "custom@<first 12 hex>" instead of kTable.
inline constexpr const char* kTableSha256 =
    "e0cb7fab32128854d72b158178225ae586c2567a6b79bdfb4bc504390038d7a7";

inline constexpr int kRatingFloor = 2200; // both WhiteElo and BlackElo at or above
inline constexpr int kMaxPly = 40;        // L, ply limit (M3b §4.2, B5)
inline constexpr int kMinGames = 5;       // N, minimum game count (M3b §4.2, B5)

// Header line values written by build_table (M3b §4.3) and read back by Table::load.
inline constexpr const char* kSourceId = "lichess_broadcast";
inline constexpr const char* kPlatform =
    "over the board (may include online events relayed as broadcasts)";
inline constexpr const char* kPlayers = "both rated 2200 or more";
inline constexpr const char* kCredit =
    "Lichess broadcast database, database.lichess.org, CC BY-SA 4.0";

// Missing or unreadable table, malformed header line or row, unsorted or duplicate key.
class GamesError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct BuildOptions {
    int max_ply = kMaxPly;
    int min_games = kMinGames;
    std::string months; // "# months" header text, e.g. "2020-01..2026-09"
};

// M3b §4: reads PGN text from `in` (input::PgnReader with max_plies = max_ply), writes the
// table in the §4.3 format to `out` and the summary counts (games read, each exclusion
// reason, kept, positions) to `log`. Byte-identical output for identical input.
void build_table(std::istream& in, std::ostream& out, std::ostream& log,
                 const BuildOptions& options);

class Table {
public:
    // Reads a §4.3 file; header strings and limits come from its header lines. Checks
    // the header and every row's shape, counts, order and move-count sums; a row's
    // position key and move legality are checked when `find` returns that row.
    [[nodiscard]] static Table load(const std::filesystem::path& file); // throws GamesError

    // Counts for the analysed position (key openings::position_key). Not found: header
    // fields set, found false, counts null, moves empty. `table` is kTable; each move's
    // `san` is computed on `board`. Throws GamesError when the found row lists a move
    // that is illegal on `board`.
    [[nodiscard]] GameSourceCounts find(const chess::Board& board) const;

private:
    struct Row {
        int games = 0;
        int white = 0;
        int draws = 0;
        int black = 0;
        std::vector<std::pair<std::string, int>> moves; // uci, games; table order
    };
    GameSourceCounts header_; // strings and limits; found false, no counts
    std::unordered_map<std::string, Row> rows_;
};

} // namespace chesswhy::games
