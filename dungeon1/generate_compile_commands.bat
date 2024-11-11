@echo off
setlocal

REM Run CMake with Ninja generator and Clang as the compiler
cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -S . -B build

REM Check if the cmake command was successful
if %errorlevel% neq 0 (
    echo CMake configuration failed.
    exit /b %errorlevel%
)

REM Copy compile_commands.json from build directory to project root, overwriting if it exists
set "SOURCE_FILE=build\compile_commands.json"
set "DEST_FILE=compile_commands.json"

if exist "%SOURCE_FILE%" (
    copy /y "%SOURCE_FILE%" "%DEST_FILE%"
    echo compile_commands.json copied to project root.
) else (
    echo compile_commands.json not found in build directory.
    exit /b 1
)

endlocal
