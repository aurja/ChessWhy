add_library(chesswhy_cli STATIC cli.cpp)
target_link_libraries(chesswhy_cli
    PUBLIC
        chesswhy_headers
        chesswhy_input
        chesswhy_engine
        chesswhy_openings
        chesswhy_analysis
        chesswhy_report
        chesswhy_render
    PRIVATE
        chesswhy_warnings
        chesswhy_games
        chesswhy_paths)

add_executable(chesswhy main.cpp)
target_link_libraries(chesswhy PRIVATE chesswhy_cli chesswhy_warnings)
set_target_properties(chesswhy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
