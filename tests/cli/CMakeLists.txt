add_executable(cli_unit_tests unit_tests.cpp)
target_include_directories(cli_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/cli")
target_link_libraries(cli_unit_tests PRIVATE
    chesswhy_cli
    Catch2::Catch2WithMain
    chesswhy_warnings
    chesswhy_paths)
catch_discover_tests(cli_unit_tests PROPERTIES LABELS unit)

add_executable(cli_fixture_test engine_fixture_test.cpp)
target_link_libraries(cli_fixture_test PRIVATE chesswhy_cli chesswhy_warnings chesswhy_paths
    chesswhy_test_progress)
target_compile_definitions(cli_fixture_test PRIVATE
    CHESSWHY_CLI_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/reports")

add_executable(cli_identity_test identity_test.cpp)
target_link_libraries(cli_identity_test PRIVATE chesswhy_cli chesswhy_warnings chesswhy_paths
    chesswhy_test_progress)

set(CLI_FIXTURES_FILE "${PROJECT_SOURCE_DIR}/tests/fixtures/positions.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CLI_FIXTURES_FILE}")
file(READ "${CLI_FIXTURES_FILE}" CLI_FIXTURES_JSON)
string(JSON CLI_FIXTURE_COUNT LENGTH "${CLI_FIXTURES_JSON}" engine_fixtures)
math(EXPR CLI_FIXTURE_LAST_INDEX "${CLI_FIXTURE_COUNT} - 1")
set(CLI_FULL_REPORT_FILES)
set(CLI_MOVES_REPORT_FILES)
set(CLI_REPORT_FIXTURES)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/reports")
foreach(index RANGE 0 ${CLI_FIXTURE_LAST_INDEX})
    string(JSON fixture_id GET "${CLI_FIXTURES_JSON}" engine_fixtures ${index} id)
    add_test(NAME cli_fixture_${fixture_id}
        COMMAND cli_fixture_test "${fixture_id}" "$<BOOL:${CHESSWHY_CHECK_TIMING}>")
    # TIMEOUT: F32 judges every legal move in two of its runs and exceeds CTest's
    # default 1500 s under ASan/UBSan (Debug); same limit as tactics_oracle.
    set_tests_properties(cli_fixture_${fixture_id} PROPERTIES
        LABELS engine
        TIMEOUT 3600
        FIXTURES_SETUP cli_report_${fixture_id})
    if(CHESSWHY_CHECK_TIMING)
        # Timed runs must not share cores; untimed reports do not depend on load
        # (Threads=1, fixed node budgets), so they run in parallel.
        set_tests_properties(cli_fixture_${fixture_id} PROPERTIES RESOURCE_LOCK engine)
    endif()
    set(report_prefix "${CMAKE_CURRENT_BINARY_DIR}/reports/${fixture_id}")
    list(APPEND CLI_FULL_REPORT_FILES "${report_prefix}.full.json")
    list(APPEND CLI_MOVES_REPORT_FILES "${report_prefix}.moves.json")
    string(JSON fixture_run_count ERROR_VARIABLE fixture_runs_error
        LENGTH "${CLI_FIXTURES_JSON}" engine_fixtures ${index} runs)
    if(NOT fixture_runs_error)
        math(EXPR fixture_run_last "${fixture_run_count} - 1")
        foreach(run_index RANGE 0 ${fixture_run_last})
            string(JSON run_name GET "${CLI_FIXTURES_JSON}" engine_fixtures ${index}
                runs ${run_index} name)
            set(run_report_prefix "${report_prefix}-${run_name}")
            list(APPEND CLI_FULL_REPORT_FILES "${run_report_prefix}.full.json")
            list(APPEND CLI_MOVES_REPORT_FILES "${run_report_prefix}.moves.json")
        endforeach()
    endif()
    list(APPEND CLI_REPORT_FIXTURES cli_report_${fixture_id})
endforeach()

set(CLI_REPORT_CHECK_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/report_schema.cmake")
file(WRITE "${CLI_REPORT_CHECK_SCRIPT}" "set(PYTHON [==[${CHESSWHY_PYTHON}]==])\n")
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}"
    "set(VALIDATOR [==[${PROJECT_SOURCE_DIR}/tools/validate_report.py]==])\n")
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}"
    "set(SCHEMA [==[${PROJECT_SOURCE_DIR}/schema/report.v7.schema.json]==])\n")
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" "set(FULL_REPORTS\n")
foreach(report IN LISTS CLI_FULL_REPORT_FILES)
    file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" "  [==[${report}]==]\n")
endforeach()
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" ")\nset(MOVES_REPORTS\n")
foreach(report IN LISTS CLI_MOVES_REPORT_FILES)
    file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" "  [==[${report}]==]\n")
endforeach()
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" ")\n")
file(APPEND "${CLI_REPORT_CHECK_SCRIPT}" [=[
string(TIMESTAMP progress_started "%s" UTC)
message("progress: report_schema 1/1 combined validation (0.0 s elapsed)")
execute_process(
    COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" ${FULL_REPORTS} ${MOVES_REPORTS}
    RESULT_VARIABLE documents_result)
if(NOT documents_result EQUAL 0)
    message(FATAL_ERROR "full or moves report validation failed")
endif()
list(LENGTH FULL_REPORTS report_count)
math(EXPR report_last "${report_count} - 1")
set(projection_index 0)
set(projection_elapsed_total 0)
foreach(index RANGE 0 ${report_last})
    math(EXPR projection_index "${projection_index} + 1")
    list(GET FULL_REPORTS ${index} full_report)
    list(GET MOVES_REPORTS ${index} moves_report)
    string(TIMESTAMP projection_started "%s" UTC)
    math(EXPR progress_elapsed "${projection_started} - ${progress_started}")
    set(projection_eta "")
    if(projection_index GREATER 1)
        math(EXPR projection_completed "${projection_index} - 1")
        math(EXPR projection_remaining "${report_count} - ${projection_index}")
        math(EXPR projection_left
            "${projection_elapsed_total} * ${projection_remaining} / ${projection_completed}")
        set(projection_eta ", ~${projection_left}.0 s left")
    endif()
    message("progress: report_schema ${projection_index}/${report_count} projection validation (${progress_elapsed}.0 s elapsed${projection_eta})")
    execute_process(
        COMMAND "${PYTHON}" "${VALIDATOR}" "${SCHEMA}" --projection
            "${full_report}" "${moves_report}"
        RESULT_VARIABLE projection_result)
    string(TIMESTAMP projection_finished "%s" UTC)
    math(EXPR projection_duration "${projection_finished} - ${projection_started}")
    math(EXPR projection_elapsed_total
        "${projection_elapsed_total} + ${projection_duration}")
    if(NOT projection_result EQUAL 0)
        message(FATAL_ERROR "report projection failed for ${full_report}")
    endif()
endforeach()
]=])

add_test(NAME report_schema COMMAND "${CMAKE_COMMAND}" -P "${CLI_REPORT_CHECK_SCRIPT}")
set_tests_properties(report_schema PROPERTIES
    LABELS engine
    FIXTURES_REQUIRED "${CLI_REPORT_FIXTURES}")

add_test(NAME cli_rerun_identity COMMAND cli_identity_test)
set_tests_properties(cli_rerun_identity PROPERTIES LABELS engine)
