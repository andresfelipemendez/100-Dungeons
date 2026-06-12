@echo off
rem Full build: platform exe (cmake) + first game dll (reload.bat).
rem After this, iterate with reload.bat only -- the exe keeps running.
setlocal
cd /d "%~dp0"

cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug || exit /b 1
cmake --build build || exit /b 1

call "%~dp0reload.bat" || exit /b 1

echo.
echo build complete. run:  build\dungeon.exe   (from this directory)
