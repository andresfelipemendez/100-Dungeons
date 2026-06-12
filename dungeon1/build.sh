#!/bin/sh
# Full Linux build: platform exe (cmake; first run also builds SDL3 from
# source via FetchContent, a few minutes) + first game .so (reload.sh).
# After this, iterate with reload.sh only -- or just save, kansi rebuilds.
set -e
cd "$(dirname "$0")"

cmake -S . -B build-linux -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel "$(nproc)"

sh reload.sh

echo
echo "build complete. run:  ./build-linux/dungeon"
