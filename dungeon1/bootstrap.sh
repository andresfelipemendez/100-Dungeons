#!/bin/sh
# THE one and only build script: bootstraps the platform exe via cmake
# (which kaji lives inside; the first run also builds SDL3 from source, a
# few minutes). Everything else -- the game .so, shaders, ship bundles,
# even rebuilding the host exe -- is a kaji target:
#     ./build-linux/dungeon --build <target>     (see kaji.cfg)
# The exe forges its own first game .so on launch.
set -e
cd "$(dirname "$0")"

cmake -S . -B build-linux -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel "$(nproc)"

echo
echo "bootstrap complete. run:  ./build-linux/dungeon"
