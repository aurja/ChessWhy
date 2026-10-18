add_library(chesswhy_pgntool STATIC pgntool.cpp)
target_include_directories(chesswhy_pgntool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chesswhy_pgntool
    PUBLIC chesswhy_headers chesswhy_input
    PRIVATE chesswhy_warnings
)

add_executable(chesswhy-pgn main.cpp)
target_link_libraries(chesswhy-pgn PRIVATE chesswhy_pgntool chesswhy_warnings)
set_target_properties(chesswhy-pgn PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
