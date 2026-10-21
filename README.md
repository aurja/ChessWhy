# ChessWhy

ChessWhy answers why a move may be best or why a chosen move is good or bad for one chess position.
Its chess facts come from Stockfish searches and deterministic rules, and each fact records its
provenance. It also reports tactical motifs: deterministic patterns of one position (and, where
needed, the move that produced it), with `rule` provenance; a motif states that the pattern is
present and claims no gain. Pins, overloaded defenders and tactical pins (a piece held in place
because removing it would enable a fork, mate, capture or other threat) are printed as
"restricted movement". The best move and each refutation are judged as tactics: a tactic is a
line whose expected score beats the next-best move by at least 0.13 (`engine`); a position with
one legal move is marked forced (`rule`). On a tactic line every motif gets a contribution
(`direct`, `indirect` or `none`), and the rule label `removal_of_defender` marks a capture that
takes away a piece's only defender. The current release renders a report without a language
model; verbalisation is planned for M4.
M2 motif definitions were written after consulting lichess-puzzler, and `removal_of_defender`
follows its `capturing_defender` theme; the Lichess puzzle database (CC0) provides the theme tags
used as the validation reference.

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

CTest's `unit` label does not need an engine; `engine` tests need the fetched Stockfish asset.
`tactics_unit_tests` checks the T fixtures (motifs) and C fixtures (hand-labelled contributions and
tactic labels) in CTest's `unit` suite; `tactics_puzzles` measures recall, and `tactics_oracle`
checks exact agreement with the independent reference implementation.
The `asan` preset builds with AddressSanitizer and UndefinedBehaviorSanitizer:

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
