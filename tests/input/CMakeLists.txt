add_executable(input_unit_tests input_unit_tests.cpp)
target_link_libraries(input_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_input
    chesswhy_warnings
    chesswhy_paths
)
catch_discover_tests(input_unit_tests PROPERTIES LABELS unit)
