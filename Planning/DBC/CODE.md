# ChessWhy — Code map

Where each concept node of [CONCEPTS.md](CONCEPTS.md) lives in the source: files and
functions, classes or structs, never line numbers. Terms are defined in
[DEFINITIONS.md](DEFINITIONS.md); the tests and fixtures that check each node are listed in
[TESTS.md](TESTS.md) and [FIXTURES.md](FIXTURES.md).

Rules, checked by `python3 tools/check_dbc.py`:

- One row per node and place. The first backticked token of a Code cell is a path from the
  repository root; the names after it are functions, classes, structs or constants in that
  file. A cell may name several paths, each followed by its own names.
- Every `implemented` or `partial` L2 node has at least one row; L3 rows are optional.
  `planned` nodes have no rows, and L1 nodes never do: their L2 nodes carry the code.
- External nodes are listed only under the diagram that owns them.

## Question

| Node | Code | Notes |
|---|---|---|
| `CommandLine` | `src/cli/cli.cpp` `parse_args` `run`; `include/chesswhy/cli.hpp` `Options` | `chesswhy`; `src/cli/main.cpp` only forwards |
| `FenInput` | `src/input/fen.cpp` `parse_fen` | Rejects illegal positions and out-of-range counters |
| `PgnInput` | `src/input/pgn.cpp` `load_pgn` `read_games` `PgnReader`; `include/chesswhy/input.hpp` `PgnPosition` `PgnGame` `PgnReader` `PgnReadOptions`; `src/pgntool/pgntool.cpp` `run` | `chesswhy-pgn` lists the games and plies of a file; `PgnReader` streams games one at a time and stops parsing at `max_plies` |
| `AskedMove` | `src/input/move.cpp` `parse_move` | SAN or UCI |
| `Settings` | `include/chesswhy/report.hpp` `Budget` `Level`; `src/cli/cli.cpp` `parse_args` | |
| `AnalysedPosition` | `include/chesswhy/analysis.hpp` `Request`; `include/chesswhy/report.hpp` `PositionInfo` | |
| `GameMainline` | `include/chesswhy/input.hpp` `PgnPosition` | Mainline boards up to the chosen ply |
| `InputError` | `include/chesswhy/input.hpp` `InputError`; `include/chesswhy/cli.hpp` `kExitInput` | |

## Engine searches

| Node | Code | Notes |
|---|---|---|
| `NodeBudget` | `src/analysis/rules.cpp` `node_budget`; `include/chesswhy/report.hpp` `NodeBudget` `EngineInfo` | Nodes per search kind and budget |
| `SearchA` | `src/analysis/analyse.cpp` `analyse`; `include/chesswhy/analysis.hpp` `kMultiPv` | |
| `SearchB` | `src/analysis/analyse.cpp` `analyse` | Null move on a copy of the board |
| `SearchC` | `src/analysis/analyse.cpp` `analyse` | Only when the asked move is not among A's lines |
| `SearchD` | `src/analysis/analyse.cpp` `search_parry` | |
| `SearchE` | `src/analysis/analyse.cpp` `assess_move` | |
| `SearchR` | `src/analysis/analyse.cpp` `judge_refutation` | |
| `StockfishProcess` | `src/engine/engine.cpp` `Engine` `search`; `include/chesswhy/engine.hpp` `SearchRequest` `SearchResult` `Line` `MultiPvCollector` `EngineError`; `src/engine/info_line.cpp` `parse_info_line` | Talks UCI; never linked |
| `ExpectedScore` | `src/analysis/rules.cpp` `es_units`; `src/analysis/analyse.cpp` `eval_for`; `include/chesswhy/report.hpp` `Eval` | |

## Threats and urgency

| Node | Code | Notes |
|---|---|---|
| `SideThreat` | `src/analysis/analyse.cpp` `make_threat` `analyse`; `include/chesswhy/report.hpp` `Threat` `Threats` | |
| `OpponentThreat` | `src/analysis/analyse.cpp` `make_threat` `analyse` | |
| `NullMoveReliable` | `src/analysis/rules.cpp` `null_move_prone` | |
| `WalkedPlies` | `src/analysis/rules.cpp` `walked_plies`; `src/analysis/analyse.cpp` `walked_moves`; `include/chesswhy/analysis.hpp` `kHorizonPlies` `kLinePlies` | |
| `Consequence` | `src/analysis/rules.cpp` `consequence`; `include/chesswhy/report.hpp` `Consequence` `ConsequenceClass` | |
| `ConsequenceRank` | `src/analysis/rules.cpp` `consequence_rank` | |
| `Urgency` | `src/analysis/analyse.cpp` `analyse`; `include/chesswhy/report.hpp` `Urgency` `UrgencyRule` | The rules sit at the end of `analyse` |

## Move verdicts

| Node | Code | Notes |
|---|---|---|
| `CandidateMove` | `src/analysis/analyse.cpp` `assess_move`; `src/analysis/rules.cpp` `walked_plies`; `include/chesswhy/report.hpp` `MoveAssessment` | `depth`, `walked_plies` |
| `Loss` | `src/analysis/analyse.cpp` `assess_move` | |
| `Verdict` | `src/analysis/rules.cpp` `verdict_for_loss`; `include/chesswhy/analysis.hpp` `kFineUnits` `kMistakeUnits` `kBlunderUnits`; `include/chesswhy/report.hpp` `Verdict` | |
| `PlayableSet` | `src/analysis/analyse.cpp` `analyse` `legal_move_count`; `include/chesswhy/report.hpp` `Report` `PositionInfo` | `fine_truncated` (becomes `playable_truncated`), `legal_moves` |
| `ParryTest` | `src/analysis/analyse.cpp` `search_parry` `pseudo_legal_uci`; `include/chesswhy/report.hpp` `ParryInfo` `ParryTest` `parry_source` | |
| `CreatedThreat` | `src/analysis/analyse.cpp` `assess_move` `has_legal_moves`; `include/chesswhy/report.hpp` `CreatesThreatSkip` | `creates_threat_skipped` |
| `Refutation` | `src/analysis/analyse.cpp` `assess_move` `make_threat` | |

## Motifs

| Node | Code | Notes |
|---|---|---|
| `BoardAtPly` | `src/tactics/motifs.cpp` `line_motifs` | |
| `StaticExchange` | `src/tactics/see.cpp` `see` `piece_value`; `src/tactics/see_internal.hpp` `see_without_piece` | |
| `StateMotif` | `src/tactics/motifs.cpp` `state_motifs`; `include/chesswhy/report.hpp` `Motif` `MotifTag` | |
| `MoveMotif` | `src/tactics/motifs.cpp` `append_move_motifs` `append_fork` `append_skewers` `append_discovered_attacks` `append_back_rank_mates` | |
| `TacticalPin` | `src/tactics/motifs.cpp` `append_pin_tactical` `append_restricted_motif`; `include/chesswhy/tactics.hpp` `MotifOptions` | |
| `Restriction` | `src/tactics/motifs.cpp` `make_restriction` `held_to` `append_restricted_motif`; `src/tactics/tactic.cpp` `denied_moves` `detail::held_piece_move_set` `detail::MoveSetLegality`; `include/chesswhy/report.hpp` `MotifRestriction` `HeldTo` | Existence condition: motif dropped when `make_restriction` returns none. The move set adds a pawn's hypothetical captures |
| `FirstAppearance` | `src/tactics/motifs.cpp` `line_motifs` `same_identity` | |
| `PositionMotifs` | `src/tactics/motifs.cpp` `position_motifs` `motif_less` | |
| `LineMotifs` | `src/tactics/motifs.cpp` `line_motifs` | |

## Tactics

| Node | Code | Notes |
|---|---|---|
| `JudgedLine` | `src/analysis/analyse.cpp` `analyse` `judge_refutation` | The best line is judged in `analyse` |
| `BestAlternative` | `src/analysis/analyse.cpp` `judge_refutation`; `include/chesswhy/report.hpp` `TacticAlternative` | |
| `OnlyMoveMargin` | `src/analysis/rules.cpp` `is_tactic_margin`; `include/chesswhy/analysis.hpp` `kTacticMarginUnits` | |
| `OpportunityJudgement` | `src/analysis/analyse.cpp` `judge_refutation` `legal_move_count`; `include/chesswhy/report.hpp` `TacticJudgement` | |
| `TacticLine` | `src/analysis/analyse.cpp` `analyse` `judge_refutation` | |
| `TacticSequence` | `src/analysis/analyse.cpp` `walked_moves` | |
| `Contribution` | `src/tactics/tactic.cpp` `judge_contributions` `denied_moves` `answers`; `include/chesswhy/tactics.hpp` `denied_moves` `answers` | |
| `TacticLabel` | `src/tactics/tactic.cpp` `tactic_labels`; `include/chesswhy/report.hpp` `TacticLabel` | |

## Positional facts

| Node | Code | Notes |
|---|---|---|
| `PawnStructure` | `src/positional/positional.cpp` `add_pawn_features` `features`; `src/analysis/analyse.cpp` `analyse` | `analyse` fills `position.positional` |
| `KingSafety` | `src/positional/positional.cpp` `add_king_features` `features` | |
| `PieceActivity` | `src/positional/positional.cpp` `add_piece_features` `features` | |
| `SpaceDevelopment` | `src/positional/positional.cpp` `mobility` `space` `undeveloped` `add_leads_and_material`; `include/chesswhy/positional.hpp` `kMobilityLead` `kSpaceLead` `kDevelopmentLead` | |
| `MaterialBalance` | `src/positional/positional.cpp` `add_leads_and_material` | |
| `PositionalChange` | `src/positional/positional.cpp` `same_identity` `line_changes`; `src/analysis/analyse.cpp` `assess_move` `walked_moves`; `include/chesswhy/report.hpp` `PositionalChange` | Over the same walked plies as the line motifs |
| `TiedToLine` | `src/render/render.cpp` `write_positional_changes` | Prints only changes along lines; `position.positional` stays in the JSON |

## Reference knowledge

| Node | Code | Notes |
|---|---|---|
| `PositionKey` | `src/openings/openings.cpp` `position_key` | |
| `OpeningNames` | `src/openings/openings.cpp` `Table` `describe`; `include/chesswhy/openings.hpp` `kDataset`; `include/chesswhy/report.hpp` `OpeningInfo` | Table file fetched by `tools/fetch_assets.sh` |
| `LastNamed` | `src/openings/openings.cpp` `describe` | |
| `BroadcastMonths` | `tools/fetch_assets.sh` `fetch_games` | Months and their SHA-256 pinned in the script |
| `GameTable` | `src/games/games.cpp` `build_table` `Table`; `src/games/main.cpp` `parse_options`; `include/chesswhy/games.hpp` `kTable` `kRatingFloor` `kMaxPly` `kMinGames` | `chesswhy-games` builds the table from PGN on stdin |
| `GameCounts` | `src/games/games.cpp` `Table`; `src/cli/cli.cpp` `run`; `include/chesswhy/report.hpp` `GamesInfo` `GameSourceCounts` `GameMoveCount`; `src/render/render.cpp` `write_games_summary` `write_game_move_counts` | `run` loads the table once (`--games`) and looks up the analysed position |
| `DataDigest` | `src/util/sha256.cpp` `sha256_hex`; `include/chesswhy/digest.hpp`; `src/games/games.cpp` `Table::load` `table_name`; `include/chesswhy/games.hpp` `kTableSha256`; `src/openings/openings.cpp` `Table::load`; `include/chesswhy/openings.hpp` `kDatasetSha256` | |

## Report

| Node | Code | Notes |
|---|---|---|
| `Fact` | `include/chesswhy/report.hpp` `Report` | Each field of the report is one fact |
| `Provenance` | `src/report/report.cpp` `to_json`; `include/chesswhy/report.hpp` `parry_source` | Written as a `source` field beside each fact |
| `Reproducible` | `src/cli/cli.cpp` `run`; `include/chesswhy/report.hpp` `EngineInfo` | |
| `ReportDocument` | `include/chesswhy/report.hpp` `Report` `to_json`; `src/report/report.cpp` `to_json` | |
| `ReportSchema` | `schema/report.v5.schema.json`; `include/chesswhy/report.hpp` `kSchemaVersion`; `tools/validate_report.py` | |
| `SemanticCheck` | `tools/validate_report.py` `check_all_semantics` `check_score_relations` `check_parry_relations` `check_threat_skip` `check_restrictions_everywhere` `check_tactic_invariants` | Run by CTest `report_schema` on every F report |
| `RerunIdentity` | `tests/cli/identity_test.cpp` | |
| `JsonOutput` | `src/cli/cli.cpp` `run` | `--json` |

## Explanation

| Node | Code | Notes |
|---|---|---|
| `RestrictedMovement` | `src/render/render.cpp` `write_motif_label` `write_restriction` `write_held_to` | |
| `Conditions` | `src/render/render.cpp` `write_candidate_count` `write_walk` `creates_threat_skip_reason` | |
| `TemplateRendering` | `src/render/render.cpp` `text` | `--no-llm`; today the only renderer |
| `ExplanationText` | `src/cli/cli.cpp` `run` | Prints the rendered text |

## Fixtures and measurement

| Node | Code | Notes |
|---|---|---|
| `HandLabel` | `tests/fixtures/motifs.json`; `tests/fixtures/tactics.json` | |
| `EngineFixtures` | `tests/fixtures/positions.json`; `tests/cli/engine_fixture_test.cpp` | |
| `InputFixtures` | `tests/openings/openings_unit_tests.cpp`; `tests/input/input_unit_tests.cpp`; `tests/pgntool/pgntool_unit_tests.cpp`; `tests/fixtures/two_games.pgn` | |
| `MotifFixtures` | `tests/fixtures/motifs.json`; `tests/tactics/tactics_unit_tests.cpp` | |
| `ContributionFixtures` | `tests/fixtures/tactics.json`; `tests/tactics/tactics_unit_tests.cpp` | |
| `ReferenceImplementation` | `tests/tactics/oracle/reference.cpp`; `tests/tactics/motif_oracle.cpp` | |
| `PuzzleSample` | `tools/sample_puzzles.py`; `tests/fixtures/puzzles/` | |
| `Recall` | `tests/tactics/puzzle_eval.cpp` | Prints recall and the untagged firing rate |
| `PositionalFixtures` | `tests/fixtures/positional.json`; `tests/positional/positional_unit_tests.cpp`; `tests/cli/engine_fixture_test.cpp` | F rows: `positional_include` |
| `PositionalReference` | `tests/positional/oracle/reference.cpp`; `tests/positional/oracle/positional_oracle.cpp`; `tests/positional/oracle/reference_self_check.cpp` | |
| `GamesOracle` | `tools/count_games.py` `GameCounter` `write_table`; `tests/games/oracle.cmake` | python-chess, development only |
