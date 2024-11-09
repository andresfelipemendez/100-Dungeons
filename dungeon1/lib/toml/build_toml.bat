@echo off
setlocal

:: Initialize environment for Visual Studio x64
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Set paths
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"
set "TOML_SRC_PATH=%PROJECT_ROOT%\lib\toml"
set "OUTPUT_PATH=%PROJECT_ROOT%\lib\toml"

:: Compile toml.c into an object file
echo Compiling toml.c...
cl /c "%TOML_SRC_PATH%\toml.c" /Fo"%OUTPUT_PATH%\toml.obj" /I"%TOML_SRC_PATH%" /MD
if %errorlevel% neq 0 (
    echo Compilation of toml.c failed with error code %errorlevel%.
    exit /b %errorlevel%
)

:: Create toml.lib from the compiled object file
echo Creating toml.lib...
lib /OUT:"%OUTPUT_PATH%\toml.lib" "%OUTPUT_PATH%\toml.obj"
if %errorlevel% neq 0 (
    echo Failed to create toml.lib with error code %errorlevel%.
    exit /b %errorlevel%
) else (
    echo toml.lib created successfully.
)

endlocal
