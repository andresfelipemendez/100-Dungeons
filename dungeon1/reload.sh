#!/bin/sh
# Manual fallback for the Linux iteration loop -- the running exe normally
# rebuilds automatically via kansi (see kansi.linux.cfg). Kept in sync with
# that config by hand. Note: the game .so is linked WITHOUT libSDL3; its SDL
# symbols resolve at dlopen time against the library the exe already loaded.
set -e
cd "$(dirname "$0")"

GLSLC=${GLSLC:-glslc}
SENI_DIR=../vendor/seni

mkdir -p build-linux

# Snapshot the hot-state header: the compiler #includes it and the assembler
# .incbins it at different moments within one compile; both must read the
# same bytes even if src/game_state.h is saved mid-compile.
cp -f src/game_state.h build-linux/game_state.h

$GLSLC src/shaders/model.vert -o build-linux/model.vert.spv
$GLSLC src/shaders/model.frag -o build-linux/model.frag.spv
$GLSLC ../engine/src/engine/render/shaders/ui.vert -o build-linux/ui.vert.spv
$GLSLC ../engine/src/engine/render/shaders/ui.frag -o build-linux/ui.frag.spv

# Vendor implementations: compiled once, linked into every rebuild.
if [ ! -f build-linux/vendor_impl.o ]; then
    gcc -c -O1 -fPIC ../engine/src/engine/vendor_impl.c \
        -I../vendor/cgltf -I../vendor/stb -I../vendor/clay \
        -o build-linux/vendor_impl.o
fi

# ui.c: clay-speaking engine file, separate C99 object (kansi rebuilds it on
# change; here only when missing -- delete build-linux/ui.o to force).
if [ ! -f build-linux/ui.o ]; then
    gcc -c -g0 -O0 -pipe -std=c99 -fPIC ../engine/src/engine/ui/ui.c \
        -I../engine/src -I../vendor/clay -I../vendor/stb \
        -I../vendor/SDL3-mingw/x86_64-w64-mingw32/include \
        -o build-linux/ui.o
fi

# Precompiled header (flags must match the .so compile exactly).
if [ ! -f build-linux/pch.h.gch ]; then
    cp -f src/pch.h build-linux/pch.h
    gcc -x c-header -g0 -O0 -pipe -std=c99 -fPIC build-linux/pch.h \
        -Ibuild-linux -Isrc -I../engine/src \
        -I../vendor/cgltf -I../vendor/stb -I../vendor/clay \
        -I../vendor/SDL3-mingw/x86_64-w64-mingw32/include \
        -I"$SENI_DIR" \
        -o build-linux/pch.h.gch
fi

gcc -shared -g0 -O0 -pipe -Wall -Wextra -std=c99 -fPIC -include build-linux/pch.h \
    src/game_unity.c \
    build-linux/vendor_impl.o \
    build-linux/ui.o \
    -Ibuild-linux \
    -Isrc \
    -I../engine/src \
    -I../vendor/cgltf \
    -I../vendor/stb \
    -I../vendor/clay \
    -I../vendor/SDL3-mingw/x86_64-w64-mingw32/include \
    -I"$SENI_DIR" \
    -lm \
    -o build-linux/game_tmp.so

# Build to a temp name, then rename: atomic, so the exe's watcher never
# observes a half-written .so.
mv -f build-linux/game_tmp.so build-linux/game_new.so
echo "reload: build-linux/game_new.so ready"
