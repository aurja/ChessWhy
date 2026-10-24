# Positional oracle against the independent M3a §4-§5 reference. The unit test samples every 31st
# puzzle across the CSV; the corpus test checks every puzzle.

add_executable(positional_reference_self_check reference_self_check.cpp reference.cpp)
target_include_directories(positional_reference_self_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(positional_reference_self_check PRIVATE chesswhy_headers chesswhy_warnings)
add_test(NAME positional_reference_self_check COMMAND positional_reference_self_check)
set_tests_properties(positional_reference_self_check PROPERTIES LABELS unit)

if(TARGET chesswhy_positional)
    add_executable(positional_oracle positional_oracle.cpp reference.cpp)
    target_include_directories(positional_oracle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(positional_oracle PRIVATE chesswhy_positional chesswhy_warnings
        chesswhy_test_progress)
    add_test(NAME positional_oracle
        COMMAND positional_oracle
            "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv"
            "${PROJECT_SOURCE_DIR}/tests/fixtures/positional.json"
            --stride 31)
    set_tests_properties(positional_oracle PROPERTIES LABELS unit)

    add_test(NAME positional_oracle_corpus
        COMMAND positional_oracle
            "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv"
            "${PROJECT_SOURCE_DIR}/tests/fixtures/positional.json")
    set_tests_properties(positional_oracle_corpus PROPERTIES LABELS corpus TIMEOUT 3600)
endif()
