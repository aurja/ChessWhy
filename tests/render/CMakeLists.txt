add_executable(render_unit_tests render_unit_tests.cpp)
target_link_libraries(render_unit_tests PRIVATE chesswhy_render Catch2::Catch2WithMain
    chesswhy_warnings chesswhy_paths)
catch_discover_tests(render_unit_tests TEST_PREFIX render_ PROPERTIES LABELS unit)
