add_library(chesswhy_engine STATIC
    engine.cpp
    info_line.cpp)
target_link_libraries(chesswhy_engine PUBLIC chesswhy_headers PRIVATE chesswhy_warnings)
