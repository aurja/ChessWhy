add_executable(report_unit_tests report_unit_tests.cpp)
target_link_libraries(report_unit_tests PRIVATE chesswhy_report Catch2::Catch2WithMain
    chesswhy_warnings chesswhy_paths)
catch_discover_tests(report_unit_tests PROPERTIES LABELS unit)

add_executable(report_schema_examples write_examples.cpp)
target_link_libraries(report_schema_examples PRIVATE chesswhy_report chesswhy_warnings chesswhy_paths)

set(report_examples_dir "${CMAKE_CURRENT_BINARY_DIR}/schema_examples")
add_test(NAME report_schema_examples
    COMMAND $<TARGET_FILE:report_schema_examples> "${report_examples_dir}")
set_tests_properties(report_schema_examples PROPERTIES
    FIXTURES_SETUP report_schema_examples LABELS unit)

add_test(NAME report_schema_unit
    COMMAND "${CHESSWHY_PYTHON}" "${PROJECT_SOURCE_DIR}/tools/validate_report.py"
        "${PROJECT_SOURCE_DIR}/schema/report.v1.schema.json"
        "${report_examples_dir}/terminal_checkmate.json"
        "${report_examples_dir}/side_in_check.json"
        "${report_examples_dir}/normal_position.json"
        "${report_examples_dir}/pgn_opening.json")
set_tests_properties(report_schema_unit PROPERTIES
    FIXTURES_REQUIRED report_schema_examples LABELS unit)
