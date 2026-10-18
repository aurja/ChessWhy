add_library(chesswhy_input STATIC
    fen.cpp
    move.cpp
    pgn.cpp
)
target_link_libraries(chesswhy_input PUBLIC chesswhy_headers PRIVATE chesswhy_warnings)
