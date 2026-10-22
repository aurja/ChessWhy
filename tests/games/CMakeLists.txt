add_executable(games_unit_tests games_unit_tests.cpp)
target_link_libraries(games_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_games
    chesswhy_util
    chesswhy_warnings
    chesswhy_paths
)
catch_discover_tests(games_unit_tests TEST_PREFIX "games_unit_tests::" PROPERTIES LABELS unit)

find_program(ZSTD_EXECUTABLE zstd REQUIRED)
add_test(
    NAME games_oracle
    COMMAND "${CMAKE_COMMAND}"
        "-DCPP_TOOL=$<TARGET_FILE:chesswhy-games>"
        "-DPYTHON=${CHESSWHY_PYTHON}"
        "-DPYTHON_TOOL=${PROJECT_SOURCE_DIR}/tools/count_games.py"
        "-DZSTD=${ZSTD_EXECUTABLE}"
        "-DMONTH_1=${CHESSWHY_ASSETS_DIR}/data/games/broadcast/lichess_db_broadcast_2020-07.pgn.zst"
        "-DMONTH_2=${CHESSWHY_ASSETS_DIR}/data/games/broadcast/lichess_db_broadcast_2022-08.pgn.zst"
        "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DOUTPUT_PREFIX=games-oracle"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/oracle.cmake"
)
set_tests_properties(games_oracle PROPERTIES LABELS games)

add_test(
    NAME games_oracle_edge
    COMMAND "${CMAKE_COMMAND}"
        "-DCPP_TOOL=$<TARGET_FILE:chesswhy-games>"
        "-DPYTHON=${CHESSWHY_PYTHON}"
        "-DPYTHON_TOOL=${PROJECT_SOURCE_DIR}/tools/count_games.py"
        "-DINPUT_PGN=${PROJECT_SOURCE_DIR}/tests/fixtures/games_edge.pgn"
        "-DMAX_PLY=20"
        "-DMIN_GAMES=1"
        "-DMONTHS=edge"
        "-DOUTPUT_PREFIX=games-oracle-edge"
        "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/oracle.cmake"
)
set_tests_properties(games_oracle_edge PROPERTIES LABELS unit)
