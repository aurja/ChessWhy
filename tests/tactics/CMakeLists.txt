# Tactics tests (Planning/milestones/M2.md §7.3, §7.4, §9, §11).

# SEE and detector unit tests, T fixtures from tests/fixtures/motifs.json.
add_executable(tactics_unit_tests tactics_unit_tests.cpp)
target_link_libraries(tactics_unit_tests PRIVATE chesswhy_tactics chesswhy_report
    Catch2::Catch2WithMain chesswhy_warnings chesswhy_paths)
catch_discover_tests(tactics_unit_tests PROPERTIES LABELS unit)

# Puzzle evaluator (recall, Lichess agreement). Usage:
# tactics_puzzle_eval <csv> [--disagreements N <out.tsv>].
# Exit 0 iff every measured tag meets its frozen recall target (M2 §8.2); before the
# targets are frozen it only prints the table and exits 0.
add_executable(tactics_puzzle_eval puzzle_eval.cpp)
target_link_libraries(tactics_puzzle_eval PRIVATE chesswhy_tactics chesswhy_report
    chesswhy_warnings)
add_test(NAME tactics_puzzles
    COMMAND tactics_puzzle_eval "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv")
set_tests_properties(tactics_puzzles PROPERTIES LABELS unit)

# Engine confirmation (precision, M2 §7.4). Usage:
# tactics_motif_confirm <csv> [--per-tag N] [--jobs J] [--details <out.tsv>].
# Exit 0 iff every claim tag meets its frozen confirmed-precision target (M2 §8.2);
# before the targets are frozen it only prints the table and exits 0.
add_executable(tactics_motif_confirm motif_confirm.cpp)
target_link_libraries(tactics_motif_confirm PRIVATE chesswhy_analysis chesswhy_engine
    chesswhy_tactics chesswhy_report chesswhy_warnings chesswhy_paths)
add_test(NAME tactics_confirm
    COMMAND tactics_motif_confirm "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv"
    --jobs 4)
set_tests_properties(tactics_confirm PROPERTIES LABELS engine)
