# Owned by WP-A2 (Planning/milestones/M3a.md §11): positional_oracle (label unit), an
# independent reference of M3a §4-§5 compared against chesswhy_positional.

add_executable(positional_reference_self_check reference_self_check.cpp reference.cpp)
target_include_directories(positional_reference_self_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(positional_reference_self_check PRIVATE chesswhy_headers chesswhy_warnings)
add_test(NAME positional_reference_self_check COMMAND positional_reference_self_check)
set_tests_properties(positional_reference_self_check PROPERTIES LABELS unit)

if(TARGET chesswhy_positional)
    add_executable(positional_oracle positional_oracle.cpp reference.cpp)
    target_include_directories(positional_oracle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(positional_oracle PRIVATE chesswhy_positional chesswhy_warnings)
    add_test(NAME positional_oracle
        COMMAND positional_oracle "${PROJECT_SOURCE_DIR}/tests/fixtures/puzzles/test.csv")
    set_tests_properties(positional_oracle PROPERTIES LABELS unit)
endif()
