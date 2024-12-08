@echo off
echo [96mStarting build...[0m

call "%~dp0env_vars.bat"

if not exist build (
    mkdir build
    echo [93mCreated build directory[0m
)

pushd build
if not exist Debug (
    mkdir Debug
    echo [93mCreated Debug directory[0m
)
popd

echo [97mCompiling main...[0m

cl /std:c++17 /EHsc /I"%INCLUDE_PATH%" /Fe"%OUTPUT_PATH%\AnitraEngine.exe" "%PROJECT_ROOT%\src\main.cpp"
if %errorlevel% neq 0 (
    echo [91mCompilation of main failed[0m
    echo [91mError level: %errorlevel%[0m
    exit /b %errorlevel%
) else (
    echo [92mMain compilation succeeded.[0m
)
