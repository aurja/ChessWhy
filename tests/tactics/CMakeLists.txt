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
    chesswhy_warnings chesswhy_test_progress)
add_test(NAME tactics_puzzles
    COMMAND tactics_puzzle_eval "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv")
set_tests_properties(tactics_puzzles PROPERTIES LABELS unit)

# Reference implementation of the motif definitions (motif recognition, M2 §7.4). Usage:
# tactics_motif_oracle <csv> [--fixtures <motifs.json>] [--mismatches N <out.tsv>].
# Exit 0 iff the detectors and the reference agree on every compared list.
add_executable(tactics_motif_oracle motif_oracle.cpp oracle/reference.cpp)
target_include_directories(tactics_motif_oracle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tactics_motif_oracle PRIVATE chesswhy_tactics chesswhy_report
    chesswhy_warnings chesswhy_test_progress)
add_test(NAME tactics_oracle
    COMMAND tactics_motif_oracle "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv"
    --fixtures "${PROJECT_SOURCE_DIR}/tests/fixtures/motifs.json")
# pin_tactical (PostM2_EM1 §4.5) makes the ASan/UBSan run take about 25 min.
set_tests_properties(tactics_oracle PROPERTIES LABELS unit TIMEOUT 3600)
