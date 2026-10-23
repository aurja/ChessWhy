add_test(NAME checker_unit
    COMMAND "${CHESSWHY_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/unit_tests.py")
set_tests_properties(checker_unit PROPERTIES LABELS unit)
