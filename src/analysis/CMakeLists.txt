add_library(chesswhy_analysis STATIC
    rules.cpp
    analyse.cpp
)
target_link_libraries(chesswhy_analysis PUBLIC chesswhy_headers chesswhy_engine)
target_link_libraries(chesswhy_analysis PRIVATE chesswhy_warnings)
