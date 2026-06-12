#!/bin/sh
# THE one and only build script: bootstraps the platform exe via cmake
# (which kaji lives inside; the first run also builds SDL3 from source, a
# few minutes). Everything else -- the game .so, shaders, ship bundles,
# even rebuilding the host exe -- is a kaji target:
#     ./build-linux/dungeon --build <target>     (see kaji.cfg)
# The exe forges its own first game .so on launch.
set -e
cd "$(dirname "$0")"

if [ "$(uname)" = "Darwin" ]; then
    # macOS: clang (gcc here is clang anyway), separate build dir -- object
    # formats and cmake caches must not collide with linux builds
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
echo "bootstrap complete. run:  ./$BUILD_DIR/dungeon"
