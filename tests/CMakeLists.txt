cmake_minimum_required(VERSION 3.23)

# add_executable(tests test.cpp)
# target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

add_compile_options(-fsanitize=address -fsanitize=undefined  -fno-omit-frame-pointer -fno-sanitize-recover)
add_link_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover -static-libasan -static-libubsan)


add_subdirectory(../src/qth ${CMAKE_BINARY_DIR}/src/qth)
add_executable(test_qth test_qth.cpp)
target_link_libraries(test_qth PRIVATE QTH Catch2::Catch2WithMain)
add_test(NAME test_qth COMMAND $<TARGET_FILE:test_qth> --colour-mode=ansi )

add_subdirectory(../src/ft8 ${CMAKE_BINARY_DIR}/src/ft8)
add_executable(test_ft8_qso test_ft8_qso.cpp)
target_link_libraries(test_ft8_qso PRIVATE FT8 QTH aether_x6100_control Catch2::Catch2WithMain)
add_test(NAME test_ft8_qso COMMAND $<TARGET_FILE:test_ft8_qso> --colour-mode=ansi )


add_subdirectory(../third-party/js8core ${CMAKE_BINARY_DIR}/third-party/js8core)
add_subdirectory(../src/js8 ${CMAKE_BINARY_DIR}/src/js8)
add_executable(test_js8 test_js8.cpp)
target_link_libraries(test_js8 PRIVATE JS8 JS8TestSignal js8core Catch2::Catch2WithMain)
add_test(NAME test_js8 COMMAND $<TARGET_FILE:test_js8> --colour-mode=ansi )

add_subdirectory(cfg)

add_subdirectory(../src/cluster ${CMAKE_BINARY_DIR}/src/cluster)
add_executable(test_cluster test_cluster.cpp)
target_compile_definitions(CLUSTER PRIVATE CLUSTER_CONF="test_cluster.conf")
target_compile_definitions(test_cluster PRIVATE CLUSTER_CONF="test_cluster.conf")
target_link_libraries(test_cluster PRIVATE CLUSTER Catch2::Catch2WithMain)
add_test(NAME test_cluster COMMAND $<TARGET_FILE:test_cluster> --colour-mode=ansi )

add_subdirectory(../src/field ${CMAKE_BINARY_DIR}/src/field)
add_executable(test_field test_field.cpp)
target_link_libraries(test_field PRIVATE FIELD Catch2::Catch2WithMain)
add_test(NAME test_field COMMAND $<TARGET_FILE:test_field> --colour-mode=ansi )

# liquid-dsp is not a host package: the UI harness deps have it
find_package(Threads REQUIRED)
# (tools/js8_ui_harness/fetch_deps.sh)
if(NOT TARGET liquid)
    find_library(LIQUID_LIB liquid HINTS ${CMAKE_SOURCE_DIR}/tools/js8_ui_harness/deps/lib)
    add_library(liquid UNKNOWN IMPORTED)
    set_target_properties(liquid PROPERTIES IMPORTED_LOCATION ${LIQUID_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/tools/js8_ui_harness/deps/include)
endif()
add_subdirectory(../third-party/wsprd ${CMAKE_BINARY_DIR}/third-party/wsprd)
add_subdirectory(../src/wspr ${CMAKE_BINARY_DIR}/src/wspr)
add_executable(test_wspr test_wspr.cpp)
target_link_libraries(test_wspr PRIVATE WSPR Catch2::Catch2WithMain)
add_test(NAME test_wspr COMMAND $<TARGET_FILE:test_wspr> --colour-mode=ansi )
add_subdirectory(../src/cw_decoder ${CMAKE_BINARY_DIR}/src/cw_decoder)
add_subdirectory(cw_decoder)
