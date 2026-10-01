file(GLOB TEST_SOURCES "*.cpp")
add_executable(test_cw_decoder ${TEST_SOURCES})
target_link_libraries(test_cw_decoder PRIVATE CW_DECODER Catch2::Catch2WithMain)
add_test(NAME test_cw_decoder COMMAND $<TARGET_FILE:test_cw_decoder> --colour-mode=ansi)
