@echo off
setlocal

REM Set paths and filenames
set "SOURCE_FILE=printLog.cpp"
set "OUTPUT_DIR=build"
set "LIB_NAME=printLog.lib"

REM Ensure output directory exists
if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
)

REM Compile the source file into an object file using clang++
clang++ -c -std=c++17 -o "printLog.o" "%SOURCE_FILE%"
if %errorlevel% neq 0 (
    echo Compilation failed.
    exit /b %errorlevel%
)

REM Create the static library using llvm-ar
llvm-ar rcs "%LIB_NAME%" "printLog.o"
if %errorlevel% neq 0 (
    echo Library creation failed.
    exit /b %errorlevel%
)

echo Static library %LIB_NAME% created successfully in %OUTPUT_DIR%.
endlocal
