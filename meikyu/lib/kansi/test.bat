@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
gcc -g -Wall -Wextra -std=c99 test.c ..\dodai\dodai_windows.c -I..\dodai -o build\test.exe || exit /b 1
build\test.exe
