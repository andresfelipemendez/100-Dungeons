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
set "EXTERNALS_SRC_PATH=%PROJECT_ROOT%\src\externals"
set "CORE_INCLUDE_PATH=%PROJECT_ROOT%\src\core"
set "OUTPUT_PATH=%PROJECT_ROOT%\build\Debug"

:: Set paths for libraries and includes
set "GLAD_INCLUDE_DIR=%PROJECT_ROOT%\lib\glad"
set "GLAD_LIB_PATH=%PROJECT_ROOT%\lib\glad\glad.lib"
set "GLFW_INCLUDE_DIR=%PROJECT_ROOT%\lib\glfw\include"
set "GLFW_LIB_PATH=%PROJECT_ROOT%\lib\glfw\lib\glfw3dll.lib"

set "IMGUI_INCLUDE_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9"
set "IMGUI_INCLUDE_BACKENDS_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9\backends"
set "IMGUI_LIB_PATH=%PROJECT_ROOT%\lib\imgui-1.90.9\imgui.lib"

:: Initialize a variable to hold all source files without leading space
set "SOURCE_FILES="

:: Iterate over each .cpp file in EXTERNALS_SRC_PATH and append it to SOURCE_FILES without a leading space
for %%f in ("%EXTERNALS_SRC_PATH%\*.cpp") do (
    if "!SOURCE_FILES!"=="" (
        set "SOURCE_FILES=%%f"
    ) else (
        set "SOURCE_FILES=!SOURCE_FILES! %%f"
    )
)

:: Check if SOURCE_FILES is empty
if "!SOURCE_FILES!"=="" (
    echo Error: No source files found in %EXTERNALS_SRC_PATH%.
    exit /b 1
)

:: Compile the DLL with all source files, include paths, and static libraries
cl /LD /Zi /std:c++17 /MD /D_ITERATOR_DEBUG_LEVEL=0 ^
    /I"%INCLUDE_PATH%" ^
    /I"%EXTERNALS_SRC_PATH%" ^
    /I"%CORE_INCLUDE_PATH%" ^
    /I"%GLAD_INCLUDE_DIR%"^
    /I"%IMGUI_INCLUDE_DIR%"^
    /I"%IMGUI_INCLUDE_BACKENDS_DIR%"^
    /I"%GLFW_INCLUDE_DIR%"^
    !SOURCE_FILES! /Fe:"%OUTPUT_PATH%\externals.dll"^
    /link^
    "%GLAD_LIB_PATH%"^
    "%GLFW_LIB_PATH%"^
    "%IMGUI_LIB_PATH%"^
    /out:"%OUTPUT_PATH%\externals.dll"

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    echo Compilation of externals.dll failed with error code %errorlevel%.
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Externals Compilation succeeded.' -ForegroundColor Green"
)

endlocal
