# M3b receipt — opening statistics

Date: 2026-10-22. Machine: `debian-home` (i7-8565U, 15 GiB, Debian 13). Branch `m3`
(shared with M3a). Contract: `Planning/milestones/M3b.md` (acceptance §12, contract
changes §13). Terms: `Planning/DBC/DEFINITIONS.md`.

## Delivered

- **Table builder** `chesswhy-games` (C++): reads the 81 pinned monthly Lichess broadcast
  PGNs (2020-01..2026-09). It keeps games with both players rated 2200 or more, standard
  chess and a result, and counts positions within the first 40 plies. It drops positions
  seen in fewer than 5 games and writes a sorted TSV. Run by `tools/fetch_assets.sh games`,
  which checks the pinned SHA-256.
- **Lookup** (`include/chesswhy/games.hpp`): `Table::load` checks structure; `find`
  validates the returned row (§13). The CLI option `--games FILE` has a default table
  path; a missing or invalid table exits 3.
- **Report v4** `games` block (provenance `games`): the table id, platform, players,
  months, credit, `max_ply`, `min_games`, and per position the game count, results and
  move counts. Each candidate and the asked move print `games: played in N of M games` or
  `games: not played in these games`.
- **Independent oracle** `tools/count_games.py` (python-chess, development only) and
  `games_oracle` (`games` label), which compares both tools byte for byte on two months.
- **Fixtures**: F25 and F28–F30 carry `games` rows (found with counts, or not found).

## The table

| Figure | Value |
|---|---|
| Games read | 1,263,913 |
| Excluded: variant / setup / rating / result / illegal | 11,654 / 2,206 / 980,538 / 5,929 / 18 |
| Games kept | 263,568 |
| Positions (≥ 5 games, ≤ 40 plies) | 117,058 |
| File | `lichess_broadcast_2200.tsv`, 10,422,667 bytes |
| SHA-256 | `e0cb7fab32128854d72b158178225ae586c2567a6b79bdfb4bc504390038d7a7` |
| Table id | `lichess_broadcast_2200@e0cb7fab3212` |
| Build time (C++) | 166.8 s |

The start position counts 262,026 games, not 263,568. A position counts only when a move
is played from it (§4). [INFERENCE] The 1,542 missing games have no moves (forfeits).

## Verification

| # | Check | Result |
|---|---|---|
| 1 | Release and ASan/UBSan builds; CTests | Builds use `-Werror`. Final (after the review fixes): Release 141/141 (107 `unit`, 33 `engine` serial, 1 `games`). ASan/UBSan 141/141: `unit` and `games` 107/107 plus `engine` 33/33 after the review fixes; `tactics_oracle` (unchanged) passed in the branch's first ASan run |
| 2 | Rebuild identity | `tools/fetch_assets.sh games` from an empty `assets/data/games/`: 81 months downloaded and checked against their pinned SHA-256s; the table builds with the same counts and SHA-256 `e0cb7fab…d7a7` (28 min 50 s wall time, mostly download) |
| 3 | Oracle | `games_oracle` passes (2020-07 and 2022-08, byte-identical). Full run of `tools/count_games.py` over all 81 months: same exclusion counts, 117,058 positions, 10,422,667 bytes, SHA-256 `e0cb7fab…d7a7`; byte-identical to the pinned table (121 min wall time) |
| 4 | F rows | F25 (12,900 games; Ba4 12,424, Bxc6 474), F28 (470; O-O 369, Nc3 69, d4 15), F29 start position (262,026; e4 123,657, d4 87,792, Nf3 28,707), F30 not found (beyond 40 plies); every F report validates against v4 |
| 5 | Facts first | counts carry `games` and name source, platform and credit; no quality wording; golden render tests pass |
| 6 | Lookup cost; wall time | load + `find(start)` 108.7–144.0 ms over five runs, mean 121.5 ms (§7 limit 200 ms); slowest `standard` F fixture 7.80 s (F06; limit 12 s) |
| 7 | Credits | `third_party/DEPENDENCIES.md` row: CC BY-SA 4.0 (broadcasts section of database.lichess.org), the 81 pinned months, the table id and SHA-256; README explains `tools/fetch_assets.sh games` |
| 8 | Gates | clang-format and `clang-tidy --quiet -p build/release` clean on all changed C++; Ruff (lint and format) and basedpyright clean on `tools/count_games.py`; ShellCheck clean on `tools/fetch_assets.sh`; `check_dbc` ok |

Smoke (`--fen 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' --budget quick
--no-llm`, 5.9 s) prints `games: 262026 games, both rated 2200 or more (platform: over the
board (may include online events relayed as broadcasts); source: Lichess broadcast
database, database.lichess.org, CC BY-SA 4.0; months: 2020-01..2026-09): White won 91439,
draws 105183, Black won 65404 [games]`, and `games: played in …` under each candidate
(e4 123,657; d4 87,792; Nf3 28,707).

## Contract changes

All in §13: WP-I adds the §9 F rows. The games row prints the report fields verbatim,
with field names (Captain, checkpoint 1). `load` checks structure only and `find`
validates the returned row (load + find was 671–724 ms before, against the 200 ms limit).

## Review

One completion review (`reviewer`, 2026-10-22) covering M3a and M3b together, scope M3a
§14, M3b §12 and the project principles. Two blockers, both against acceptance 3 (the
independent counter), both fixed at the root:

- **Position key kept a pseudo-legal en passant square.** `openings::position_key`
  returned `getFen(false)` and relied on chess-library to drop an en passant square
  without a legal capture; after `makeMove` it does not. In an eligible game reaching
  9...f5 with White's e5 pawn pinned (`tests/fixtures/games_edge.pgn`), the builder
  stored `f6`, Python stored `-`, and a `--fen` lookup missed. `position_key` now keeps
  the square only when a legal en passant capture exists. The fix is shared by the
  builder, the lookup and the opening names.
- **Python oracle parsed variations.** After an illegal move inside a variation
  (`1.e4 (1.d4 Ke6) e5`), python-chess's error recovery left the variation board in
  place, so later mainline moves were counted from it. The visitor now skips variations.

Regression: `games_oracle_edge` (`unit`) runs both tools on `games_edge.pgn` (19 vs 22
positions before the fix, byte-identical after), plus a `position_key` unit case. The
pinned table rebuilt with the fixed builder has the same SHA-256 (`e0cb7fab…d7a7`), so
the shipped table was not affected by either bug. One recheck found both blockers
resolved.
