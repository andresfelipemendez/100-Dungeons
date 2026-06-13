#!/bin/sh
# Bootstraps the ENGINE exe via cmake, once per machine (the first run also
# builds SDL3 from source, a few minutes). Everything else -- game dlls,
# shaders, ship bundles, even rebuilding this exe -- is a kaji target the
# running engine generates per project:
#     ./build-linux/meikyu --path <project> --build <target>
set -e
cd "$(dirname "$0")"

if [ "$(uname)" = "Darwin" ]; then
    BUILD_DIR=build-mac
    CC=cc
    JOBS="$(sysctl -n hw.ncpu)"
else
    BUILD_DIR=build-linux
    CC=gcc
    JOBS="$(nproc)"
fi

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_C_COMPILER="$CC" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo
echo "bootstrap complete. run:  ./$BUILD_DIR/meikyu"
