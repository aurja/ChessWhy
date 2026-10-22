# Pinned dependencies

## Submodules (`third_party/`)

| Dependency | Pin | License | Role | Milestone |
|---|---|---|---|---|
| Disservin/chess-library | `53e6a841dcda7059a2af363d85f785ef1817304a` (master, 2026-07-26) | MIT | board, legal moves, FEN, PGN stream parser, SAN/UCI | M1 |
| nlohmann/json | `v3.12.0` | MIT | report serialisation | M1 |
| catchorg/Catch2 | `v3.16.1` | BSL-1.0 | tests | M1 |
| yhirose/cpp-httplib | `v0.61.0` | MIT | local HTTP API | M6 |
| ggml-org/llama.cpp | `b11712` | MIT | `libllama` inference | M4 |

Clone with `git clone --recurse-submodules`, or run
`git submodule update --init --recursive`. Change a pin only as an explicit task and
update this table.

## Development-only Python packages (`.venv/`, git-ignored, `tools/requirements-dev.txt`)

| Package | Pin | License | Role | Milestone |
|---|---|---|---|---|
| python-jsonschema/jsonschema with attrs, jsonschema-specifications, referencing, rpds-py | `4.26.0`; others pinned in `tools/requirements-dev.txt` | MIT (all five) | validates reports against `schema/report.v2.schema.json` in CTest | M1 |
| niklasf/python-chess (`chess`) | `1.11.2` | GPL-3.0-or-later; piece drawings by Colin M.L. Burnett used under their BSD option | fixture-board SVGs and independent `tools/count_games.py` oracle for `games_oracle`; development-only, never linked into or shipped with ChessWhy | PostM2_EM1, M3 |

## Fetched assets (`assets/`, git-ignored, `tools/fetch_assets.sh`)

| Asset | Version | SHA-256 | License | Use |
|---|---|---|---|---|
| Stockfish Linux x86-64 universal | `sf_19` | `9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f` (tarball) | GPL-3.0 | separate UCI process; never linked or redistributed |
| Qwen3.5-0.8B Q4_K_M GGUF (bartowski) | — | `fb044e93939a70469c905781334f5de1e6c8b608ced6cbc8c9249bd4127d9526` | Apache-2.0 | libllama build smoke test only |
| Lichess puzzle database | 2026-10-05 export `assets/data/lichess_db_puzzle.csv.zst`; committed sample `tests/fixtures/puzzles/test.csv` | `76335bfa7d7c4a7f93c1366d81549e53951ebb79dd43d34904cab8f22d962f8d` (source file) | CC0 | 6,157,341 source puzzles; eligible: 1,326,623 (`RatingDeviation ≤ 80`, `NbPlays ≥ 1000`, `Popularity ≥ 80`, ≤7 moves); sample: 9,300 puzzles (9,301 CSV lines including header, ≈1.4 MB) |
| lichess-org/chess-openings `a.tsv`–`e.tsv` | commit `a6189a30dc273ccb21fc2536a9a2fefd5592a67a` | per file in `tools/fetch_assets.sh` | CC0 | M1 opening names |
| Lichess broadcast PGNs; derived `lichess_broadcast_2200.tsv` | `81` monthly files, `2020-01..2026-09`; source SHA-256s pinned in `tools/fetch_assets.sh` | Table `lichess_broadcast_2200@e0cb7fab3212`; SHA-256 `e0cb7fab32128854d72b158178225ae586c2567a6b79bdfb4bc504390038d7a7`; `10,422,667` bytes, `117,058` positions | CC BY-SA 4.0 ([broadcasts section](https://database.lichess.org/#broadcasts)); credit `Lichess broadcast database, database.lichess.org` (recorded as CC0 before `2026-10-21`) | Data only; monthly PGNs fetched, not vendored. `tools/fetch_assets.sh games` reads `1,263,913` games and keeps `263,568` (both players rated ≥`2200`, standard chess, decisive or drawn result, legal moves); max `40` plies, min `5` games per position; build ≈`167 s`. |

## References (no code used)

| Reference | Pin | License | Use |
|---|---|---|---|
| lichess-puzzler | commit `608cf7f2b2a2a02682736ff176b454a30b398e01` | AGPL-3.0 | motif definitions consulted; puzzle themes used as the validation reference; `removal_of_defender` after `capturing_defender`. No code used |
