@echo off
setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build"

if not exist "%BUILD_DIR%" (
    echo Build directory does not exist. Please run generate.bat first.
    exit /b 1
)

echo Building the engine target...
cmake --build "%BUILD_DIR%" --target engine --config Debug

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

powershell -Command "Write-Host 'Engine DLL build successfully.' -ForegroundColor Green"

endlocal
