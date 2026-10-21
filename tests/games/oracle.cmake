foreach(required_file IN ITEMS "${MONTH_1}" "${MONTH_2}" "${PYTHON_TOOL}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "games oracle input is missing: ${required_file}")
    endif()
endforeach()
if(NOT EXISTS "${CPP_TOOL}" OR NOT EXISTS "${PYTHON}")
    message(FATAL_ERROR "games oracle executable is missing")
endif()

set(cpp_output "${TEST_BINARY_DIR}/games-oracle-cpp.tsv")
set(python_output "${TEST_BINARY_DIR}/games-oracle-python.tsv")
execute_process(
    COMMAND "${ZSTD}" -dc "${MONTH_1}" "${MONTH_2}"
    COMMAND "${CPP_TOOL}" --min-games 2 --months 2020-07,2022-08
    OUTPUT_FILE "${cpp_output}"
    ERROR_VARIABLE cpp_error
    RESULTS_VARIABLE cpp_results
)
if(NOT "${cpp_results}" STREQUAL "0;0")
    message(FATAL_ERROR "C++ games oracle command failed (${cpp_results}): ${cpp_error}")
endif()

execute_process(
    COMMAND "${ZSTD}" -dc "${MONTH_1}" "${MONTH_2}"
    COMMAND "${PYTHON}" "${PYTHON_TOOL}" --min-games 2 --months 2020-07,2022-08
    OUTPUT_FILE "${python_output}"
    ERROR_VARIABLE python_error
    RESULTS_VARIABLE python_results
)
if(NOT "${python_results}" STREQUAL "0;0")
    message(FATAL_ERROR "Python games oracle command failed (${python_results}): ${python_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${cpp_output}" "${python_output}"
    RESULT_VARIABLE compare_result
)
file(REMOVE "${cpp_output}" "${python_output}")
if(NOT "${compare_result}" STREQUAL "0")
    message(FATAL_ERROR "C++ and Python games tables differ")
endif()
