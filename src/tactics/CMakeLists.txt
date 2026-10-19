# Tactical motif detectors (Planning/milestones/M2.md §4-§6). Sources owned by WP1.
# Temporary guard until WP1 lands its sources; removed at batch 1 integration.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/see.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/motifs.cpp")
    message(STATUS "chesswhy_tactics: sources not written yet (M2 WP1); target skipped")
    return()
endif()

add_library(chesswhy_tactics STATIC
    see.cpp
    motifs.cpp
)
target_link_libraries(chesswhy_tactics PUBLIC chesswhy_headers)
target_link_libraries(chesswhy_tactics PRIVATE chesswhy_warnings)
