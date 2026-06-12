@echo off
rem Manual fallback for the iteration loop -- the running exe normally
rem rebuilds automatically via kansi (see kansi.cfg). Kept in sync with that
rem config by hand.
setlocal
cd /d "%~dp0"

set GLSLC=C:\VulkanSDK\1.4.341.1\Bin\glslc.exe
set SENI_DIR=C:\Users\andres\Development\seni

if not exist build mkdir build

rem Snapshot the hot-state header: the compiler #includes it and the
rem assembler .incbins it at different moments within one compile. Saving
rem src\game_state.h mid-compile would desync the dll's embedded layout from
rem the layout it was compiled against. Both reads go through this copy
rem (-Ibuild is searched before -Isrc, and SENI_EMBED_LAYOUT points here).
copy /y src\game_state.h build\game_state.h >nul || exit /b 1

%GLSLC% src\shaders\model.vert -o build\model.vert.spv || exit /b 1
%GLSLC% src\shaders\model.frag -o build\model.frag.spv || exit /b 1
%GLSLC% ..\engine\src\engine\render\shaders\ui.vert -o build\ui.vert.spv || exit /b 1
%GLSLC% ..\engine\src\engine\render\shaders\ui.frag -o build\ui.frag.spv || exit /b 1

rem Vendor implementations: compiled once, linked into every dll rebuild.
if not exist build\vendor_impl.o (
    gcc -c -O1 ..\engine\src\engine\vendor_impl.c ^
        -I..\vendor\cgltf -I..\vendor\stb -I..\vendor\clay ^
        -o build\vendor_impl.o || exit /b 1
)

rem ui.c: clay-speaking engine file, separate C99 object (kansi rebuilds it
rem on change; here only when missing -- delete build\ui.o to force).
if not exist build\ui.o (
    gcc -c -g0 -O0 -pipe -std=c99 ..\engine\src\engine\ui\ui.c ^
        -I..\engine\src -I..\vendor\clay -I..\vendor\stb ^
        -I..\vendor\SDL3-mingw\x86_64-w64-mingw32\include ^
        -o build\ui.o || exit /b 1
)

rem Precompiled header (flags must match the dll compile exactly).
if not exist build\pch.h.gch (
    copy /y src\pch.h build\pch.h >nul || exit /b 1
    gcc -x c-header -g0 -O0 -pipe -std=c99 build\pch.h ^
        -Ibuild -Isrc -I..\engine\src ^
        -I..\vendor\cgltf -I..\vendor\stb -I..\vendor\clay ^
        -I..\vendor\SDL3-mingw\x86_64-w64-mingw32\include ^
        -I%SENI_DIR% ^
        -o build\pch.h.gch || exit /b 1
)

gcc -shared -g0 -O0 -pipe -Wall -Wextra -std=c99 -include build/pch.h ^
    src\game_unity.c ^
    build\vendor_impl.o ^
    build\ui.o ^
    -Ibuild ^
    -Isrc ^
    -I..\engine\src ^
    -I..\vendor\cgltf ^
    -I..\vendor\stb ^
    -I..\vendor\clay ^
    -I..\vendor\SDL3-mingw\x86_64-w64-mingw32\include ^
    -I%SENI_DIR% ^
    -L..\vendor\SDL3-mingw\x86_64-w64-mingw32\lib ^
    -lSDL3 ^
    -o build\game_tmp.dll || exit /b 1

rem Build to a temp name, then rename: the rename is atomic enough that the
rem exe's file watcher never observes a half-written dll.
move /y build\game_tmp.dll build\game_new.dll >nul || exit /b 1
echo reload: build\game_new.dll ready
