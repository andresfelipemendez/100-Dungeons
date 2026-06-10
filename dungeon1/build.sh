#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR=build
BUILD_TYPE="${BUILD_TYPE:-Debug}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)"

exec "$BUILD_DIR/bin/game_engine" "$@"
