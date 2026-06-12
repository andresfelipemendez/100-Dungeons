@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
gcc -g -Wall -Wextra -std=c99 test.c platform_windows.c -o build\test.exe || exit /b 1
build\test.exe
