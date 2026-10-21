add_library(chesswhy_analysis STATIC
    rules.cpp
    analyse.cpp
)
target_link_libraries(chesswhy_analysis PUBLIC chesswhy_headers chesswhy_engine chesswhy_tactics)
target_link_libraries(chesswhy_analysis PRIVATE chesswhy_positional)
target_link_libraries(chesswhy_analysis PRIVATE chesswhy_warnings)
