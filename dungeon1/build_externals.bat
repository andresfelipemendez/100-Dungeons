@echo off
setlocal enabledelayedexpansion
call "%~dp0env_vars.bat" 

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

:: Compile the DLL with clang++, using all source files, include paths, and libraries
clang++ -shared -std=c++17 -g -D_ITERATOR_DEBUG_LEVEL=0 -D_MT -D_DLL ^
    -I"%INCLUDE_PATH%" -I"%ENGINE_INCLUDE_PATH%" -I"%EXTERNALS_SRC_PATH%" -I"%CORE_INCLUDE_PATH%" -I"%GLAD_INCLUDE_DIR%" -I"%FASTGLTF_INCLUDE_DIR%" -I"%GLM_INCLUDE_DIR%" ^
    -I"%IMGUI_INCLUDE_DIR%" -I"%IMGUI_INCLUDE_BACKENDS_DIR%" -I"%GLFW_INCLUDE_DIR%" ^
    !SOURCE_FILES! -o "%OUTPUT_PATH%\externals.dll" ^
    -L"%FASTGLTF_LIB_DIR%" -L"%GLAD_LIB_DIR%" -L"%GLFW_LIB_DIR%" -L"%IMGUI_LIB_DIR%" ^
    -lfastgltf -lglad -lglfw3dll -limgui -lucrt -lmsvcrt^
    -fuse-ld=lld -Wl,/machine:x64

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of externals.dll failed with error code %errorlevel%.' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Externals Compilation succeeded.' -ForegroundColor Green"
)

endlocal
