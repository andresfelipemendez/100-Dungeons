@echo off
setlocal enabledelayedexpansion

call "%~dp0env_vars.bat" 

set "SOURCE_FILES="

for %%f in ("%ENGINE_SRC_PATH%\*.cpp") do (
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
clang "%PROJECT_ROOT%\src\generator\componentserializer.c" -o "%OUTPUT_PATH%\componentserializer.exe"

"%OUTPUT_PATH%\componentserializer.exe"

:: Compile the DLL with clang++, using all source files, include paths, and libraries
clang++ -shared -std=c++17 -g -D_ITERATOR_DEBUG_LEVEL=0 -D_MT -D_DLL -DIMGUI_DEFINE_MATH_OPERATORS^
    -I"%INCLUDE_PATH%" -I"%CORE_INCLUDE_PATH%" -I"%EXTERNAL_INCLUDE_PATH%" -I"%FASTGLTF_INCLUDE_DIR%" -I"%GLM_INCLUDE_DIR%"^
    -I"%GLAD_INCLUDE_DIR%" -I"%IMGUI_INCLUDE_DIR%" -I"%IMGUI_INCLUDE_BACKENDS_DIR%" -I"%TOML_INCLUDE_DIR%" ^
    -I"%GLFW_INCLUDE_DIR%" ^
    !SOURCE_FILES! -o "%OUTPUT_PATH%\engine.dll" ^
    -L"%FASTGLTF_LIB_DIR%" -L"%TOML_LIB_DIR%" -L"%GLAD_LIB_DIR%" -L"%GLFW_LIB_DIR%" -L"%IMGUI_LIB_DIR%" ^
    -lfastgltf -ltoml -lglad -lglfw3dll -limgui ^
    -fuse-ld=lld -Wl,/machine:x64

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation failed with error code %errorlevel%.' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Engine Compilation succeeded.' -ForegroundColor Green"
)

endlocal
