# ChessWhy — Tests

Every CTest in the build, grouped by test program, with its label, what it checks
and the concept nodes of [CONCEPTS.md](CONCEPTS.md) it checks (their code is in
[CODE.md](CODE.md)). Terms are defined in [DEFINITIONS.md](DEFINITIONS.md) (§6.3 for
labels, gates and builds); fixtures are listed in [FIXTURES.md](FIXTURES.md).

**Snapshot:** 2026-10-24, M4 milestone-end gate: 179 CTests (134 `unit`, 43 `engine`,
2 `games`, no `model` yet), all passing in Release and ASan/UBSan.

**Keeping it current.** The orchestrator updates this file in the same commit
that adds, renames or removes a test, and checks it at each milestone end.
Catch2 test cases appear under their test-case name; the `engine_`, `report_`,
`move_report_` and `render_` prefixes are added by the CMake files.

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
| verdict boundaries use the playable and inaccuracy cutoffs | unit | Loss 100/101 and 200/201 WDL units; best vs okay by the same-score flag; above 200 mistake or, when the move allows a tactic, blunder | `Verdict`, `VdPlayable`, `VdBest`, `VdOkay`, `VdSmall`, `VdBlunder`, `VdMistake` |
| same score compares cp or mate, not expected score | unit | Same cp with different ES is the same score; cp 12 vs 13, mate vs cp and mate 2 vs 3 are not | `VdSameScore` |
| tactic and allows-tactic use the move's line consequence | unit | Mate and material classes are tactics, promotion and positional are not; material −1 or a negative mate allows a tactic, material 0 or a positive mate does not | `TacticTest`, `AllowsTactic`, `VdAllows` |
| opportunity margin boundary is inclusive | unit | 260 WDL units is an opportunity margin, 259 is not | `OnlyMoveMargin`, `TjMargin` |
| selection sets Search A width and nodes | unit | `best` and `top3` give five lines, `top7` seven, every whole-board keyword every legal move; nodes are 200k per line at standard; fewer legal moves cap the width | `SearchA`, `NodeBudget` |
| first five and selected lines are detailed | unit | Lines 1–5 are detailed, later lines only when selected | `CandidateMove`, `SearchD`, `SearchE`, `SearchR` |
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
| tempo labels use the frozen boundaries | unit | Tempo value −1 is zugzwang, 0 and 99 WDL units non-tempo, 100 and 200 no label, 201 tempo-dependent | `TempoLabel` |
| tempo-dependent positions require a labelled root value | unit | A missing root tempo value or one without the tempo-dependent label is not a tempo-dependent position | `TempoLabel`, `TempoValue` |
| tempo ledger labels cover threat, reply, counter-threat and neither | unit | Hand-built ply flags give threatening, pure reply, counter-threat and neither; only a threatening ply answered by a pure reply gains a tempo | `PlyLabel`, `TempoGained`, `PlFirst`, `PlStops`, `PlOwn`, `PlCounter`, `PlThreatening`, `PlNeither`, `PlCounterThreat`, `PlPure` |
| material balance uses pawn, minor, rook and queen values | unit | Material balance 1/3/5/9 from each side's view | `RunOutcome` |
| initiative runs identify each ending | unit | Spent, unfinished (walk ends first, also right after a tempo gained), contested and runs (mate, or an answer that does not stop the threat) with their start and end plies and tempi | `InitiativeRun`, `RunEnding` |
| initiative runs reject invalid labelled ledgers | unit | A ledger whose labels `label_ledger` cannot produce is a programming error (`std::logic_error`) | `InitiativeRun` |
| initiative scanning resumes at the previous run's ending ply | unit | A contested run's counter-threat can start the other side's run at its ending ply | `InitiativeRun`, `RunEnding` |
| initiative conversion checks material and used positional changes | unit | Converted by a material gain from the run's start to the end of the walk, or by a change in use inside the run that favours the holder; evaporated otherwise | `RunOutcome`, `FavouredSide` |

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
| report_JSON follows v7 report key order | unit | Keys come out in schema order (stable diffs, reproducible reports) | `ReportSchema`, `JsonOutput`, `Reproducible` |
| report_enum values use the schema spellings | unit | Nine report enums serialise to the schema's strings | `ReportSchema`, `JsonOutput` |
| report_scores and optional values are serialized | unit | ES, cp, mate and nulls | `ExpectedScore`, `ReportSchema`, `JsonOutput` |
| report_motif JSON uses the schema v7 shape | unit | Motif objects, including `threat` (`{kind, square}` for `pin_tactical`, null otherwise) and `restriction` | `ReportSchema`, `StateMotif`, `TacticalPin`, `Restriction`, `JsonOutput` |
| report_opportunity judgements and tactic-line fields serialize in v7 | unit | Opportunity judgement, `is_tactic`, `allows_tactic`, tactic labels, root motifs and contributions | `ReportSchema`, `OpportunityJudgement`, `TacticLine`, `TacticLabel`, `Contribution` |
| report_v7 analysis fields and restriction serialize | unit | `legal_moves`, `playable_truncated`, `depth`, `walked_plies`, `detailed`, `creates_threat_skipped` and `restriction` with `held_to` | `ReportSchema`, `CandidateMove`, `PlayableSet`, `CreatedThreat`, `Restriction` |
| report_tempo fields serialize in v7 | unit | Root and per-move `tempo_value`, the tempo ledger with plies, tempi and initiative runs, `tempo_ledger_skipped`, and `net_change` / `in_use` on positional changes | `ReportSchema`, `TempoValue`, `TempoLedger`, `NetChange`, `UseTest` |

`move_report_tests.cpp` (`move_report_unit_tests`), on hand-built full reports:

| Test | Label | Checks | Concepts |
|---|---|---|---|
| move_report_move reports select the keyword sets and preserve A order | unit | Each keyword selects exactly its moves, in A order | `MoveSelection`, `Verdict`, `PlayableSet` |
| move_report_move report selection unions top, keywords, and asked moves once | unit | `topN`, keywords and asked moves together give each move once; asked moves outside A come last | `MoveSelection`, `AskedMove`, `MoveRequest` |
| move_report_move report headers copy only the M4 position and game fields | unit | The document's position, opening and game-source headers equal the full report's fields | `MoveReport`, `RelevanceFilter` |
| move_report_move report relevance filter keeps conditional fields and drops irrelevant ones | unit | Refutation, parried threat, contributing motifs and the net positional change are kept when they bear on the move; other facts are dropped | `RelevanceFilter`, `MoveReport`, `Contribution`, `TiedToLine` |
| move_report_move report created threat keeps only the specified consequence classes | unit | A created threat is kept only when its consequence class wins something | `RelevanceFilter`, `CreatedThreat` |
| move_report_move report JSON uses the v7 moves shape and stable key order | unit | `to_json` writes `document: "moves"` with keys in schema order | `JsonOutput`, `MoveReport`, `Reproducible` |
| move_report_move report tempo and positional groups follow impact order | unit | Move report groups (parried threat, created threat, motifs, tempo rows, dynamic and static positional rows) come out in `impact_order`, ranked by consequence | `ImpactRating`, `MoveReport`, `TempoGained` |
| move_report_move report positional filtering and order follows root tempo dependence | unit | Only net changes that pass the use test are kept; dynamic facts come first in a tempo-dependent position, static facts first otherwise | `TiedToLine`, `RelevanceFilter`, `ImpactRating`, `TempoLabel` |
| move_report_move report tempo projections are null exactly when no ledger exists | unit | Tempo gained and initiative rows are null when the line has no tempo ledger and copied from it otherwise | `RelevanceFilter`, `TempoLedger`, `InitiativeRun` |
| move_report_impact order sorts ranked groups stably around both positional base orders | unit | Groups with the same consequence rank keep their base order, for both the dynamic-first and the static-first order | `ImpactRating`, `StaticScale` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `report_schema_examples` | `report_schema_examples` (`write_examples.cpp`) | unit | Writes eight example reports (terminal checkmate, side in check, normal, PGN opening, tactic with labels, forced, negative margin, refutation) and the move report document of each; they carry positional features and changes and a games source, found and not found | `ReportDocument`, `MoveReport`, `JsonOutput`, `ReportSchema`, `GameCounts` |
| `report_schema_unit` | `tools/validate_report.py` | unit | The eight full reports validate against `schema/report.v7.schema.json` | `ReportSchema` |
| `move_report_schema_unit` | `tools/validate_report.py` | unit | The eight move report documents validate against `schema/moves.v7.schema.json` | `ReportSchema`, `MoveReport` |

## 7. Render — `tests/render/render_unit_tests.cpp` (`render_unit_tests`)

Text rendering of a report, compared with `tests/render/golden/*.txt`.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| render_example reports match the golden text | unit | The eight example reports render byte for byte as the golden full report text | `TemplateRendering`, `ExplanationText`, `ReportDocument` |
| render_move report documents match golden text without candidate wording | unit | Six move report documents (best move, playable set, asked move outside A, blunder with refutation, empty selection, terminal position) render byte for byte as their golden files; no output contains "candidate" in any case | `TemplateRendering`, `MoveReport`, `Conditions` |
| render_non-detailed engine lines omit unsearched details | unit | An A line that was not detailed prints no parry, created threat or refutation, even if the fields were filled | `TemplateRendering`, `CandidateMove` |
| render_position evaluation renders the mate score when available | unit | The mate score is shown alongside ES | `TemplateRendering`, `ExpectedScore` |
| render_motif rows and level-independent output | unit | Motif rows; output does not change with `--level` where it should not | `ExplanationText`, `StateMotif`, `TacticalPin`, `TemplateRendering` |
| render_opportunities, tactic labels and contributions render from report fields | unit | Opportunity text uses only report fields; pins, overloaded defenders and `pin_tactical` print as "restricted movement (absolute pin / relative pin / overloaded defender / tactic: Nd5-f4 fork)" | `OpportunityJudgement`, `TacticLabel`, `Contribution`, `RestrictedMovement` |
| render_positional and game facts render only from report fields | unit | Positional change rows (`+`/`−`, ply, `not at end`, value) under each line, `position.positional` not printed; the games row after the opening (found or not in the table) and the per-move row; no ranking words | `TemplateRendering`, `TiedToLine`, `GameCounts` |
| render_PostM3_EM2 template wording matches its golden lines | unit | Conditions ("if Black passes", deferred parry, skip reason, facts read to ply k of n, engine and budget, engine lines of legal moves), `SEE` for a capture and held-to wording render as in the plan | `TemplateRendering`, `Conditions`, `RestrictedMovement` |
| render_full report tempo rows follow the v7 templates | unit | Root and after-move tempo values with their labels, the tempo ledger (with skip reasons), initiative runs, and ` (passing change)` / ` (fails the use test)` marks render as in the plan | `TemplateRendering`, `TempoValue`, `TempoLedger`, `InitiativeRun`, `RunOutcome`, `NetChange`, `UseTest` |

## 8. CLI — `tests/cli/`

`unit_tests.cpp` (`cli_unit_tests`), no engine:

| Test | Label | Checks | Concepts |
|---|---|---|---|
| CLI argument parsing applies defaults and validates usage | unit | Defaults (`club`, `standard`, the game-count table under `assets/`), `--games PATH`, usage errors; a missing game-count table exits 3 with one message | `CommandLine`, `Settings`, `InputError`, `GameCounts` |
| CLI error fixtures return the required status and one diagnostic | unit | E fixtures: exit code and a single message line | `CommandLine`, `InputFixtures`, `InputError` |
| FEN and SAN/UCI parse fixtures identify the same moves | unit | Parse fixtures: SAN and UCI forms give the same move | `FenInput`, `AskedMove`, `InputFixtures` |
| PGN opening fixtures describe current and last named positions | unit | U fixtures: current and last-named openings | `PgnInput`, `GameMainline`, `OpeningNames`, `LastNamed` |
| CLI move selection parses keywords, topN and moves | unit | The `--moves` grammar: no flag means `best`; each keyword, `topN`, SAN and UCI moves, mixed, deduplicated (keywords in fixed order, moves in typed order, the largest `topN`); an empty item, `top0`, `top` or an unparseable item exits with the input error status and one message | `MoveRequest`, `AskedMove`, `CommandLine`, `InputError` |
| CLI output flags select move/full content and text/JSON format | unit | P9 matrix on a fake report: no flag gives move report text, `--json` the move report document, `--full-report` the full report text, both the full report JSON | `JsonOutput`, `TemplateRendering`, `CommandLine` |

Program tests:

| Test | Program | Label | Checks | Concepts |
|---|---|---|---|---|
| `cli_fixture_F01` … `cli_fixture_F39` (one per F fixture, F16 absent: 38 tests) | `cli_fixture_test` (`engine_fixture_test.cpp`) | engine | Runs the CLI on the fixture with `--json --full-report` and checks its `equals` fields, `playable_includes`, `motifs_include`, `positional_include`, `line_prefix`; `text_contains` on the move report text (plain run), `full_text_contains` on the full report text (`--full-report`), and the `moves_` keys on the move report document (`--json`); repeats this for each entry of `runs`; with `CHESSWHY_CHECK_TIMING` on (Release), fails above 15 s on the default run (12 s before 2026-10-24) and runs alone (engine lock); without it (ASan preset) the fixtures run in parallel; CTest timeout 3600 s; prints a progress line per CLI run; saves both JSON documents for `report_schema` | `EngineFixtures`, `Verdict`, `AllowsTactic`, `TacticTest`, `OpportunityJudgement`, `MoveSelection`, `MoveReport`, `StateMotif`, `Conditions` |
| `report_schema` | `tools/validate_report.py` | engine | Every saved F full report validates against `schema/report.v7.schema.json` and passes the semantic check (verdict and loss, mate and score, parry test, urgency, legal lines, asked-move copy, threat skips, restrictions, tactic and opportunity relations); every saved move report document validates against `schema/moves.v7.schema.json` and passes the projection check against the full report of the same command; runs after all `cli_fixture_*` tests | `ReportSchema`, `SemanticCheck`, `ProjectionCheck`, `EngineFixtures` |
| `cli_rerun_identity` | `cli_identity_test` (`identity_test.cpp`) | engine | F01, F03, F12, F25, F29, F30: the full report JSON and the move report document are each byte-identical across two runs; so is the move report text for a fixed position (`--moves Nf6` after 3.Qh5) | `RerunIdentity`, `Reproducible`, `ReportDocument`, `JsonOutput`, `MoveReport` |

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
| positional P fixtures exercise every include and exclude | unit | P fixtures: every hand-labelled feature and change reported, every excluded one absent; PU fixtures: every kept net change passes the use test, every cut one fails it, every passing change is not a net change | `PositionalFixtures`, `PawnStructure`, `KingSafety`, `PieceActivity`, `SpaceDevelopment`, `MaterialBalance`, `PositionalChange`, `NetChange`, `UseTest` |
| positional fixture matching handles locally constructed rows | unit | The fixture matcher itself on rows built in the test | `PositionalFixtures` |
| early passer and rook-file changes pass within two plies | unit | A passed pawn or rook-file change made by the move or its reply passes the use test without later use | `UseTest`, `UtOwn` |
| favoured side follows change direction and weakness polarity | unit | An appearing weakness favours the owner's opponent, another appearing feature its owner, a disappearance the other side; pawn islands and chains favour nobody | `FavouredSide` |
| king pressure uses checks, zone captures, and distinct end attackers | unit | King pressed by a check or a capture next to the king from the change's ply on, or two distinct enemy pieces attacking the king zone at the end | `UtPressed`, `UtCastle`, `UtUsed` |
| static scale groups and dynamic tags follow the contracted tables | unit | Every positional tag's static-scale group and dynamic flag | `StaticScale` |
| positional impact order uses both mode orders and preserves ties | unit | Dynamic facts first in a tempo-dependent position, static facts first otherwise, then static-scale group; equal facts keep their order | `ImpactRating`, `StaticScale` |

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

## 13. Checker — `tests/checker/unit_tests.py` (`checker_unit`)

Python `unittest` cases on hand-built JSON, one CTest.

| Test | Label | Checks | Concepts |
|---|---|---|---|
| `checker_unit` | unit | The semantic check rejects a best move with a different score, a blunder that allows no tactic, tactic flags that disagree with the consequence, details on a non-detailed line, labels off tactic lines, a wrong opportunity flag at the margin, a selection out of typed order and a wrong `playable_truncated`; it derives `creates_threat_skipped` from the board on detailed lines only (a non-detailed checking move keeps null); one mutant per tempo relation (tempo value and label at the unit boundaries, ledger labels and tempi, initiative runs and their outcome) and per use-test rule (tags, timing, king pressed, superseded changes) is rejected, net change and `in_use` are re-derived independently; the projection check's independent filter drops non-contributing motifs and changes that are not net or fail the use test, orders the rest by impact order, and flags a move fact that differs from the full report | `SemanticCheck`, `ProjectionCheck`, `MoveSelection`, `NetChange`, `UseTest`, `TempoValue`, `TempoLedger`, `InitiativeRun`, `ImpactRating` |
