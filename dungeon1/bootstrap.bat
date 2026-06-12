@echo off
rem THE one and only build script: bootstraps the platform exe via cmake
rem (which kaji lives inside). Everything else -- the game dll, shaders,
rem ship bundles, even rebuilding the host exe -- is a kaji target:
rem     build\dungeon.exe --build <target>     (see kaji.cfg)
rem The exe forges its own first game dll on launch.
setlocal
cd /d "%~dp0"

cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug || exit /b 1
cmake --build build || exit /b 1

echo.
echo bootstrap complete. run:  build\dungeon.exe
