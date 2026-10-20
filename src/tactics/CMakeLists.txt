# Tactical motif detectors (Planning/milestones/M2.md §4-§6) and tactic rules
# (Planning/milestones/PostM2_EM1.md §4.3, §4.4).
add_library(chesswhy_tactics STATIC
    see.cpp
    motifs.cpp
    tactic.cpp
)
target_link_libraries(chesswhy_tactics PUBLIC chesswhy_headers)
target_link_libraries(chesswhy_tactics PRIVATE chesswhy_warnings)
