@echo off
setlocal enabledelayedexpansion

:: Set the project root directory (assuming the script is in the project root)
set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build"

:: Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo Build directory does not exist. Please run generate.bat first.
    exit /b 1
)

:: Build the externals project specifically
echo Building the externals library...
cmake --build "%BUILD_DIR%" --target externals --config Debug

:: Check if the build was successful
if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

powershell -Command "Write-Host 'Externals DLL built successfully.' -ForegroundColor Green"

endlocal
