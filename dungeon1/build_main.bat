@echo off
call "%~dp0env_vars.bat"
if not exist build mkdir build
pushd build
if not exist Debug mkdir Debug
popd
cl /std:c++17 /EHsc /I"%INCLUDE_PATH%" /Fe"%OUTPUT_PATH%\AnitraEngine.exe" "%PROJECT_ROOT%\src\main.cpp"
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of main failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'main Compilation succeeded.' -ForegroundColor Green"
)