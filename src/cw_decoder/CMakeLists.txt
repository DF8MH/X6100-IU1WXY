# CW decoder (ported from upstream x6100_gui): C++17, liquid-dsp for the FFT,
# NCO and resampler. Used by the app (src/cw.cpp) and by tests/cw_decoder.
add_library(CW_DECODER STATIC
    time_classifier.cpp
    morse_decoder.cpp
    cw_receiver.cpp
)

target_compile_features(CW_DECODER PUBLIC cxx_std_17)
target_include_directories(CW_DECODER PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CW_DECODER PUBLIC liquid)
