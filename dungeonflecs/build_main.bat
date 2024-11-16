@echo off

call "%~dp0env_vars.bat" 

clang++ -std=c++17 -I"%INCLUDE_PATH%" "%PROJECT_ROOT%\src\main.cpp" -o "%OUTPUT_PATH%\AnitraEngine.exe"

if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of main failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'main Compilation succeeded.' -ForegroundColor Green"
)
