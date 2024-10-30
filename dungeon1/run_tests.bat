@echo off
setlocal enabledelayedexpansion
set "PROJECT_ROOT=%~dp0"
set BUILD_DIR=%cd%\build

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -B "%BUILD_DIR%" -S "%PROJECT_ROOT%"
cmake --build "%BUILD_DIR%" --config Debug
ctest --test-dir "%BUILD_DIR%" --output-on-failure --build-config Debug