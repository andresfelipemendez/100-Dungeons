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
cl.exe /nologo /LD /std:c++17 /EHsc /MD /Zi ^
    /I. ^
    /I"%PROJECT_ROOT%" ^
    /I"%PROJECT_ROOT%\src" ^
    !SOURCE_FILES! ^
    /link ^
    /LIBPATH:"%GLAD_LIB_DIR%" /LIBPATH:"%GLFW_LIB_DIR%" /LIBPATH:"%IMGUI_LIB_DIR%" ^
    glad.lib glfw3dll.lib imgui.lib ^
     /OUT:"%OUTPUT_PATH%\core.dll" ^

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of core.dll failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Core Compilation succeeded.' -ForegroundColor Green"
)

endlocal
