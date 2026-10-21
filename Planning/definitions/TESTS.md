# ChessWhy — Tests

Every CTest in the build, grouped by test program, with its label and what it
checks. Terms are defined in [TERMINOLOGY.md](TERMINOLOGY.md) (§6.3 for labels,
gates and builds); fixtures are listed in [FIXTURES.md](FIXTURES.md).

**Snapshot:** 2026-10-21, commit `a4b6292` plus the uncommitted F21–F24 and
C01–C16 fixtures: 107 CTests (80 `unit`, 27 `engine`, no `model` yet). Release
run: all pass except `tactic fixtures exercise contributions and labels` (the
seven C-fixture mismatches in FIXTURES.md §6).

**Keeping it current.** The orchestrator updates this file in the same commit
that adds, renames or removes a test, and checks it at each milestone end.
Catch2 test cases appear under their test-case name; the `engine_`, `report_`
and `render_` prefixes are added by the CMake files.

Run: `ctest --test-dir build/release` (all), `-L unit` (no engine), `-L engine`.
Engine tests need `assets/engines/stockfish` and fail without it.

## 1. Input — `tests/input/input_unit_tests.cpp` (`input_unit_tests`)

| Test | Label | Checks |
|---|---|---|
| FEN counter boundaries are enforced before chess-library narrowing | unit | Half-move and full-move counters out of range are rejected before conversion |
| FEN rejects invalid king, pawn, and castling boundaries | unit | Pawn on a back rank, extra king, castling right without its rook |
| PGN replay enforces the 75-move half-move limit | unit | Replaying a PGN past the 75-move rule is an input error |
| PGN unit fixtures preserve mainline positions | unit | U fixtures: the PGN game and ply give the stored FEN |
| all move parsing fixtures resolve to their expected UCI moves | unit | Parse fixtures P01–P13: SAN and UCI input both resolve |
| annotated PGN reader preserves mainline SAN, NAGs, and headers | unit | `annotated.pgn`: comments and variations skipped, mainline, NAGs and headers kept |
| four- and six-field FEN fixture normalization | unit | Four-field FENs get default counters; normalisation drops an unusable en-passant square |
| input error fixtures report the required function-level fragments | unit | E01–E11 at the library level: the error text contains the required fragments |
| load_pgn selects requested annotated mainline positions | unit | Game and ply selection in `annotated.pgn` |
| load_pgn stops before errors in later games | unit | A broken later game does not stop reading an earlier one |
| negative PGN ply is rejected | unit | `--ply` below zero is an input error |

## 2. Engine — `tests/engine/`

`engine_unit_tests.cpp` (`engine_unit_tests`) parses canned UCI text; no engine runs.

| Test | Label | Checks |
|---|---|---|
| engine_parse Stockfish info lines with centipawn scores | unit | `info` lines with `cp` scores, WDL and PV |
| engine_parse positive and negative mate scores | unit | `mate N` for both sides |
| engine_discard incomplete or bound Stockfish info lines | unit | Lines without a PV or with `lowerbound`/`upperbound` are ignored |
| engine_default multipv and reject malformed numeric fields | unit | Missing `multipv` means slot 1; a line with malformed numbers is not parsed |
| engine_MultiPV collector accepts the implicit single slot | unit | A single-line search without `multipv` fields |
| engine_MultiPV collector rejects a search with no exact lines | unit | All-bound output is an error, not an empty result |
| engine_MultiPV collector reconciles a bound final F06 batch | unit | The F06 case where the last batch was bound: earlier exact lines are kept |
| engine_MultiPV bound move falls back to an unused exact slot line | unit | A move whose final line was bound gets an earlier exact line of an unused move |
| engine_MultiPV bound move omits a fallback already selected | unit | The fallback never lists the same move twice (the F18 `fxe8=Q` duplicate) |

`engine_engine_tests.cpp` (`engine_engine_tests`) starts the real Stockfish.

| Test | Label | Checks |
|---|---|---|
| engine_Stockfish searches are reproducible and honor request limits | engine | The same request twice gives identical lines; `searchmoves` restricts the search to the given move |
| engine_missing engine binary reports an EngineError with its path | engine | A wrong engine path fails with the path in the message (E15 at the library level) |

## 3. Openings — `tests/openings/openings_unit_tests.cpp` (`openings_unit_tests`)

| Test | Label | Checks |
|---|---|---|
| openings names match known positions and mainline ancestors | unit | Current and last-named opening for known positions and transpositions |
| openings table load failures identify their source | unit | A missing or broken openings table names the file (E16 at the library level) |

## 4. Analysis — `tests/analysis/unit_tests.cpp` (`analysis_unit_tests`)

Rules that turn engine numbers into report facts; no engine runs.

| Test | Label | Checks |
|---|---|---|
| analysis node budgets match the frozen search table | unit | Node counts per search and budget equal the plan's table |
| expected score keeps WDL units exact | unit | ES computed from WDL without rounding drift |
| verdict boundaries are inclusive at fine, mistake and blunder | unit | ES losses exactly at the thresholds: 60 fine, 260 mistake, 540 blunder (WDL units) |
| tactic margin boundary is inclusive | unit | A margin exactly at the tactic threshold counts as a tactic |
| consequence rank follows the frozen class table | unit | Order mate > material > promotion > positional |
| positive mate score takes precedence over other consequence classes | unit | A positive mate score makes the line class mate |
| quiet consequence is positional and a knight capture is material | unit | Basic class assignment |
| promotion counts piece change but remains promotion class below one pawn | unit | Promotion counted in material; class stays promotion under one pawn net |
| consequence nets captures made by both sides | unit | Material is the net of both sides' captures |
| consequence extends captures after six plies and stops at ten | unit | Capture extension past the six-ply horizon, capped at ten |
| walked plies follows the six-ply horizon and capture extension | unit | How many PV plies are walked |
| walked plies stops at illegal moves and the end of a short PV | unit | Short or broken PVs end the walk |
| illegal PV suffix stops consequence walking defensively | unit | An illegal engine move ends the walk instead of crashing |
| null move reliability depends on side-to-move non-pawn material | unit | The null-move threat search is marked unreliable when the side to move has at most one piece besides king and pawns |

## 5. Tactics — `tests/tactics/`

`tactics_unit_tests.cpp` (`tactics_unit_tests`):

| Test | Label | Checks |
|---|---|---|
| piece values and static exchange evaluation cover x-rays and pins | unit | SEE with x-ray attackers and pinned defenders |
| SEE does not treat en passant as a capture on the pawn square | unit | En-passant edge case in SEE |
| SEE permits king recapture only onto a safe square | unit | A king recaptures only when the square is not defended |
| state motifs include hanging pieces and pin kinds | unit | Hanging pieces, absolute and relative pins |
| hanging motifs follow the turn and suppress captures in progress | unit | A piece just captured in an exchange is not "hanging" |
| state motifs identify back-rank weakness and overloaded defenders | unit | Back-rank weakness, overloaded defender |
| state motifs find trapped pieces only when every legal escape stays lost | unit | Trapped piece needs all escapes to lose material |
| trapped pieces evaluate every legal escape on a side-to-move board | unit | Trapped-piece escapes are judged with the right side to move |
| state motifs are ordered and omitted for terminal positions | unit | Stable order; no motifs on mate or stalemate |
| a state motif is reported only on the ply it first appears | unit | No repeats of the same state motif along a line |
| move motifs detect forks and skewers | unit | Fork, skewer |
| move motifs detect discoveries and double check | unit | Discovered attack and check, double check |
| castling rook checks are not discoveries or solo double checks | unit | A castling rook giving check is not a discovery |
| move motifs detect back-rank mate and promotion | unit | Back-rank mate and queen promotion |
| motif fixtures exercise every frozen include and exclude | unit | T fixtures: every include found, no exclude found |
| contributions mark move motifs and later motif-piece activity direct | unit | Contribution rule: `direct` |
| contribution indirect attacks use pseudo-attacks and skip castling | unit | Contribution rule: `indirect` |
| contribution none covers Y-side, ply-n and ply-zero motifs | unit | Contribution rule: `none` cases |
| contributions treat castling and en passant as piece activity | unit | Special moves count as piece activity |
| removal-of-defender labels revive T33 and reject T34 | unit | The two former T fixtures as label tests |
| removal-of-defender labels reject near misses | unit | Similar positions that must not get the label |
| tactic fixtures exercise contributions and labels | unit | C fixtures: every hand-labelled contribution and label. **Fails today** on C01, C08, C09, C10, C12, C13, C14 |

Program tests:

| Test | Program | Label | Checks |
|---|---|---|---|
| `tactics_puzzles` | `tactics_puzzle_eval` (`puzzle_eval.cpp`) | unit | On the test puzzle sample: recall per measured tag must meet its frozen M2 target; also prints the recall table, Lichess agreement and the `removal_of_defender` line (not gated) |
| `tactics_oracle` | `tactics_motif_oracle` (`motif_oracle.cpp`, `oracle/reference.cpp`) | unit | Detectors against the independent reference implementation on every puzzle position and every T fixture; passes only with zero mismatches |

## 6. Report — `tests/report/`

`report_unit_tests.cpp` (`report_unit_tests`):

| Test | Label | Checks |
|---|---|---|
| report_JSON follows schema key order | unit | Keys come out in schema order (stable diffs, reproducible reports) |
| report_enum values use the schema spellings | unit | Nine report enums serialise to the schema's strings |
| report_scores and optional values are serialized | unit | ES, cp, mate and nulls |
| report_motif JSON uses the schema v3 shape | unit | Motif objects |
| report_tactic judgements and motif contributions serialize in report v3 | unit | Tactic judgement, labels and contributions |

Program tests:

| Test | Program | Label | Checks |
|---|---|---|---|
| `report_schema_examples` | `report_schema_examples` (`write_examples.cpp`) | unit | Writes eight example reports (terminal checkmate, side in check, normal, PGN opening, tactic with labels, forced, negative margin, refutation) |
| `report_schema_unit` | `tools/validate_report.py` | unit | Those eight examples validate against `schema/report.v3.schema.json` |

## 7. Render — `tests/render/render_unit_tests.cpp` (`render_unit_tests`)

Text rendering of a report, compared with `tests/render/golden/*.txt`.

| Test | Label | Checks |
|---|---|---|
| render_example reports match the golden text | unit | The eight example reports render byte for byte as the golden files |
| render_position evaluation renders the mate score when available | unit | The mate score is shown alongside ES |
| render_motif rows and level-independent output | unit | Motif rows; output does not change with `--level` where it should not |
| render_tactic judgements, labels and contributions render from report fields | unit | Tactic text uses only report fields |

## 8. CLI — `tests/cli/`

`unit_tests.cpp` (`cli_unit_tests`), no engine:

| Test | Label | Checks |
|---|---|---|
| CLI argument parsing applies defaults and validates usage | unit | Defaults (`club`, `standard`) and usage errors |
| CLI error fixtures return the required status and one diagnostic | unit | E fixtures: exit code and a single message line |
| FEN and SAN/UCI parse fixtures identify the same moves | unit | Parse fixtures: SAN and UCI forms give the same move |
| PGN opening fixtures describe current and last named positions | unit | U fixtures: current and last-named openings |

Program tests:

| Test | Program | Label | Checks |
|---|---|---|---|
| `cli_fixture_F01` … `cli_fixture_F24` (one per F fixture, F16 absent: 23 tests) | `cli_fixture_test` (`engine_fixture_test.cpp`) | engine | Runs the CLI on the fixture and checks its `equals` fields, `fine_includes`, `motifs_include` and `line_prefix`; runs the invariant check on every tactic judgement; with `CHESSWHY_CHECK_TIMING` on, fails above 12 s; saves the report for `report_schema` |
| `report_schema` | `tools/validate_report.py` | engine | Every saved F report validates against `schema/report.v3.schema.json`; runs after all `cli_fixture_*` tests |
| `cli_rerun_identity` | `cli_identity_test` (`identity_test.cpp`) | engine | F01, F03, F12 (JSON) and a text report for a fixed position (`Nf6` after 3.Qh5) are byte-identical across two runs |

## 9. pgntool — `tests/pgntool/pgntool_unit_tests.cpp` (`pgntool_unit_tests`)

Developer tool that lists PGN moves or games as TSV; compared with `tests/pgntool/golden/`.

| Test | Label | Checks |
|---|---|---|
| pgntool move TSV matches golden output | unit | Per-move table for `annotated.pgn` |
| pgntool game TSV matches golden output regardless of flag order | unit | Per-game table; `--games` before or after the file |
| pgntool selects a single game regardless of flag order | unit | `--game N` |
| pgntool reports argument and input errors without stdout | unit | Missing file, game out of range: message on stderr only |
| pgntool help takes precedence anywhere in the arguments | unit | `--help` wins |
