add_executable(engine_unit_tests engine_unit_tests.cpp)
target_link_libraries(engine_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_engine
    chesswhy_warnings
    chesswhy_paths)
catch_discover_tests(engine_unit_tests TEST_PREFIX engine_ PROPERTIES LABELS unit)

add_executable(engine_engine_tests engine_engine_tests.cpp)
target_link_libraries(engine_engine_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_engine
    chesswhy_warnings
    chesswhy_paths)
catch_discover_tests(engine_engine_tests TEST_PREFIX engine_ PROPERTIES LABELS engine)
