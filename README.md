# ChessWhy

ChessWhy answers why a move may be best or why a chosen move is good or bad for one chess position.
Its chess facts come from Stockfish searches and deterministic rules, and each fact records its
provenance. It also reports tactical motifs: deterministic patterns of one position (and, where
needed, the move that produced it), with `rule` provenance; a motif states that the pattern is
present and claims no gain. Pins, overloaded defenders and tactical pins are reported as
"restricted movement"; a tactical pin holds a piece because moving it would leave a specified
threat available.

Every judged move gets a verdict from its loss in expected score (ES) against the best move:

| Verdict | Rule |
|---|---|
| `best` | the engine's top line, or another line with exactly the same score (e.g. two mates in 1) |
| `okay` | any other move losing at most 0.05 ES |
| `inaccuracy` | loses more than 0.05 and at most 0.10 ES |
| `mistake` | loses more than 0.10 ES and allows no tactic |
| `blunder` | loses more than 0.10 ES and allows a tactic |

`best` and `okay` moves are **playable**. A line is a **tactic** when the side playing it mates
or wins at least 1 pawn net against best replies over the walked plies (a move *allows a tactic*
when its own line does that for the opponent). A line is an **opportunity** when every
alternative is clearly worse: its margin over the next-best move is at least 0.13 ES (`engine`).
A line can be either, both or neither; a position with one legal move is marked forced
(`rule`). On a tactic or opportunity line every motif gets a contribution (`direct`,
`indirect` or `none`), and the rule label `removal_of_defender` marks a capture that takes away a
piece's only defender. Engine scores depend on the search: asking for more moves widens the
search, so the same move can get a different verdict in a different request; each request is
reproducible. The current release renders reports without a language model; verbalisation is
planned for M5.

In report v8, `pin_absolute`, `pin_relative`, `overloaded_defender` and `pin_tactical` have a
`restriction` object. Its `piece` is the held piece (the first target for either pin or a tactical
pin, the actor for an overloaded defender); `restricted` and `free` are sorted UCI move lists.
`restricted` contains moves the motif denies or makes costly: an absolute pin includes pseudo-legal
moves off the pinner-to-king segment; a relative pin or overloaded defender includes moves that
expose the rear or guarded target to a win; a tactical pin includes moves that still allow its
named threat. `free` contains legal moves in the held piece's move set that are not restricted.
`held_to.kind` is `square` when there is no free move; otherwise it is `file`, `rank` or `diagonal`
when all free destinations lie on that line through the held piece, and `squares` otherwise. The
existence condition is that `restricted` is non-empty. Moves are judged with the held side to move.
A held pawn's move set also includes a hypothetical capture onto every on-board diagonal-front
square, whatever stands there (including an empty square), judged after the pawn moves there and
the previous occupant is removed.
Move report text prints restricted-movement rows without the restricted/free move lists and
names every label of merged motif repeats.

Positional facts in `position.positional` describe pawn structure, king safety, pieces, space/development
and material; per-line `positional` entries record features appearing or disappearing, with `rule`
provenance. Lichess broadcast counts use `games` provenance and never judge a move: the full report
text shows `games: 262026 games ... [games]` under the position, and each move shows
`Played in N of M games (platform: ...; source: ...) [games]`. Reports credit the source as `Lichess broadcast database,
database.lichess.org` under CC BY-SA 4.0.

The **tempo value** (`engine`) is how much a side's expected score drops if it passes instead of
moving: at the root for the side to move (`position.tempo_value`), and after each detailed move
for the opponent. Above 0.10 the position is **tempo-dependent**, below 0.05 **non-tempo**, and
below 0 **zugzwang**. Each detailed line gets a **tempo ledger**: every walked ply is labelled
`threatening`, `neither`, `counter_threat` or `pure_reply`, from a null-move threat search (a
threat is mate or at least one pawn of material) and, after a threatening ply, a reply search
(budget `L`). A threatening ply answered by a pure reply **gains a tempo**; an even trade
threatens nothing by itself. An **initiative run** (`rule`) is a series of one side's tempo gains;
it ends `spent`, `contested`, `runs` (the line turns into a tactic) or `unfinished`, and is
`converted` when its holder wins material or a positional change in use during the run, otherwise
`evaporated`. The tempo ledger is skipped in check and where a null move is unreliable.

Loaded data digests use the in-tree SHA-256 implementation. When the loaded bytes match their
pins, `games.table` reports `lichess_broadcast_2200@e0cb7fab3212` and `opening.dataset` reports
`lichess-org/chess-openings@a6189a3`; otherwise each reports `custom@` plus the first 12
SHA-256 hex digits. For a changed openings dataset, the digest covers `a.tsv` through `e.tsv`
concatenated in that order.


M2 motif definitions were written after consulting lichess-puzzler, and `removal_of_defender`
follows its `capturing_defender` theme; the Lichess puzzle database (CC0) provides the theme tags
used as the validation reference.

## Requirements and setup

A C++ compiler (g++ 14 or clang 19), CMake 3.25 or newer, and Ninja are required to build. Python 3 and the development requirements are needed for the report-schema and `games_oracle` tests.

```sh
git submodule update --init --recursive
tools/fetch_assets.sh engine openings
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements-dev.txt
```

The asset command fetches Stockfish and opening tables. After building, run
`tools/fetch_assets.sh games` to fetch the 81 monthly broadcast PGNs and build the game-count table
(about 167 s). On Debian, if creating the virtual environment fails because `venv` is unavailable,
install `python3-venv` first.

## Build and test

```sh
cmake --preset release && cmake --build --preset release
tools/fetch_assets.sh games
ctest --preset release
```

CTest's `unit` label does not need an engine; `engine` tests need the fetched Stockfish asset; `games`
needs the broadcast months. `tactics_unit_tests` checks the T fixtures (motifs) and C fixtures
(hand-labelled contributions and tactic labels) in CTest's `unit` suite; `tactics_puzzles` measures
recall, and `tactics_oracle` checks exact agreement with the independent reference implementation.
`positional_oracle` compares detectors with an independent reference on puzzle boards; P
fixtures are in `tests/fixtures/positional.json`, and engine fixtures F25–F30 cover positional facts.
Both oracles run every 31st test puzzle (300 of 9,300) under `unit`; `tactics_oracle_corpus` and
`positional_oracle_corpus` (label `corpus`) run every puzzle (53,378 positional boards).
`games_oracle` independently rebuilds the table with `tools/count_games.py` and requires byte-identical
output.
Use `tools/run_tests.py --build-dir build/release` to show live progress with concise test output;
pass CTest arguments such as `-R 'cli_fixture_F0[1-3]' -j4` after the build directory. Alternatively,
run `ctest --test-dir build/release -V` to stream all test output directly. The runner also atomically
writes `<build-dir>/test_progress.json` (override with `--status-file PATH`); OMP discovers
`.omp/extensions/test-progress.ts` at startup and shows active runs and recent results in the TUI.
Engine fixture tests build each report once and check all four outputs from it; each takes two
CTest slots (`PROCESSORS 2`: our process plus one Stockfish).
With `CHESSWHY_CHECK_TIMING` on (the `release` preset), `cli_timing` (label `timing`) runs alone and
fails if a standard-budget F fixture takes more than 15 s.
The `asan` preset builds at `-O1` with AddressSanitizer and UndefinedBehaviorSanitizer, and its test
preset runs the labels `unit` and `sanitize` (F fixtures chosen to cover code paths; Release checks
the facts):

```sh
cmake --preset asan && cmake --build --preset asan
ctest --preset asan
```

## Use

Explain a position's best move (the default) or the moves named by `--moves`:

```sh
build/release/chesswhy --fen "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3" --moves playable
```

`--moves` takes a comma-separated list without spaces; the result is the union of its items:

| Item | Selects |
|---|---|
| a move (SAN or UCI) | that move, judged even when the engine did not list it |
| `best` | the engine's top line only (the default) |
| `topN` | the engine's first N lines, whatever their verdict (e.g. `top3`) |
| `playable` | every `best` or `okay` move |
| `inaccuracies`, `mistakes`, `blunders` | every move with that verdict |
| `unplayable` | every inaccuracy, mistake and blunder |
| `all` | every legal move |

The keywords from `playable` to `all` judge every legal move and take several times the default
time. `--json` chooses the format and `--full-report` the content, independently:

| Flags | Output |
|---|---|
| none | move report text: one section per selected move |
| `--json` | move report document (`schema/moves.v8.schema.json`) |
| `--full-report` | full report text: every computed fact |
| `--json --full-report` | full report JSON (`schema/report.v8.schema.json`) |

The default move report lays out each selected move as a position walk: after the move's verdict
rows, each position on its line gets a block with its FEN, its evaluation for White, the move
played from it (except after the last walked ply), and the motifs, tactic labels, threat rows and
positional changes assigned to that ply. Tempo facts appear only in the JSON and the full report
text.

A move report keeps the move's verdict, line, consequence, tactic and opportunity judgements and
refutation, and only the threats, motifs and positional changes that bear on that move; every
field is a field of the full report or a filter of one. A positional change enters it only when
it is a **net change** (it differs from the root) that passes the **use test**: the line uses it
(a weak pawn is attacked or captured, a passed pawn advances, a rook uses its file, a king that
can still castle is pressed, a lead counts only in a tempo-dependent position) or it always
counts (material, the exchange, the bishop pair, a rook on the seventh, an outpost, a bad
bishop). The full report marks each change's `net_change` and `in_use`. After the core claims,
the move report's groups follow its `impact_order`: the group with the highest consequence rank
first (parried threat, created threat, motifs); ties keep tempo and dynamic positional facts
first in a tempo-dependent position, static positional facts first otherwise.

Analyze a position from a PGN. `--game` is 1-based; `--ply` counts half-moves from the game's start, with 0 denoting the initial position. Before White's move N, use ply `2(N−1)`; before Black's move N, use ply `2N−1`. Budgets are `quick`, `standard` (default), or `deep`.
`--games FILE` selects the broadcast-count table (default
`assets/data/games/lichess_broadcast_2200.tsv`). A missing or invalid table fails with exit 3 and one
`chesswhy: error: ...` line.

Analysis is position-only: PGN history selects the position but is not an input to the searches or
rules. The playable moves listed are those among the lines the engine searched; with `best` or
`topN`, unlisted moves are not judged and may also be playable. `playable_truncated` is true iff
the engine returned all K lines, K is less than the number of legal moves, and every line is
playable.

```sh
build/release/chesswhy --pgn tests/fixtures/two_games.pgn --game 1 --ply 2 --json --full-report --budget quick
```

Save the full report JSON as `full.json` and the move report document of the same command (without
`--full-report`) as `moves.json`, then run the semantic checker; it picks each document's schema
from its `document` field (add more paths to validate several files):

```sh
.venv/bin/python tools/validate_report.py schema/report.v8.schema.json full.json moves.json
.venv/bin/python tools/validate_report.py schema/report.v8.schema.json --projection full.json moves.json
```

The checker validates the schema first, then checks report relations it cannot express: loss and
verdict thresholds, FEN side/check/legal-move counts, legal move pairs and lines, the asked-move
copy, tactic and opportunity relations, held-piece restriction moves and `held_to`, tempo values
and labels, ledger labels and tempi, initiative runs and outcomes, net changes and the use test.
`--projection` also checks that every move report field is the full report's field or a filter
of one. It uses python-jsonschema and python-chess from the development requirements.


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
