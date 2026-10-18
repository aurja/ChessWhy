add_executable(pgntool_unit_tests pgntool_unit_tests.cpp)
target_include_directories(pgntool_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/pgntool")
target_link_libraries(pgntool_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_pgntool
    chesswhy_warnings
    chesswhy_paths
)
catch_discover_tests(pgntool_unit_tests PROPERTIES LABELS unit)
