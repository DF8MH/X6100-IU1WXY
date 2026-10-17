#!/bin/bash
# The PC tests (tests/ from the dev repository). Only the test programs are built: "all"
# would also build LVGL and the radio control library for the PC, which do not build
# there (the LVGL fork's non-NEON blending and gpiod.h); the tests that need them are
# reported as not run.

cmake -B build_test/ -DENABLE_TESTING=YES -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit 1
targets=$(cmake --build build_test/ --target help | grep -oE '(test_[a-z0-9_]+|[a-z0-9_]+_test)$' | sort -u)
cmake --build build_test/ -j10 --target $targets -- -k > build_test/build.log 2>&1 || \
    echo "some tests did not build: see build_test/build.log"
cd build_test/ && ctest --output-on-failure
