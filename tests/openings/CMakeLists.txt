add_executable(openings_unit_tests openings_unit_tests.cpp)
target_link_libraries(openings_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_openings
    chesswhy_util
    chesswhy_warnings
    chesswhy_paths
)
catch_discover_tests(openings_unit_tests
    TEST_SPEC "~\"opening dataset identity follows all five loaded file bytes\""
    PROPERTIES LABELS unit
)
catch_discover_tests(openings_unit_tests
    TEST_SPEC "\"opening dataset identity follows all five loaded file bytes\""
    PROPERTIES LABELS engine
)
