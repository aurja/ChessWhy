# M3b — Opening statistics

Status: **draft** (2026-10-21). Frozen on branch `m3` once the Captain approves the
changed diagrams and the open choices in §1. Planned together with M3a
(`Planning/milestones/M3a.md`) and built in parallel with it; both share report v4.

Intent: for the analysed position, the report states how often strong players reached it
over the board and which moves they chose, with results, from a pinned game-count table
built offline from the Lichess broadcast database. Counts carry `games` provenance, name
their source and platform, and are never phrased as move quality.

## 1. Captain's decisions

| # | Decision | Choice |
|---|---|---|
| B1 | Source | Lichess broadcasts only (2026-10-21): both players 2200+, January 2020 to September 2026 (81 monthly files). The Polyglot book was rejected |
| B2 | Broken games | Deputy's call: a game with an illegal or unreadable move within the ply limit is skipped whole and counted. Moves after the ply limit are not read, so a relay error late in a game (most of them are king moves in the endgame) does not cost the game |
| B3 | Games relayed twice | Not de-duplicated (Deputy's call) |
| B4 | Online events relayed as broadcasts | Not filtered; the source definition says the platform is "over the board" for most games and may include online relays (Deputy's call) |
| B5 | Ply limit and minimum game count | **Open** (§4.2): 40 plies and 5 games, from the pilot |

## 2. Contract files

Created by the orchestrator in the contract commit, before batch 1:

| File | Content |
|---|---|
| `include/chesswhy/games.hpp` | §6 API |
| `include/chesswhy/input.hpp` | `PgnReader` (§6) |
| `include/chesswhy/report.hpp` | §6 structs (shared with M3a) |
| `include/chesswhy/cli.hpp` | `Options::games` |
| `schema/report.v4.schema.json` | §5 plus M3a §6 |
| `src/games/CMakeLists.txt`, `tests/games/CMakeLists.txt` | new module `games` in the top-level module list; executable `chesswhy-games` |

## 3. Scope

- A streaming PGN reader that reports a broken game and carries on (§6).
- `chesswhy-games`: builds the table from PGN text on standard input (§4).
- `tools/fetch_assets.sh games`: downloads the pinned months, checks their SHA-256,
  builds the table with `chesswhy-games` and checks the table's SHA-256.
- Lookup at analysis time, report v4 `games` (§5), template rows (§8), `--games PATH`.
- Not in scope: other sources (Lichess rated games, Polyglot books, other databases),
  rating bands, time controls, per-year splits, de-duplication, any statement that a
  move is good or bad because of its counts, transpositions beyond the position key.

## 4. Table

### 4.1 Which games count

A game enters the table iff all hold; otherwise it is excluded and counted under the first
failing reason, in this order:

| Reason | Rule |
|---|---|
| `variant` | a `Variant` tag other than `Standard` |
| `setup` | a `FEN` or `SetUp` tag |
| `rating` | `WhiteElo` or `BlackElo` missing, not a whole number, or below 2200 |
| `result` | `Result` not `1-0`, `0-1` or `1/2-1/2` |
| `illegal` | a move among the first L plies cannot be read or is illegal |

### 4.2 What is counted

L = **ply limit** (40, open B5), N = **minimum game count** (5, open B5).

- For each kept game and each k = 0 … L−1 for which the game has a move k+1: the
  position Pₖ (key `openings::position_key`) with the move played from it. A position
  reached twice in one game (a repetition) counts only at its first occurrence.
- Per position: `games` = number of counted games, `white`/`draws`/`black` = their
  results, and per move the number of those games that played it. So `games` equals the
  sum of the move counts.
- A position is kept iff `games` ≥ N. All moves of a kept position are listed, even those
  played once.

### 4.3 File

`assets/data/games/lichess_broadcast_2200.tsv`, UTF-8, LF line ends. Header lines, in
this order:

```text
# source lichess_broadcast
# platform over the board (may include online events relayed as broadcasts)
# players both rated 2200 or more
# months 2020-01..2026-09
# credit Lichess broadcast database, database.lichess.org, CC BY-SA 4.0
# max_ply 40
# min_games 5
# games_read <n>
# excluded variant=<n> setup=<n> rating=<n> result=<n> illegal=<n>
# games_kept <n>
# positions <n>
```

Then one row per kept position, sorted by key bytewise:
`key<TAB>games<TAB>white<TAB>draws<TAB>black<TAB>moves`, where `moves` is `uci:n`
pairs joined by `,`, ordered by count descending, then uci ascending. Building twice from
the same input gives the same bytes.

### 4.4 Pinning

- `tools/fetch_assets.sh` lists the 81 month files with the SHA-256 published in
  `https://database.lichess.org/broadcast/sha256sums.txt` on 2026-10-21 and fails on any
  mismatch. If Lichess re-exports a month, re-pinning is an explicit task.
- The table's SHA-256 is pinned in `fetch_assets.sh` and in `games::kTable`
  (`"lichess_broadcast_2200@<first 12 hex digits>"`) after the first full build, as a §13
  row.

## 5. Report v4 (games part)

```json
"games": {
  "source": "games",
  "by_source": [{
    "id": "lichess_broadcast",
    "table": "lichess_broadcast_2200@0123456789ab",
    "platform": "over the board (may include online events relayed as broadcasts)",
    "players": "both rated 2200 or more",
    "months": "2020-01..2026-09",
    "credit": "Lichess broadcast database, database.lichess.org, CC BY-SA 4.0",
    "max_ply": 40, "min_games": 5,
    "found": true,
    "games": 1840, "white": 640, "draws": 790, "black": 410,
    "moves": [{"san": "e4", "uci": "e2e4", "games": 1140}]
  }]
}
```

- `games` is required on the report (after `opening`) and always set, also for terminal
  positions. `by_source` has one entry today.
- Strings and limits come from the table's header lines; `table` is `games::kTable`.
- `found` false → `games`, `white`, `draws`, `black` null and `moves` empty.
- `moves`: every move of the row, in table order; `san` from the analysed position.

## 6. Headers

```cpp
// report.hpp
struct GameMoveCount { std::string san; std::string uci; int games = 0; };
struct GameSourceCounts {
    std::string id, table, platform, players, months, credit;
    int max_ply = 0;
    int min_games = 0;
    bool found = false;
    std::optional<int> games, white, draws, black;   // null iff !found
    std::vector<GameMoveCount> moves;                // empty iff !found
};
struct GamesInfo { std::vector<GameSourceCounts> by_source; };
// Report: GamesInfo games;   // after opening

// input.hpp
struct PgnReadOptions { std::optional<int> max_plies; };  // stop reading moves after this
struct PgnReadResult {
    PgnGame game;                      // headers always; moves up to the error or limit
    std::optional<std::string> error;  // set: this game is broken; reading goes on
};
class PgnReader {
public:
    explicit PgnReader(std::istream& in, PgnReadOptions options = {});
    [[nodiscard]] std::optional<PgnReadResult> next();   // nullopt at end of input
};
// read_games(path) keeps its contract and uses PgnReader.

// games.hpp
namespace chesswhy::games {
inline constexpr const char* kTable = "lichess_broadcast_2200@<pinned>";
inline constexpr int kRatingFloor = 2200;
inline constexpr int kMaxPly = 40;
inline constexpr int kMinGames = 5;
class GamesError : public std::runtime_error { using std::runtime_error::runtime_error; };
struct BuildOptions { int max_ply = kMaxPly; int min_games = kMinGames;
                      std::string months; };       // header text, e.g. "2020-01..2026-09"
// §4: reads PGN from `in`, writes the table to `out`; summary counts to `log`.
void build_table(std::istream& in, std::ostream& out, std::ostream& log,
                 const BuildOptions& options);
class Table {
public:
    [[nodiscard]] static Table load(const std::filesystem::path& file); // GamesError
    [[nodiscard]] GameSourceCounts find(const chess::Board& board) const;
};
} // namespace chesswhy::games

// cli.hpp
// Options: std::filesystem::path games;  // default <assets>/data/games/lichess_broadcast_2200.tsv
```

The contract commit's headers are authoritative; the block above is a summary.

## 7. Analysis and CLI

- `--games PATH` loads the table once per run. A missing, unreadable or malformed table
  is an asset error (exit 3), like the openings dataset.
- The analysed position is looked up; `san` for each move comes from the analysed board.
- Lookup adds at most 200 ms per run (Release); the receipt records it.

## 8. Template

- After the opening row:
  `games: 1840 over-the-board games, both rated 2200 or more (Lichess broadcasts
  2020-01..2026-09, CC BY-SA 4.0): White won 640, draws 790, Black won 410 [games]`, or
  `games: position not in the table (it keeps positions reached in at least 5 games
  within the first 40 plies) [games]`.
- Under each candidate and the asked move, when found: `games: played in 1140 of 1840
  games [games]` or `games: not played in these games [games]`.
- No wording that ranks moves by count ("popular", "main line", "best" are not used).
  Wording pinned by golden tests.

## 9. Tests

| Test | Label | Checks |
|---|---|---|
| `games_unit_tests` | `unit` | §4 on small hand-written PGN text: each exclusion reason, a repetition, a game shorter than L, a move at ply L+1 not counted, an illegal move after L not excluding the game, N boundary (N−1 and N), move order ties, header lines; `Table::load` errors; `find` hit and miss |
| `input_unit_tests` (extended) | `unit` | `PgnReader`: broken game reported and the next game read; `max_plies`; `read_games` unchanged |
| `games_oracle` | `games` | `chesswhy-games` on the pinned months 2020-07 and 2022-08 (with N = 2) equals byte for byte the output of `tools/count_games.py`, an independent python-chess counter written from §4 |
| F rows (`positions.json`) | `engine` | start position: `found`, exact `games` and the top three moves with exact counts; a position past ply 40: not found; one candidate row in the template |

New CTest label `games`: needs the monthly files fetched by `tools/fetch_assets.sh games`
(AGENTS.md updated in the contract commit). The `engine` label now also needs the table.

## 10. Work packages

Each worker owns only the files listed, builds and runs its own targets, and returns
under 300 words. Batch 1: WP-B1, WP-B2 (with M3a's batch 1 and WP-R). Batch 2: WP-I
(M3a §11). Milestone end: WP-D (M3a §11).

| WP | Owned files | Done when | Non-goals |
|---|---|---|---|
| WP-B1 reader + table | `src/input/pgn.cpp`, `tests/input/*` (reader tests only), `src/games/*`, `tests/games/*`, `tools/fetch_assets.sh` | `PgnReader` per §6; §4 in `build_table` and `chesswhy-games`; `Table::load`/`find`; `fetch_assets.sh games` downloads, verifies, builds, verifies; tests of §9 except F rows; full table built once and its SHA-256 reported | report, render, analysis |
| WP-B2 oracle | `tools/count_games.py` | independent §4 counter written from this plan without reading WP-B1's code; same output format; Ruff and basedpyright clean | C++ |

WP-R and WP-I (M3a §11) carry §5, §7 and §8.

## 11. Captain's checkpoints

0. Before batch 1: the changed diagrams and B5.
1. After batch 1: the table's numbers (games read, each exclusion, kept, positions, file
   size, SHA-256) and the F rows.
2. Milestone end: try the smoke command and approve merging `m3`.

## 12. Acceptance

1. Release and ASan/UBSan builds warning-free; all `unit`, `engine` and `games` CTests
   pass in both trees.
2. `tools/fetch_assets.sh games` from a clean `assets/data/games/` rebuilds the table
   with the pinned SHA-256 (rebuild identity).
3. `games_oracle` passes; at the gate, `tools/count_games.py` over all 81 months gives a
   table byte-identical to the pinned one (run once, recorded in the receipt).
4. F rows pass; every F report validates against `report.v4.schema.json`.
5. Facts first: counts carry `games`, name source and platform, and the template uses no
   quality wording (golden tests).
6. Lookup cost within §7; `standard` ≤ 12 s per F fixture still holds.
7. `third_party/DEPENDENCIES.md` row for the broadcast database (CC BY-SA 4.0) names the
   pinned months and the table id; `README.md` explains `fetch_assets.sh games`.
8. Gates: clang-format and clang-tidy on changed C++; Ruff and basedpyright on changed
   Python; ShellCheck on `tools/fetch_assets.sh`; `python3 tools/check_dbc.py`.

Smoke command: `build/release/chesswhy --fen
'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' --budget quick --no-llm`
prints the `games` row with the pinned start-position counts, and `games: played in …`
under each candidate.

## 13. Contract changes

| Date | Change | Reason |
|---|---|---|
