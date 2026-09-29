# Headless host render of the app dialogs (FT8, JS8, SSTV, WeFax, NavTex, Cluster)
# with the real button bar, for look-and-feel comparison. Unresolved
# firmware symbols are auto-stubbed by gen_stubs.py (see README.md).
cmake_minimum_required(VERSION 3.23)
project(apps_ui_harness C CXX ASM)
get_filename_component(X ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(D ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(LV_DRAW_NEON=0)
add_compile_options(-g -O1 -fno-omit-frame-pointer -w)
include_directories(${X} ${X}/src ${X}/src/qth ${X}/third-party/rapidxml ${X}/third-party/utf8)
include_directories(SYSTEM ${D}/src ${D}/include /usr/include/glib-2.0 /usr/lib/x86_64-linux-gnu/glib-2.0/include /usr/include/libnm)
add_subdirectory(${D}/src/lvgl lvgl)
add_subdirectory(${X}/third-party/js8core js8core)
add_subdirectory(${X}/src/js8 js8)
file(READ ${X}/src/styles.c STYLES)
string(REPLACE "A:/dev/shm/" "A:${X}/" STYLES "${STYLES}")
file(WRITE ${CMAKE_BINARY_DIR}/styles_host.c "${STYLES}")
file(GLOB FONTS ${X}/src/fonts/*.c)
add_library(apps OBJECT
    ${X}/src/dialog.c ${X}/src/buttons.cpp ${X}/src/textarea_window.c ${X}/src/kbd_rollover.c
    ${X}/src/qth/qth.c ${X}/src/scheduler.cpp
    ${X}/src/widgets/lv_waterfall.c ${X}/src/widgets/lv_finder.c
    ${X}/src/dialog_js8.c
    ${X}/src/dialog_sstv.c ${X}/src/sstv/decoder.c ${X}/src/sstv/encoder.c
    ${X}/src/dialog_wefax.c ${X}/src/wefax/decoder.c
    ${X}/src/dialog_navtex.c ${X}/src/navtex/decoder.cpp ${X}/src/navtex/navtex_rx.cpp ${X}/src/navtex/fftfilt.cxx
    ${X}/src/dialog_ft8.c ${X}/src/ft8/table_view.c ${X}/src/ft8/utils.c ${X}/src/ft8/qso.cpp
    ${X}/src/dialog_settings.cpp
    ${X}/src/dialog_cluster.c ${X}/src/cluster/spots.c ${X}/src/cluster/client.c ${X}/src/freq_list.c ${X}/src/dialog_modes.c
    ${X}/src/dialog_msg_voice.c ${X}/src/dialog_msg_cw.c ${X}/src/cw_encoder.c
    ${X}/src/info.c ${X}/src/meter.c ${X}/src/clock.c ${X}/src/tx_info.c
    ${X}/src/cfg/settings_manager.cpp ${X}/src/cfg/pending_writes.cpp ${X}/src/cfg/storage_policy.cpp
    ${X}/src/cfg/subject.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cfg_host.cpp
    ${CMAKE_BINARY_DIR}/styles_host.c ${FONTS})
target_link_libraries(apps PRIVATE lvgl)
set_source_files_properties(${X}/src/info.c PROPERTIES COMPILE_DEFINITIONS "BTCAT_CLIENTS_FILE=\"btcat.clients\"")
set_source_files_properties(${X}/src/scheduler.cpp PROPERTIES COMPILE_OPTIONS "-include;cstdlib")
add_executable(apps_ui_harness main.c stubs.c $<TARGET_OBJECTS:apps> ${CMAKE_BINARY_DIR}/stubs_auto.S)
target_link_options(apps_ui_harness PRIVATE -Wl,--no-demangle)
target_link_libraries(apps_ui_harness PRIVATE lvgl JS8 js8core ${D}/lib/libliquid.a fftw3f png sndfile m pthread)
