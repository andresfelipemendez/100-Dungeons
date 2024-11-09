@echo off
setlocal enabledelayedexpansion

:: Check for --skip-vcvars parameter to avoid calling vcvars64.bat
set "SKIP_VCVARS=false"
if "%1"=="--skip-vcvars" (
    set "SKIP_VCVARS=true"
)

:: Call vcvars64.bat to set up environment variables if not skipped
if "%SKIP_VCVARS%"=="false" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

:: Set paths
set "COMPILER=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\cl.exe"
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"
set "INCLUDE_PATH=%PROJECT_ROOT%\include"
set "CORE_SRC_PATH=%PROJECT_ROOT%\src\core"
set "ENGINE_SRC_PATH=%PROJECT_ROOT%\src\engine"
set "EXTERNALS_SRC_PATH=%PROJECT_ROOT%\src\externals"
set "OUTPUT_PATH=%PROJECT_ROOT%\build\Debug"

:: Initialize a variable to hold all source files without leading space
set "SOURCE_FILES="

:: Iterate over each .cpp file in CORE_SRC_PATH and append it to SOURCE_FILES without a leading space
for %%f in ("%CORE_SRC_PATH%\*.cpp") do (
    if "!SOURCE_FILES!"=="" (
        set "SOURCE_FILES=%%f"
    ) else (
        set "SOURCE_FILES=!SOURCE_FILES! %%f"
    )
)

:: Check if SOURCE_FILES is empty
if "!SOURCE_FILES!"=="" (
    echo Error: No source files found in %CORE_SRC_PATH%.
    exit /b 1
)

:: Compile the DLL with all source files, include paths, and static libraries
cl /LD /Zi /std:c++17 /MD /D_ITERATOR_DEBUG_LEVEL=0 ^
    /I"%INCLUDE_PATH%" /I"%CORE_SRC_PATH%" /I"%ENGINE_SRC_PATH%" /I"%EXTERNALS_SRC_PATH%"^
    !SOURCE_FILES! /Fe:"%OUTPUT_PATH%\core.dll"^
    /link ^
    /out:"%OUTPUT_PATH%\core.dll"

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host ' Compilation of core.dll failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Core Compilation succeeded.' -ForegroundColor Green"
)

endlocal
