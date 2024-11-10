@echo off

call "%~dp0env_vars.bat" 

clang++ "%PROJECT_ROOT%\src\main.cpp" -o "%OUTPUT_PATH%\AnitraEngine.exe" -std=c++17 -g -I"%INCLUDE_PATH%"
