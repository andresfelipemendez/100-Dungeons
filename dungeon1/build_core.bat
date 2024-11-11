@echo off
setlocal enabledelayedexpansion
call "%~dp0env_vars.bat"

set "SOURCE_FILES="

for %%f in ("%CORE_SRC_PATH%\*.cpp") do (
    if "!SOURCE_FILES!"=="" (
        set "SOURCE_FILES=%%f"
    ) else (
        set "SOURCE_FILES=!SOURCE_FILES! %%f"
    )
)

if "!SOURCE_FILES!"=="" (
    echo Error: No source files found in %CORE_SRC_PATH%.
    exit /b 1
)

:: Compile the DLL with clang++, using the gathered source files, include paths, and libraries
clang++ -shared -std=c++17 -g ^
    -I"%INCLUDE_PATH%" -I"%CORE_INCLUDE_PATH%"  -I"%ENGINE_INCLUDE_PATH%" -I"%EXTERNAL_INCLUDE_PATH%"^
    -I"%GLAD_INCLUDE_DIR%" -I"%GLFW_INCLUDE_DIR%" ^
    -I"%IMGUI_INCLUDE_DIR%" -I"%IMGUI_INCLUDE_BACKENDS_DIR%" ^
    !SOURCE_FILES! -o "%OUTPUT_PATH%\core.dll" ^
    -L"%GLAD_LIB_DIR%" -L"%GLFW_LIB_DIR%" -L"%IMGUI_LIB_DIR%" ^
    -lglad -lglfw3dll -limgui

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of core.dll failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Core Compilation succeeded.' -ForegroundColor Green"
)

endlocal
