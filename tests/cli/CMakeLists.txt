add_executable(cli_unit_tests unit_tests.cpp)
target_link_libraries(cli_unit_tests PRIVATE
    chesswhy_cli
    Catch2::Catch2WithMain
    chesswhy_warnings
    chesswhy_paths)
catch_discover_tests(cli_unit_tests PROPERTIES LABELS unit)

add_executable(cli_fixture_test engine_fixture_test.cpp)
target_link_libraries(cli_fixture_test PRIVATE chesswhy_cli chesswhy_warnings chesswhy_paths)
target_compile_definitions(cli_fixture_test PRIVATE
    CHESSWHY_CLI_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/reports")

add_executable(cli_identity_test identity_test.cpp)
target_link_libraries(cli_identity_test PRIVATE chesswhy_cli chesswhy_warnings chesswhy_paths)

set(CLI_FIXTURES_FILE "${PROJECT_SOURCE_DIR}/tests/fixtures/positions.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CLI_FIXTURES_FILE}")
file(READ "${CLI_FIXTURES_FILE}" CLI_FIXTURES_JSON)
string(JSON CLI_FIXTURE_COUNT LENGTH "${CLI_FIXTURES_JSON}" engine_fixtures)
math(EXPR CLI_FIXTURE_LAST_INDEX "${CLI_FIXTURE_COUNT} - 1")
set(CLI_REPORT_FILES)
set(CLI_REPORT_FIXTURES)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reports")
foreach(index RANGE 0 ${CLI_FIXTURE_LAST_INDEX})
    string(JSON fixture_id GET "${CLI_FIXTURES_JSON}" engine_fixtures ${index} id)
    add_test(NAME cli_fixture_${fixture_id}
        COMMAND cli_fixture_test "${fixture_id}" "$<BOOL:${CHESSWHY_CHECK_TIMING}>")
    set_tests_properties(cli_fixture_${fixture_id} PROPERTIES
        LABELS engine
        FIXTURES_SETUP cli_report_${fixture_id})
    list(APPEND CLI_REPORT_FILES "${CMAKE_CURRENT_BINARY_DIR}/reports/${fixture_id}.json")
    list(APPEND CLI_REPORT_FIXTURES cli_report_${fixture_id})
endforeach()

add_test(NAME report_schema
    COMMAND "${CHESSWHY_PYTHON}"
        "${PROJECT_SOURCE_DIR}/tools/validate_report.py"
        "${PROJECT_SOURCE_DIR}/schema/report.v1.schema.json"
        ${CLI_REPORT_FILES})
set_tests_properties(report_schema PROPERTIES
    LABELS engine
    FIXTURES_REQUIRED "${CLI_REPORT_FIXTURES}")

add_test(NAME cli_rerun_identity COMMAND cli_identity_test)
set_tests_properties(cli_rerun_identity PROPERTIES LABELS engine)
