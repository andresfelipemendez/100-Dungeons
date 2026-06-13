@echo off
rem Bootstraps the ENGINE exe via cmake, once per machine. Everything else
rem -- game dlls, shaders, ship bundles, even rebuilding this exe -- is a
rem kaji target the running engine generates per project:
rem     build\meikyu.exe --path <project> --build <target>
setlocal
cd /d "%~dp0"

cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug || exit /b 1
cmake --build build || exit /b 1

echo.
echo bootstrap complete. run:  build\meikyu.exe
