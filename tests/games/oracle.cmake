if(NOT EXISTS "${CPP_TOOL}" OR NOT EXISTS "${PYTHON}" OR NOT EXISTS "${PYTHON_TOOL}")
    message(FATAL_ERROR "games oracle executable or tool is missing")
endif()

set(output_prefix "${OUTPUT_PREFIX}")
if(output_prefix STREQUAL "")
    set(output_prefix "games-oracle")
endif()
set(cpp_output "${TEST_BINARY_DIR}/${output_prefix}-cpp.tsv")
set(python_output "${TEST_BINARY_DIR}/${output_prefix}-python.tsv")
string(TIMESTAMP progress_origin "%s" UTC)
macro(start_progress step detail)
    string(TIMESTAMP progress_step_started "%s" UTC)
    math(EXPR progress_elapsed "${progress_step_started} - ${progress_origin}")
    message("progress: games_oracle step ${step}/3 ${detail} started (${progress_elapsed}.0 s elapsed)")
endmacro()
macro(finish_progress step detail)
    string(TIMESTAMP progress_step_finished "%s" UTC)
    math(EXPR progress_step_elapsed "${progress_step_finished} - ${progress_step_started}")
    math(EXPR progress_elapsed "${progress_step_finished} - ${progress_origin}")
    message("progress: games_oracle step ${step}/3 ${detail} done in ${progress_step_elapsed}.0 s (${progress_elapsed}.0 s elapsed)")
endmacro()

if(DEFINED INPUT_PGN)
    if(NOT EXISTS "${INPUT_PGN}")
        message(FATAL_ERROR "games oracle input is missing: ${INPUT_PGN}")
    endif()
    start_progress(1 "C++ tables")
    execute_process(
        COMMAND "${CPP_TOOL}" --min-games "${MIN_GAMES}" --max-ply "${MAX_PLY}"
            --months "${MONTHS}"
        INPUT_FILE "${INPUT_PGN}"
        OUTPUT_FILE "${cpp_output}"
        ERROR_VARIABLE cpp_error
        RESULT_VARIABLE cpp_results
    )
    finish_progress(1 "C++ tables")
    if(NOT "${cpp_results}" STREQUAL "0")
        message(FATAL_ERROR "C++ games oracle command failed (${cpp_results}): ${cpp_error}")
    endif()

    start_progress(2 "Python tables")
    execute_process(
        COMMAND "${PYTHON}" "${PYTHON_TOOL}" --min-games "${MIN_GAMES}"
            --max-ply "${MAX_PLY}" --months "${MONTHS}" "${INPUT_PGN}"
        OUTPUT_FILE "${python_output}"
        ERROR_VARIABLE python_error
        RESULT_VARIABLE python_results
    )
    finish_progress(2 "Python tables")
    if(NOT "${python_results}" STREQUAL "0")
        message(FATAL_ERROR "Python games oracle command failed (${python_results}): ${python_error}")
    endif()
else()
    foreach(required_file IN ITEMS "${MONTH_1}" "${MONTH_2}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR "games oracle input is missing: ${required_file}")
        endif()
    endforeach()
    if(NOT EXISTS "${ZSTD}")
        message(FATAL_ERROR "games oracle decompressor is missing")
    endif()

    start_progress(1 "C++ tables")
    execute_process(
        COMMAND "${ZSTD}" -dc "${MONTH_1}" "${MONTH_2}"
        COMMAND "${CPP_TOOL}" --min-games 2 --months 2020-07,2022-08
        OUTPUT_FILE "${cpp_output}"
        ERROR_VARIABLE cpp_error
        RESULTS_VARIABLE cpp_results
    )
    finish_progress(1 "C++ tables")
    if(NOT "${cpp_results}" STREQUAL "0;0")
        message(FATAL_ERROR "C++ games oracle command failed (${cpp_results}): ${cpp_error}")
    endif()

    start_progress(2 "Python tables")
    execute_process(
        COMMAND "${ZSTD}" -dc "${MONTH_1}" "${MONTH_2}"
        COMMAND "${PYTHON}" "${PYTHON_TOOL}" --min-games 2 --months 2020-07,2022-08
        OUTPUT_FILE "${python_output}"
        ERROR_VARIABLE python_error
        RESULTS_VARIABLE python_results
    )
    finish_progress(2 "Python tables")
    if(NOT "${python_results}" STREQUAL "0;0")
        message(FATAL_ERROR "Python games oracle command failed (${python_results}): ${python_error}")
    endif()
endif()

start_progress(3 "table comparison")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${cpp_output}" "${python_output}"
    RESULT_VARIABLE compare_result
)
finish_progress(3 "table comparison")
file(REMOVE "${cpp_output}" "${python_output}")
if(NOT "${compare_result}" STREQUAL "0")
    message(FATAL_ERROR "C++ and Python games tables differ")
endif()
