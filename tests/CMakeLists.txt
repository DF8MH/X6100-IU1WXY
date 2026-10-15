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

add_executable(test_tx_level test_tx_level.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/tx_level.c)
target_include_directories(test_tx_level PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_tx_level PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_tx_level COMMAND $<TARGET_FILE:test_tx_level> --colour-mode=ansi )

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

find_package(PNG REQUIRED)
add_executable(test_wefax test_wefax.c ../src/wefax/decoder.c ../src/wefax/encoder.c ../src/sstv/encoder.c)
target_include_directories(test_wefax PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_wefax PRIVATE PNG::PNG m)
add_test(NAME test_wefax COMMAND $<TARGET_FILE:test_wefax>)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
add_executable(test_sstv test_sstv.c ../src/sstv/decoder.c ../src/sstv/encoder.c ../src/sstv/vis.c
    ../src/sstv/linesync.c ../src/sstv/preview.c)
target_include_directories(test_sstv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_sstv PRIVATE PNG::PNG PkgConfig::SNDFILE Threads::Threads m)
add_test(NAME test_sstv COMMAND $<TARGET_FILE:test_sstv>)

add_executable(test_freq_strip test_freq_strip.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/freq_strip.c)
target_compile_definitions(test_freq_strip PRIVATE FREQ_STRIP_STEP_ONLY)
target_link_libraries(test_freq_strip PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_freq_strip COMMAND $<TARGET_FILE:test_freq_strip> --colour-mode=ansi )

add_executable(test_paddle_keyer test_paddle_keyer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/paddle_keyer.c)
target_compile_definitions(test_paddle_keyer PRIVATE PADDLE_IAMBIC_ONLY)
target_include_directories(test_paddle_keyer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_paddle_keyer PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_paddle_keyer COMMAND $<TARGET_FILE:test_paddle_keyer> --colour-mode=ansi )

add_executable(test_swr_guard test_swr_guard.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/swr_guard.c)
target_include_directories(test_swr_guard PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_swr_guard PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_swr_guard COMMAND $<TARGET_FILE:test_swr_guard> --colour-mode=ansi )

add_executable(test_dsc test_dsc.cpp ../src/navtex/dsc.c ../src/navtex/decoder.cpp ../src/navtex/navtex_rx.cpp
    ../src/navtex/fftfilt.cxx)
target_include_directories(test_dsc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_dsc PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_dsc COMMAND $<TARGET_FILE:test_dsc> --colour-mode=ansi )
