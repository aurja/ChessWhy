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
| `CommandLine` | `src/cli/cli.cpp` `parse_args` `build_report` `run`; `include/chesswhy/cli.hpp` `Options` | `chesswhy`; `src/cli/main.cpp` only forwards; `run` is `build_report` then `write_output` with exit codes |
| `FenInput` | `src/input/fen.cpp` `parse_fen` | Rejects illegal positions and out-of-range counters |
| `PgnInput` | `src/input/pgn.cpp` `load_pgn` `read_games` `PgnReader`; `include/chesswhy/input.hpp` `PgnPosition` `PgnGame` `PgnReader` `PgnReadOptions`; `src/pgntool/pgntool.cpp` `run` | `chesswhy-pgn` lists the games and plies of a file; `PgnReader` streams games one at a time and stops parsing at `max_plies` |
| `AskedMove` | `src/input/move.cpp` `parse_move`; `src/cli/selection.cpp` `resolve_moves` | SAN or UCI; several moves, deduplicated by UCI, in typed order |
| `MoveRequest` | `src/cli/selection.cpp` `resolve_moves` `parse_top`; `src/cli/selection.hpp` `ResolvedMoves`; `include/chesswhy/report.hpp` `Selection`; `src/cli/cli.cpp` `parse_args` `usage` | `--moves` grammar; the help warns that whole-board keywords are slow |
| `Settings` | `include/chesswhy/report.hpp` `Budget` `Level`; `src/cli/cli.cpp` `parse_args` | |
| `AnalysedPosition` | `include/chesswhy/analysis.hpp` `Request`; `include/chesswhy/report.hpp` `PositionInfo` | |
| `GameMainline` | `include/chesswhy/input.hpp` `PgnPosition` | Mainline boards up to the chosen ply |
| `InputError` | `include/chesswhy/input.hpp` `InputError`; `include/chesswhy/cli.hpp` `kExitInput` | |

## Engine searches

| Node | Code | Notes |
|---|---|---|
| `NodeBudget` | `src/analysis/rules.cpp` `node_budget`; `include/chesswhy/report.hpp` `NodeBudget` `EngineInfo` | Nodes per search kind and budget |
| `SearchA` | `src/analysis/analyse.cpp` `analyse`; `src/analysis/rules.cpp` `multipv_lines` `judges_every_move` `search_a_nodes`; `include/chesswhy/analysis.hpp` `kMinMultiPv` | K from the selection; each line keeps its share of A's nodes |
| `SearchB` | `src/analysis/analyse.cpp` `analyse` | Null move on a copy of the board |
| `SearchC` | `src/analysis/analyse.cpp` `analyse` | Once per asked move not among A's lines, in typed order |
| `SearchD` | `src/analysis/analyse.cpp` `search_parry`; `src/analysis/rules.cpp` `is_detailed_line` | Detailed lines only |
| `SearchE` | `src/analysis/analyse.cpp` `assess_move`; `src/analysis/rules.cpp` `is_detailed_line` | Detailed lines only |
| `SearchR` | `src/analysis/analyse.cpp` `judge_refutation`; `src/analysis/rules.cpp` `is_detailed_line` | Detailed lines only |
| `ThreatSearch` | `src/analysis/analyse.cpp` `complete_tempo_ledger`; `src/analysis/rules.cpp` `node_budget` | Budget `L`; ply 1 reads E's result |
| `ReplySearch` | `src/analysis/analyse.cpp` `complete_tempo_ledger`; `src/analysis/rules.cpp` `node_budget` | Budget `L`; after a threatening ply and after each acceptable alternative of the forced test |
| `ForcedSearch` | `src/analysis/analyse.cpp` `complete_tempo_ledger` `legal_move_count`; `include/chesswhy/report.hpp` `ForcedTest` `ReplyLine` | MultiPV `min(5, legal)` at `L`, only on a ply whose reply gain is false |
| `StockfishProcess` | `src/engine/engine.cpp` `Engine` `search`; `include/chesswhy/engine.hpp` `SearchRequest` `SearchResult` `Line` `MultiPvCollector` `EngineError`; `src/engine/info_line.cpp` `parse_info_line` | Talks UCI; never linked |
| `ExpectedScore` | `src/analysis/rules.cpp` `es_units`; `src/analysis/analyse.cpp` `eval_for`; `include/chesswhy/report.hpp` `Eval` | |

## Threats and urgency

| Node | Code | Notes |
|---|---|---|
| `SideThreat` | `src/analysis/analyse.cpp` `make_threat` `analyse`; `include/chesswhy/report.hpp` `Threat` `Threats` | |
| `OpponentThreat` | `src/analysis/analyse.cpp` `make_threat` `analyse` | |
| `NullMoveReliable` | `src/analysis/rules.cpp` `null_move_prone` | |
| `WalkedPlies` | `src/analysis/rules.cpp` `walk` `continuation_move` `game_over`; `src/analysis/analyse.cpp` `walked_moves`; `include/chesswhy/analysis.hpp` `Walk` `kHorizonPlies` `kLinePlies` | Reads the raw PV for the 11th-move settled test |
| `Consequence` | `src/analysis/rules.cpp` `consequence`; `include/chesswhy/report.hpp` `Consequence` `ConsequenceClass` | `settled` and `terminal_draw` come from `walk` |
| `TerminalDraw` | `src/analysis/rules.cpp` `walk` `insufficient_material` `game_over`; `include/chesswhy/report.hpp` `TerminalDraw` `DrawKind` | Insufficient material as python-chess, not `chess.hpp`'s |
| `ConsequenceRank` | `src/analysis/rules.cpp` `consequence_rank` | |
| `Urgency` | `src/analysis/analyse.cpp` `analyse`; `include/chesswhy/report.hpp` `Urgency` `UrgencyRule` | The rules sit at the end of `analyse` |

## Move verdicts

| Node | Code | Notes |
|---|---|---|
| `CandidateMove` | `src/analysis/analyse.cpp` `assess_move`; `src/analysis/rules.cpp` `walk` `is_detailed_line`; `include/chesswhy/report.hpp` `MoveAssessment` | `depth`, `walked_plies`, `detailed` |
| `Loss` | `src/analysis/analyse.cpp` `assess_move` | |
| `AllowsTactic` | `src/analysis/rules.cpp` `allows_tactic` `consequence`; `src/analysis/analyse.cpp` `assess_move` | `allows_tactic`, read along the move's own line from the analysed position |
| `Verdict` | `src/analysis/rules.cpp` `verdict` `same_score`; `include/chesswhy/analysis.hpp` `kPlayableUnits` `kInaccuracyUnits`; `include/chesswhy/report.hpp` `Verdict` | |
| `PlayableSet` | `src/analysis/analyse.cpp` `analyse` `legal_move_count`; `include/chesswhy/report.hpp` `Report` `PositionInfo` | `playable_truncated`, `legal_moves` |
| `ParryTest` | `src/analysis/analyse.cpp` `search_parry` `pseudo_legal_uci`; `include/chesswhy/report.hpp` `ParryInfo` `ParryTest` `parry_source` | |
| `CreatedThreat` | `src/analysis/analyse.cpp` `assess_move` `has_legal_moves`; `include/chesswhy/report.hpp` `CreatesThreatSkip` | `creates_threat_skipped` |
| `Refutation` | `src/analysis/analyse.cpp` `assess_move` `make_threat` `judge_refutation` `complete_move_details`; `include/chesswhy/report.hpp` `RefutationSkip` | `refutation_skipped` after a game-over move; search R skipped |

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
| `BestAlternative` | `src/analysis/analyse.cpp` `judge_refutation`; `include/chesswhy/report.hpp` `BestAlternative` | |
| `OnlyMoveMargin` | `src/analysis/rules.cpp` `is_opportunity_margin`; `include/chesswhy/analysis.hpp` `kOpportunityMarginUnits` | |
| `OpportunityJudgement` | `src/analysis/analyse.cpp` `judge_refutation` `legal_move_count`; `include/chesswhy/report.hpp` `OpportunityJudgement` | |
| `TacticTest` | `src/analysis/rules.cpp` `is_tactic` `consequence`; `src/analysis/analyse.cpp` `assess_move` | `is_tactic` on every move line and refutation |
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
| `TiedToLine` | `src/report/move_report.cpp` `assess_move`; `src/render/render.cpp` `write_positional_changes` `write_positional_change` `write_move_positional_group` | Move reports keep `net_change && in_use`; the full report text marks passing changes and changes that fail the use test |
| `NetChange` | `src/positional/positional.cpp` `mark_use` `contains_identity`; `include/chesswhy/report.hpp` `PositionalChange` | `mark_use` computes the root features itself |
| `UseTest` | `src/positional/positional.cpp` `mark_use` `king_pressed` `king_file_pressed` `king_zone` `used_by_later_capture` `has_attacked_feature_pawn` `used_passer` `used_rook_file` | Called by `src/analysis/analyse.cpp` `analyse` before the ledger; `king_file_pressed` is the shelter witness |
| `FavouredSide` | `src/positional/positional.cpp` `favours` `line_changes`; `include/chesswhy/report.hpp` `PositionalChange` | `line_changes` sets `captured`; `favours` gives no side for a captured weakness |
| `StaticScale` | `src/positional/positional.cpp` `static_scale_group` `is_dynamic`; `include/chesswhy/positional.hpp` `StaticScaleGroup` | |

## Tempo and initiative

| Node | Code | Notes |
|---|---|---|
| `TempoValue` | `src/analysis/analyse.cpp` `make_tempo_value` `complete_move_details` `analyse`; `include/chesswhy/report.hpp` `TempoValue` | Root from A and B, after a move from its eval and E |
| `TempoLabel` | `src/analysis/rules.cpp` `tempo_label` `tempo_dependent`; `include/chesswhy/analysis.hpp` `kTempoDependentUnits` `kNonTempoUnits` | |
| `ThreatTest` | `src/analysis/analyse.cpp` `complete_tempo_ledger` | |
| `ReplyTest` | `src/analysis/analyse.cpp` `complete_tempo_ledger` | |
| `SettlePoint` | `src/analysis/rules.cpp` `settle_ply` `continuation_move` | Settled when `j < n` or the walk is settled |
| `ReplyGain` | `src/analysis/rules.cpp` `reply_gain` `board_positions` `settle_ply` `material_balance`; `src/analysis/analyse.cpp` `complete_tempo_ledger`; `include/chesswhy/report.hpp` `ReplyGain` | `board_positions` holds every walked board, since the settle point can lie past the ply being labelled |
| `ForcedTest` | `src/analysis/rules.cpp` `forced_test_result`; `src/analysis/analyse.cpp` `complete_tempo_ledger`; `include/chesswhy/report.hpp` `ForcedTest` `ReplyLine` | Acceptable: within 100 units of line 1; `m` is the number of lines Stockfish returns |
| `PlyLabel` | `src/analysis/rules.cpp` `label_ledger`; `include/chesswhy/report.hpp` `TempoPly` `TempoPlyLabel` | Reads `reply_gain` and `forced_test` |
| `TempoGained` | `src/analysis/rules.cpp` `label_ledger` | |
| `TempoLedger` | `src/analysis/analyse.cpp` `complete_tempo_ledger`; `include/chesswhy/report.hpp` `TempoLedger` `TempoLedgerSkip` `Tempi` | |
| `InitiativeRun` | `src/analysis/rules.cpp` `initiative_runs`; `include/chesswhy/report.hpp` `InitiativeRun` | Precondition: plies labelled by `label_ledger` |
| `RunEnding` | `src/analysis/rules.cpp` `initiative_runs`; `include/chesswhy/report.hpp` `InitiativeEnding` | |
| `RunOutcome` | `src/analysis/rules.cpp` `initiative_runs` `material_balance` `settle_ply`; `include/chesswhy/report.hpp` `InitiativeOutcome` | Reads `positional::favours` and `positional::is_dynamic` |

## Reference knowledge

| Node | Code | Notes |
|---|---|---|
| `PositionKey` | `src/openings/openings.cpp` `position_key` | |
| `OpeningNames` | `src/openings/openings.cpp` `Table` `describe`; `include/chesswhy/openings.hpp` `kDataset`; `include/chesswhy/report.hpp` `OpeningInfo` | Table file fetched by `tools/fetch_assets.sh` |
| `LastNamed` | `src/openings/openings.cpp` `describe` | |
| `BroadcastMonths` | `tools/fetch_assets.sh` `fetch_games` | Months and their SHA-256 pinned in the script |
| `GameTable` | `src/games/games.cpp` `build_table` `Table`; `src/games/main.cpp` `parse_options`; `include/chesswhy/games.hpp` `kTable` `kRatingFloor` `kMaxPly` `kMinGames` | `chesswhy-games` builds the table from PGN on stdin |
| `GameCounts` | `src/games/games.cpp` `Table`; `src/cli/cli.cpp` `build_report`; `include/chesswhy/report.hpp` `GamesInfo` `GameSourceCounts` `GameMoveCount`; `src/render/render.cpp` `write_games_summary` `write_game_move_counts` | `build_report` loads the table once (`--games`) and looks up the analysed position |
| `DataDigest` | `src/util/sha256.cpp` `sha256_hex`; `include/chesswhy/digest.hpp`; `src/games/games.cpp` `Table::load` `table_name`; `include/chesswhy/games.hpp` `kTableSha256`; `src/openings/openings.cpp` `Table::load`; `include/chesswhy/openings.hpp` `kDatasetSha256` | |

## Report

| Node | Code | Notes |
|---|---|---|
| `Fact` | `include/chesswhy/report.hpp` `Report` | Each field of the report is one fact |
| `Provenance` | `src/report/report.cpp` `to_json`; `include/chesswhy/report.hpp` `parry_source` | Written as a `source` field beside each fact |
| `Reproducible` | `src/cli/cli.cpp` `build_report`; `include/chesswhy/report.hpp` `EngineInfo` | |
| `ReportDocument` | `include/chesswhy/report.hpp` `Report` `to_json`; `src/report/report.cpp` `to_json` | |
| `ReportSchema` | `schema/report.v9.schema.json`; `include/chesswhy/report.hpp` `kSchemaVersion`; `tools/validate_report.py` | |
| `SemanticCheck` | `tools/validate_report.py` `check_all_semantics` `check_score_relations` `check_parry_relations` `check_threat_skip` `check_refutation_skip` `check_restrictions_everywhere` `check_tactic_invariants` `check_all_consequence_semantics` `check_line_consequence` `rule_game_over` `replay_walk` `check_tempo_and_positional_semantics` `check_tempo_value` `check_tempo_ledger` `settle_point` `expected_reply_gain` `check_forced_test` `expected_initiative_runs` `favoured_side` `king_pressed` `use_test` `check_positional_relations` | Run by CTest `report_schema` on every F report; P1–P11 re-implemented independently on python-chess, one mutant per rule in `tests/checker/unit_tests.py` |
| `RerunIdentity` | `tests/cli/identity_test.cpp` | |
| `JsonOutput` | `src/cli/output.cpp` `write_output`; `src/report/move_report.cpp` `to_json` | `--json` prints the move report document, `--json --full-report` the full report |
| `MoveSelection` | `src/report/move_report.cpp` `move_reports` `selected_from_a` `selected_by_keyword`; `src/analysis/analyse.cpp` `selected_by_selection` | Engine order, asked moves outside A last, each move once |
| `RelevanceFilter` | `src/report/move_report.cpp` `assess_move` `relevant_motifs` `projected_threat` `has_created_threat_consequence`; `include/chesswhy/report.hpp` `MotifLabel` `Motif::also` | Positional changes: `net_change && in_use`, ordered by `positional::impact_order`. Motif repeats merge inside `relevant_motifs` |
| `ImpactRating` | `src/report/move_report.cpp` `impact_order`; `src/positional/positional.cpp` `impact_order`; `include/chesswhy/move_report.hpp` `ImpactGroup`; `src/render/render.cpp` `write_move_report` | Groups for the move report; positional facts ordered within their group |
| `MoveReport` | `include/chesswhy/move_report.hpp` `MoveReport` `MoveReportDocument` `SelectionInfo` `PositionSummary`; `src/report/move_report.cpp` `move_reports`; `schema/moves.v9.schema.json` | |
| `ProjectionCheck` | `tools/validate_report.py` `projected_move_document` `project_assessment` `net_positional_changes` `positional_identity` `projected_tempo_gained` `check_projection` | Second, independent filter; `--projection FULL MOVES` |

## Explanation

| Node | Code | Notes |
|---|---|---|
| `RestrictedMovement` | `src/render/render.cpp` `write_motif_label` `write_motif_inner_label` `write_compact_restricted_motif_label` `write_restriction` `write_held_to` | Compact rows in the move report text: `write_motifs(…, compact_restricted = true)` |
| `Conditions` | `src/render/render.cpp` `write_engine_line_count` `write_selection_count` `write_walk` `write_walk_settlement` `write_move_refutation` `creates_threat_skip_reason` | Walk suffixes ` (stalemate)`, ` (insufficient material)`, ` (not settled)`; `Refuted by: none, the move ends the game` |
| `PositionWalk` | `src/render/render.cpp` `write_move_report` `write_position_block` `write_no_move_block` `write_root_evaluation` `write_line_evaluation` `write_white_evaluation` `write_position_move` `write_move_report_group` `write_move_motifs` `write_move_tactic_labels` `write_move_positional_group` `write_tempo_value` `write_initiative_run`; `src/report/move_report.cpp` `replay_line_fens`; `tools/validate_report.py` (line_fens replay in the projection check) | `walked_plies` + 1 position blocks per move; facts placed by their position; tempo rows in the `ImpactGroup::Tempo` case of `write_move_report_group` (from 2026-10-25) |
| `TemplateRendering` | `src/render/render.cpp` `text` `write_move_report` `write_selection` `write_move_details` `write_tempo_value` `write_tempo_ledger` `write_initiative_runs` | Two overloads of `text`: move report text and full report text |
| `ExplanationText` | `src/cli/output.cpp` `write_output` | Prints the rendered text |

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
