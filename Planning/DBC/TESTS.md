# ChessWhy — Tests

Every CTest in the build, grouped by test program, with its label, what it checks
and the concept nodes of [CONCEPTS.md](CONCEPTS.md) it checks (their code is in
[CODE.md](CODE.md)). Terms are defined in [DEFINITIONS.md](DEFINITIONS.md) (§6.3 for
labels, gates and builds); fixtures are listed in [FIXTURES.md](FIXTURES.md).

**Snapshot:** 2026-10-22, M3 completion-review fixes (legal en passant in the position
key, mainline-only Python oracle): 141 CTests (107 `unit`, 33 `engine`, 1 `games`, no
`model` yet), all passing in Release; `tactics_unit_tests` has 26 test cases.

**Keeping it current.** The orchestrator updates this file in the same commit
that adds, renames or removes a test, and checks it at each milestone end.
Catch2 test cases appear under their test-case name; the `engine_`, `report_`
and `render_` prefixes are added by the CMake files.

Run: `ctest --test-dir build/release` (all), `-L unit` (no engine), `-L engine`.
Engine tests need `assets/engines/stockfish` and fail without it.

## 1. Input — `tests/input/input_unit_tests.cpp` (`input_unit_tests`)

| Test | Label | Checks | Concepts |
|---|---|---|---|
| FEN counter boundaries are enforced before chess-library narrowing | unit | Half-move and full-move counters out of range are rejected before conversion | `FenInput`, `InputError` |
| FEN rejects invalid king, pawn, and castling boundaries | unit | Pawn on a back rank, extra king, castling right without its rook | `FenInput`, `InputError` |
| PGN replay enforces the 75-move half-move limit | unit | Replaying a PGN past the 75-move rule is an input error | `PgnInput`, `InputError` |
| PGN unit fixtures preserve mainline positions | unit | U fixtures: the PGN game and ply give the stored FEN | `PgnInput`, `GameMainline`, `InputFixtures` |
| all move parsing fixtures resolve to their expected UCI moves | unit | Parse fixtures P01–P13: SAN and UCI input both resolve | `AskedMove`, `FenInput`, `InputFixtures` |
| annotated PGN reader preserves mainline SAN, NAGs, and headers | unit | `annotated.pgn`: comments and variations skipped, mainline, NAGs and headers kept | `PgnInput`, `GameMainline` |
| four- and six-field FEN fixture normalization | unit | Four-field FENs get default counters; normalisation drops an unusable en-passant square | `FenInput`, `PositionKey`, `InputFixtures` |
| input error fixtures report the required function-level fragments | unit | E01–E11 at the library level: the error text contains the required fragments | `InputError`, `InputFixtures` |
| load_pgn selects requested annotated mainline positions | unit | Game and ply selection in `annotated.pgn` | `PgnInput`, `GameMainline` |
| load_pgn stops before errors in later games | unit | A broken later game does not stop reading an earlier one | `PgnInput`, `GameMainline`, `InputError` |
| negative PGN ply is rejected | unit | `--ply` below zero is an input error | `PgnInput`, `InputError` |
| streaming PGN reader reports a broken game and continues | unit | `PgnReader`: a game with an illegal move comes back with its error and the next game is read | `PgnInput`, `GtLegal` |
| streaming PGN reader skips moves after its ply limit | unit | `PgnReader` with `max_plies`: moves past the limit are never parsed, so an illegal move there is no error | `PgnInput`, `GtLegal` |

## 2. Engine — `tests/engine/`

`engine_unit_tests.cpp` (`engine_unit_tests`) parses canned UCI text; no engine runs.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| engine_parse Stockfish info lines with centipawn scores | unit | `info` lines with `cp` scores, WDL and PV | `ExpectedScore`, `StockfishProcess` |
| engine_parse positive and negative mate scores | unit | `mate N` for both sides | `ExpectedScore`, `StockfishProcess` |
| engine_discard incomplete or bound Stockfish info lines | unit | Lines without a PV or with `lowerbound`/`upperbound` are ignored | `StockfishProcess` |
| engine_default multipv and reject malformed numeric fields | unit | Missing `multipv` means slot 1; a line with malformed numbers is not parsed | `SearchA`, `StockfishProcess` |
| engine_MultiPV collector accepts the implicit single slot | unit | A single-line search without `multipv` fields | `SearchA`, `StockfishProcess` |
| engine_MultiPV collector rejects a search with no exact lines | unit | All-bound output is an error, not an empty result | `SearchA`, `StockfishProcess` |
| engine_MultiPV collector reconciles a bound final F06 batch | unit | The F06 case where the last batch was bound: earlier exact lines are kept | `SearchA`, `StockfishProcess` |
| engine_MultiPV bound move falls back to an unused exact slot line | unit | A move whose final line was bound gets an earlier exact line of an unused move | `SearchA`, `StockfishProcess` |
| engine_MultiPV bound move omits a fallback already selected | unit | The fallback never lists the same move twice (the F18 `fxe8=Q` duplicate) | `SearchA`, `StockfishProcess` |

`engine_engine_tests.cpp` (`engine_engine_tests`) starts the real Stockfish.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| engine_Stockfish searches are reproducible and honor request limits | engine | The same request twice gives identical lines; `searchmoves` restricts the search to the given move | `SearchA`, `SearchC`, `NodeBudget`, `StockfishProcess` |
| engine_missing engine binary reports an EngineError with its path | engine | A wrong engine path fails with the path in the message (E15 at the library level) | `StockfishProcess`, `InputError` |

## 3. Openings — `tests/openings/openings_unit_tests.cpp` (`openings_unit_tests`)

| Test | Label | Checks | Concepts |
|---|---|---|---|
| openings names match known positions and mainline ancestors | unit | Current and last-named opening for known positions and transpositions | `OpeningNames`, `LastNamed`, `PositionKey` |
| openings table load failures identify their source | unit | A missing or broken openings table names the file (E16 at the library level) | `InputError`, `OpeningNames` |
| opening digest pins match the asset fetch script | unit | The five `kDatasetSha256` pins equal the SHA-256 values `tools/fetch_assets.sh` checks | `DataDigest`, `OpeningNames` |
| opening dataset identity follows all five loaded file bytes | engine | A byte-identical copy of the five files loads as `kDataset`; one changed byte gives `custom@` and the first 12 hex digits of the digest; missing data fails (needs the files in `assets/`; label `unit` before checkpoint 2, 2026-10-22) | `DataDigest`, `OpeningNames` |

## 4. Analysis — `tests/analysis/unit_tests.cpp` (`analysis_unit_tests`)

Rules that turn engine numbers into report facts; no engine runs.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| analysis node budgets match the frozen search table | unit | Node counts per search and budget equal the plan's table | `NodeBudget`, `Settings` |
| expected score keeps WDL units exact | unit | ES computed from WDL without rounding drift | `ExpectedScore` |
| verdict boundaries are inclusive at fine, mistake and blunder | unit | ES losses exactly at the thresholds: 60 fine, 260 mistake, 540 blunder (WDL units) | `Loss`, `Verdict`, `ExpectedScore` |
| tactic margin boundary is inclusive | unit | A margin exactly at the tactic threshold counts as a tactic | `OnlyMoveMargin`, `TacticJudgement` |
| consequence rank follows the frozen class table | unit | Order mate > material > promotion > positional | `CqIsMate`, `CqIsMaterial`, `CqIsPromotion`, `CqIsPositional` |
| positive mate score takes precedence over other consequence classes | unit | A positive mate score makes the line class mate | `CqMate`, `CqIsMate`, `Consequence` |
| quiet consequence is positional and a knight capture is material | unit | Basic class assignment | `CqIsPositional`, `CqIsMaterial`, `CqMaterial` |
| promotion counts piece change but remains promotion class below one pawn | unit | Promotion counted in material; class stays promotion under one pawn net | `CqPromotion`, `CqMaterial`, `CqIsPromotion` |
| consequence nets captures made by both sides | unit | Material is the net of both sides' captures | `CqMaterial` |
| consequence extends captures after six plies and stops at ten | unit | Capture extension past the six-ply horizon, capped at ten | `WalkedPlies`, `CqMaterial` |
| walked plies follows the six-ply horizon and capture extension | unit | How many PV plies are walked | `WalkedPlies` |
| walked plies stops at illegal moves and the end of a short PV | unit | Short or broken PVs end the walk | `WalkedPlies` |
| illegal PV suffix stops consequence walking defensively | unit | An illegal engine move ends the walk instead of crashing | `WalkedPlies`, `CqMaterial` |
| null move reliability depends on side-to-move non-pawn material | unit | The null-move threat search is marked unreliable when the side to move has at most one piece besides king and pawns | `NullMoveReliable`, `SearchB` |
| UCI move line retains a positive search depth | unit | The parsed `depth` of a PV line is kept for `depth` in the report | `CandidateMove` |

## 5. Tactics — `tests/tactics/`

`tactics_unit_tests.cpp` (`tactics_unit_tests`):

| Test | Label | Checks | Concepts |
|---|---|---|---|
| piece values and static exchange evaluation cover x-rays and pins | unit | SEE with x-ray attackers and pinned defenders | `StaticExchange` |
| SEE does not treat en passant as a capture on the pawn square | unit | En-passant edge case in SEE | `StaticExchange` |
| SEE permits king recapture only onto a safe square | unit | A king recaptures only when the square is not defended | `StaticExchange` |
| state motifs include hanging pieces and pin kinds | unit | Hanging pieces, absolute and relative pins | `StateMotif` |
| hanging motifs follow the turn and suppress captures in progress | unit | A piece just captured in an exchange is not "hanging" | `StateMotif`, `StaticExchange` |
| state motifs identify back-rank weakness and overloaded defenders | unit | Back-rank weakness, overloaded defender | `StateMotif` |
| state motifs find trapped pieces only when every legal escape stays lost | unit | Trapped piece needs all escapes to lose material | `StateMotif`, `StaticExchange` |
| trapped pieces evaluate every legal escape on a side-to-move board | unit | Trapped-piece escapes are judged with the right side to move | `StateMotif` |
| state motifs are ordered and omitted for terminal positions | unit | Stable order; no motifs on mate or stalemate | `StateMotif` |
| a state motif is reported only on the ply it first appears | unit | No repeats of the same state motif along a line | `FirstAppearance` |
| move motifs detect forks and skewers | unit | Fork, skewer | `MoveMotif` |
| move motifs detect discoveries and double check | unit | Discovered attack and check, double check | `MoveMotif` |
| castling rook checks are not discoveries or solo double checks | unit | A castling rook giving check is not a discovery | `MoveMotif` |
| move motifs detect back-rank mate and promotion | unit | Back-rank mate and queen promotion | `MoveMotif` |
| motif fixtures exercise every frozen include and exclude | unit | T fixtures: every include found (with `threat` for `pin_tactical`, and `restriction` where the row gives one), no exclude found | `MotifFixtures`, `StateMotif`, `MoveMotif`, `TacticalPin`, `Restriction` |
| contributions mark move motifs and X captures direct | unit | Contribution rule: `direct` = a move motif, or a later capture by the tactic side of one of the motif's defending-side pieces; the defender's own moves do not count | `Contribution`, `CoDirect`, `MoveMotif`, `ContributionFixtures` |
| indirect contributions require a denied reply that answers an X move | unit | Rule 3′: `indirect` = a restricted move of the held piece (or the king's step onto a blocker's square) answers a later tactic-side move: escapes check or captures on its landing square; a pinned piece may capture only on its pin line; continuity broken once a motif piece moves | `Contribution`, `CoDenied`, `CoIndirect`, `ContributionFixtures`, `TacticSequence` |
| contribution none covers Y-side, ply-n and ply-zero motifs | unit | Contribution rule: `none` cases | `Contribution`, `CoNotUsed`, `ContributionFixtures` |
| contributions count X captures, not castling movement, as direct | unit | En passant capture of a motif pawn is direct; castling the motif's rook is not | `Contribution`, `CoDirect`, `ContributionFixtures` |
| motif-piece movement is not direct and blocks later indirect attribution | unit | The tactic side moving its own motif piece (e.g. the pinner) is not direct, and once any motif piece has moved or been captured, later moves cannot make it indirect | `Contribution`, `CoDirect`, `CoIndirect`, `ContributionFixtures` |
| removal-of-defender labels revive T33 and reject T34 | unit | The two former T fixtures as label tests | `TacticLabel`, `ContributionFixtures` |
| removal-of-defender labels reject near misses | unit | Similar positions that must not get the label | `TacticLabel`, `ContributionFixtures` |
| denied moves and answers apply restricted-reply rules | unit | `denied_moves` per tag (back-rank steps with the blocker removed, pins off the segment, overloaded defender, tactical pin) and `answers` for check and capture replies | `CoDenied`, `CoIndirect`, `Restriction` |
| restricted movement reports held moves and line constraints | unit | `restriction` lists restricted and free moves on the held side's board; `held_to` is a square, a file, rank or diagonal, or listed squares; motifs without a restricted move are dropped | `Restriction`, `StateMotif`, `TacticalPin` |
| restricted pawn move sets include hypothetical captures | unit | A pinned pawn's captures onto an empty square, onto its own piece's square and onto the last rank (four promotions each) are restricted moves; its push along the line is free | `Restriction`, `StateMotif` |
| a held piece other than a pawn without legal moves restricts nothing | unit | An overloaded rook boxed in by its own knights is not reported (existence condition; non-pawns have no hypothetical moves) | `Restriction`, `StateMotif` |
| held move restrictions never include capturing a king | unit | Puzzle UG08H after g2-f3 Nc6-d4: White is in check from the knight; the overloaded knight's restricted moves exclude d4-f3, which would take the king on the board with Black to move | `Restriction`, `StateMotif` |
| tactic fixtures exercise contributions and labels | unit | C fixtures: every hand-labelled contribution and label; `pin_tactical` is checked only where an entry lists it (with `threat`), not in the exact set | `ContributionFixtures`, `Contribution`, `TacticLabel`, `TacticalPin` |
| pin tactical options and threat ordering are honored | unit | `MotifOptions{.pin_tactical = false}` yields no `pin_tactical` (position and line); order by threat kind, then square (T48's board); a rook that still holds a castling right can be the restricted piece | `TacticalPin`, `TpPiece`, `TpNewThreat`, `TpFound` |
| pin tactical restrictions do not use expired en passant rights | unit | The restriction board is the position after a null move, so an en-passant capture threat that needs the expired right does not make a `pin_tactical` | `TacticalPin`, `TpMoves`, `TpAnyRestricted`, `Restriction` |
| pin tactical identity includes its threat | unit | On a legal root with neither king in check, the same actor and targets (Qa2 → g7, h8) with a different threat square (a1 at ply 0, b2 after Rb1-a1) is a new instance in `line_motifs` | `TacticalPin`, `TpFound`, `LineMotifs` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `tactics_puzzles` | `tactics_puzzle_eval` (`puzzle_eval.cpp`) | unit | On the test puzzle sample (with `pin_tactical` switched off): recall per measured tag must meet its frozen M2 target; also prints the recall table, the untagged firing rate, Lichess agreement and the `removal_of_defender` line (not gated) | `PuzzleSample`, `Recall`, `TacticLabel`, `MotifFixtures` |
| `tactics_oracle` | `tactics_motif_oracle` (`motif_oracle.cpp`, `oracle/reference.cpp`) | unit | Detectors against the independent reference implementation on every puzzle position and every T fixture; passes only with zero mismatches. `pin_tactical` is compared on every 16th puzzle (0-based data row index divisible by 16; 582 puzzles) and on every T fixture. CTest `TIMEOUT 3600` (about 24 min under ASan/UBSan) | `ReferenceImplementation`, `MotifFixtures`, `PuzzleSample`, `TacticalPin` |

## 6. Report — `tests/report/`

`report_unit_tests.cpp` (`report_unit_tests`):

| Test | Label | Checks | Concepts |
|---|---|---|---|
| report_JSON follows schema key order | unit | Keys come out in schema order (stable diffs, reproducible reports) | `ReportSchema`, `JsonOutput`, `Reproducible` |
| report_enum values use the schema spellings | unit | Nine report enums serialise to the schema's strings | `ReportSchema`, `JsonOutput` |
| report_scores and optional values are serialized | unit | ES, cp, mate and nulls | `ExpectedScore`, `ReportSchema`, `JsonOutput` |
| report_motif JSON uses the schema v5 shape | unit | Motif objects, including `threat` (`{kind, square}` for `pin_tactical`, null otherwise) and `restriction` | `ReportSchema`, `StateMotif`, `TacticalPin`, `Restriction`, `JsonOutput` |
| report_tactic judgements and motif contributions serialize in report v5 | unit | Tactic judgement, labels and contributions | `ReportSchema`, `TacticJudgement`, `TacticLabel`, `Contribution` |
| report_v5 analysis fields and restriction serialize | unit | `legal_moves`, `depth`, `walked_plies`, `creates_threat_skipped` and `restriction` with `held_to` | `ReportSchema`, `CandidateMove`, `FineSet`, `CreatedThreat`, `Restriction` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `report_schema_examples` | `report_schema_examples` (`write_examples.cpp`) | unit | Writes eight example reports (terminal checkmate, side in check, normal, PGN opening, tactic with labels, forced, negative margin, refutation); they carry positional features and changes and a games source, found and not found | `ReportDocument`, `JsonOutput`, `ReportSchema`, `GameCounts` |
| `report_schema_unit` | `tools/validate_report.py` | unit | Those eight examples validate against `schema/report.v5.schema.json` | `ReportSchema` |

## 7. Render — `tests/render/render_unit_tests.cpp` (`render_unit_tests`)

Text rendering of a report, compared with `tests/render/golden/*.txt`.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| render_example reports match the golden text | unit | The eight example reports render byte for byte as the golden files | `TemplateRendering`, `ExplanationText`, `ReportDocument` |
| render_position evaluation renders the mate score when available | unit | The mate score is shown alongside ES | `TemplateRendering`, `ExpectedScore` |
| render_motif rows and level-independent output | unit | Motif rows; output does not change with `--level` where it should not | `ExplanationText`, `StateMotif`, `TacticalPin`, `TemplateRendering` |
| render_tactic judgements, labels and contributions render from report fields | unit | Tactic text uses only report fields; pins, overloaded defenders and `pin_tactical` print as "restricted movement (absolute pin / relative pin / overloaded defender / tactic: Nd5-f4 fork)" | `TacticJudgement`, `TacticLabel`, `Contribution`, `RestrictedMovement` |
| render_positional and game facts render only from report fields | unit | Positional change rows (`+`/`−`, ply, `not at end`, value) under each line, `position.positional` not printed; the games row after the opening (found or not in the table) and the per-move row; no ranking words | `TemplateRendering`, `TiedToLine`, `GameCounts` |
| render_PostM3_EM2 template wording matches its golden lines | unit | Conditions ("if Black passes", deferred parry, skip reason, facts read to ply k of n, engine and budget, candidates of legal moves), `SEE` for a capture and held-to wording render as in the plan | `TemplateRendering`, `Conditions`, `RestrictedMovement` |

## 8. CLI — `tests/cli/`

`unit_tests.cpp` (`cli_unit_tests`), no engine:

| Test | Label | Checks | Concepts |
|---|---|---|---|
| CLI argument parsing applies defaults and validates usage | unit | Defaults (`club`, `standard`, the game-count table under `assets/`), `--games PATH`, usage errors; a missing game-count table exits 3 with one message | `CommandLine`, `Settings`, `InputError`, `GameCounts` |
| CLI error fixtures return the required status and one diagnostic | unit | E fixtures: exit code and a single message line | `CommandLine`, `InputFixtures`, `InputError` |
| FEN and SAN/UCI parse fixtures identify the same moves | unit | Parse fixtures: SAN and UCI forms give the same move | `FenInput`, `AskedMove`, `InputFixtures` |
| PGN opening fixtures describe current and last named positions | unit | U fixtures: current and last-named openings | `PgnInput`, `GameMainline`, `OpeningNames`, `LastNamed` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `cli_fixture_F01` … `cli_fixture_F31` (one per F fixture, F16 absent: 30 tests) | `cli_fixture_test` (`engine_fixture_test.cpp`) | engine | Runs the CLI on the fixture and checks its `equals` fields, `fine_includes`, `motifs_include`, `positional_include`, `line_prefix` and `text_contains` (in the text report); with `CHESSWHY_CHECK_TIMING` on, fails above 12 s; saves the report for `report_schema` | `EngineFixtures`, `Verdict`, `TacticJudgement`, `StateMotif`, `Conditions` |
| `report_schema` | `tools/validate_report.py` | engine | Every saved F report validates against `schema/report.v5.schema.json` and passes the semantic check (verdict and loss, mate and score, parry test, urgency, legal lines, asked-move copy, threat skips, restrictions, tactic invariants); runs after all `cli_fixture_*` tests | `ReportSchema`, `SemanticCheck`, `EngineFixtures` |
| `cli_rerun_identity` | `cli_identity_test` (`identity_test.cpp`) | engine | F01, F03, F12, F25, F29, F30 (JSON) and a text report for a fixed position (`Nf6` after 3.Qh5) are byte-identical across two runs | `RerunIdentity`, `Reproducible`, `ReportDocument`, `JsonOutput` |

## 9. pgntool — `tests/pgntool/pgntool_unit_tests.cpp` (`pgntool_unit_tests`)

Developer tool that lists PGN moves or games as TSV; compared with `tests/pgntool/golden/`.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| pgntool move TSV matches golden output | unit | Per-move table for `annotated.pgn` | `PgnInput`, `GameMainline` |
| pgntool game TSV matches golden output regardless of flag order | unit | Per-game table; `--games` before or after the file | `PgnInput`, `GameMainline`, `CommandLine` |
| pgntool selects a single game regardless of flag order | unit | `--game N` | `PgnInput`, `GameMainline`, `CommandLine` |
| pgntool reports argument and input errors without stdout | unit | Missing file, game out of range: message on stderr only | `CommandLine`, `InputError`, `PgnInput` |
| pgntool help takes precedence anywhere in the arguments | unit | `--help` wins | `CommandLine` |

## 10. Positional — `tests/positional/`

`positional_unit_tests.cpp` (`positional_unit_tests`):

| Test | Label | Checks | Concepts |
|---|---|---|---|
| pawn structure handles edge files and both relative directions | unit | Isolated, doubled and island rules on the a and h files, for both colours | `PawnStructure` |
| passed pawns, connected runs, backward pawns and chains follow their boundaries | unit | Passed, connected passed, backward and chain rules at their rank and file limits | `PawnStructure` |
| castling, king-zone attackers and rook captures are tracked | unit | Castling rights, including a right lost when the rook is captured; king-zone attackers | `KingSafety` |
| king files use both edges and relative shield ranks | unit | King files clipped at the board edge; shield gap only for a king on relative rank 1 or 2 | `KingSafety` |
| piece features use their square, file, and colour rules | unit | Bishop pair, rooks on open, half-open and seventh-rank files, outposts, bad bishops | `PieceActivity` |
| mobility, space, and development leads include thresholds but not one below | unit | Each lead at its threshold and one below | `SpaceDevelopment` |
| positional identity follows each tag's identity column | unit | `same_identity` for every tag | `PcCompare`, `PcSame` |
| line changes compare identities and hold at the final position | unit | Appears and disappears per ply, `holds_at_end`, ordering | `PositionalChange`, `PcAppears`, `PcDisappears`, `PcEnd`, `PcHolds`, `PcUndone` |
| promotions, en passant and identity persistence are reflected along a line | unit | A promoted piece, an en-passant capture, and a feature whose pieces or value change keeping its identity | `PositionalChange`, `PcBoards`, `PcSame` |
| positional P fixtures exercise every include and exclude | unit | P fixtures: every hand-labelled feature and change reported, every excluded one absent | `PositionalFixtures`, `PawnStructure`, `KingSafety`, `PieceActivity`, `SpaceDevelopment`, `MaterialBalance`, `PositionalChange` |
| positional fixture matching handles locally constructed rows | unit | The fixture matcher itself on rows built in the test | `PositionalFixtures` |

Program tests (`tests/positional/oracle/`):

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `positional_reference_self_check` | `positional_reference_self_check` (`reference_self_check.cpp`) | unit | The independent reference on its own hand cases for every rule group | `PositionalReference` |
| `positional_oracle` | `positional_oracle` (`positional_oracle.cpp`, `reference.cpp`) | unit | Detectors against the independent reference on every board of every test-sample puzzle line (53,378 boards, 9,300 lines) and on every P fixture board: features, line changes and the mobility, space and development counts; passes only with zero mismatches; prints the per-tag board counts | `PositionalReference`, `PuzzleSample`, `PositionalFixtures`, `PcFeatures` |

## 11. Games — `tests/games/`

`games_unit_tests.cpp` (`games_unit_tests`):

| Test | Label | Checks | Concepts |
|---|---|---|---|
| games table counts first-failing exclusions in contract order | unit | Each exclusion reason (variant, setup, rating, result, illegal) counted once, by the first rule a game fails | `GtStandard`, `GtRating`, `GtResult`, `GtLegal`, `GtExcluded` |
| repeated positions contribute only their first occurrence per game | unit | A position repeated within a game counts once | `GtCount` |
| games table respects ply limit and excludes N-minus-one positions | unit | No position after ply 40; a position with N−1 games is dropped | `GtLegal`, `GtKeep`, `GtDropped` |
| minimum-count boundary, move tie ordering, and table lookup round trip | unit | N games kept; ties in move order; `Table::find` hit and miss after `build_table` | `GtKeep`, `GtRow`, `GameTable`, `GameCounts` |
| games table loader rejects malformed headers, rows, and ordering | unit | `Table::load` rejects bad headers, row shapes, counts and order; a row with an illegal move loads, and `find` on its position throws | `GameTable`, `GtRow` |
| position keys keep only legally capturable en passant squares | unit | After a double pawn step whose only capturing pawn is pinned (9...f5 in `games_edge.pgn`'s first game), the key's en passant field is `-`; with a legal capture (1.e4 a6 2.e5 d5) it keeps `d6` | `PositionKey` |
| games table digest pin matches the asset fetch script | unit | `kTableSha256` equals the SHA-256 value `tools/fetch_assets.sh` checks | `DataDigest`, `GameTable` |
| games table identity follows byte-identical pins | games | A byte-identical copy of the table loads as `kTable`; one changed byte gives `custom@` and the first 12 hex digits; a missing table fails (needs the table in `assets/`; label `unit` before checkpoint 2, 2026-10-22) | `DataDigest`, `GameCounts` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `games_oracle` | `chesswhy-games`, `tools/count_games.py` (`oracle.cmake`) | games | Both tools on the 2020-07 and 2022-08 months with N = 2 write byte-identical tables | `GamesOracle`, `GameTable`, `BroadcastMonths` |
| `games_oracle_edge` | `chesswhy-games`, `tools/count_games.py` (`oracle.cmake`) | unit | Both tools on `tests/fixtures/games_edge.pgn` (max ply 20, N = 1) write byte-identical tables: a pseudo-legal en passant square is left out of the key, and a variation with an illegal move is not counted | `GamesOracle`, `PositionKey`, `GtCount` |

## 12. Util — `tests/util/sha256_unit_tests.cpp` (`sha256_unit_tests`)

| Test | Label | Checks | Concepts |
|---|---|---|---|
| SHA-256 matches FIPS 180-4 vectors | unit | `sha256_hex` on the FIPS 180-4 test vectors, including the multi-block and one-million-`a` cases | `DataDigest` |
