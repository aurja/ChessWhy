# ChessWhy

ChessWhy answers why a move may be best or why a chosen move is good or bad for one chess position. Its chess facts come from Stockfish searches and deterministic rules, and each fact records its provenance. The current release renders a report without a language model; verbalisation is planned for M4.

## Requirements and setup

A C++ compiler (g++ 14 or clang 19), CMake 3.25 or newer, and Ninja are required to build. Python 3 and the development requirements are needed for the report-schema tests.

```sh
git submodule update --init --recursive
tools/fetch_assets.sh engine openings
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements-dev.txt
```

The asset command fetches Stockfish and the opening tables. On Debian, if creating the virtual environment fails because `venv` is unavailable, install `python3-venv` first.

## Build and test

```sh
cmake --preset release && cmake --build --preset release
ctest --preset release
```

CTest's `unit` label does not need an engine; `engine` tests need the fetched Stockfish asset. The `asan` preset builds with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset asan && cmake --build --preset asan
ctest --preset asan
```

## Use

Explain a position and a candidate move (the FEN example used by the M1 smoke check):

```sh
build/release/chesswhy --fen "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3" --move Nf6
```

Analyze a position from a PGN. `--game` is 1-based; `--ply` counts half-moves from the game's start, with 0 denoting the initial position. Before White's move N, use ply `2(N−1)`; before Black's move N, use ply `2N−1`. `--move` accepts SAN or UCI; `--json` selects JSON output, and budgets are `quick`, `standard` (default), or `deep`.

```sh
build/release/chesswhy --pgn tests/fixtures/two_games.pgn --game 1 --ply 2 --json --budget quick
```

List a PGN's mainline plies as TSV, including the FEN before and after each move:

```sh
build/release/chesswhy-pgn tests/fixtures/two_games.pgn --game 1
```

`chesswhy-pgn` prints a header and one row per half-move. Its columns are `game`, `ply`, `move_number`, `side`, `san`, `uci`, `nags`, `fen_before`, and `fen_after`.

## Exit codes

- `chesswhy`: 0 on success; 2 for usage, FEN, PGN, or move errors; 3 for engine or asset errors.
- `chesswhy-pgn`: 0 on success; 2 for argument or PGN errors.

## License

ChessWhy is licensed under the MIT License; see `LICENSE.txt`. Dependencies retain their own licenses, listed in [`third_party/DEPENDENCIES.md`](third_party/DEPENDENCIES.md). Stockfish is GPL-3.0, fetched separately and run as a separate process; it is never linked or redistributed.
