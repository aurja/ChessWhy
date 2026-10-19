# Tactical motif detectors (Planning/milestones/M2.md §4-§6).
add_library(chesswhy_tactics STATIC
    see.cpp
    motifs.cpp
)
target_link_libraries(chesswhy_tactics PUBLIC chesswhy_headers)
target_link_libraries(chesswhy_tactics PRIVATE chesswhy_warnings)
