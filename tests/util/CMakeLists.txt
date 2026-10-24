add_library(chesswhy_test_progress INTERFACE)
target_include_directories(chesswhy_test_progress INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(sha256_unit_tests sha256_unit_tests.cpp)
target_link_libraries(sha256_unit_tests PRIVATE
    Catch2::Catch2WithMain
    chesswhy_util
    chesswhy_warnings
)
catch_discover_tests(sha256_unit_tests PROPERTIES LABELS unit)
