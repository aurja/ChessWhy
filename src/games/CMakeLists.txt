add_library(chesswhy_games STATIC games.cpp)
target_link_libraries(chesswhy_games PUBLIC chesswhy_headers PRIVATE
    chesswhy_input
    chesswhy_openings
    chesswhy_warnings
)

add_executable(chesswhy-games main.cpp)
target_link_libraries(chesswhy-games PRIVATE chesswhy_games chesswhy_warnings)
set_target_properties(chesswhy-games PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
