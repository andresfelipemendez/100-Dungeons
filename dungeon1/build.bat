@echo off
setlocal enabledelayedexpansion

:: Set up environment variables
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Define paths
set "COMPILER=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\cl.exe"
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"
set "OUTPUT_PATH=%PROJECT_ROOT%\build\Debug"
set "ASSETS_PATH=%PROJECT_ROOT%\assets"

:: Compile Core DLL
call "%PROJECT_ROOT%\build_core.bat" --skip-vcvars
call "%PROJECT_ROOT%\build_externals.bat" --skip-vcvars
call "%PROJECT_ROOT%\build_engine.bat" --skip-vcvars
:: Compile main.cpp as an executable
call "%COMPILER%" /Zi /std:c++17 /MD "%PROJECT_ROOT%\src\main.cpp" /Fe:"%OUTPUT_PATH%\AnitraEngine.exe"

:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of AnitraEngine.exe failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'AnitraEngine.exe Compilation succeeded.' -ForegroundColor Green"
)

set "GLFW_DLL_PATH=%PROJECT_ROOT%\lib\glfw\lib-vc2022\glfw3.dll"
:: Copy glfw.dll to the output directory
if exist "%GLFW_DLL_PATH%" (
    copy "%GLFW_DLL_PATH%" "%OUTPUT_PATH%"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to copy glfw.dll to the output directory' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'glfw.dll copied successfully to the output directory' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'glfw.dll not found in the specified path' -ForegroundColor Red"
    exit /b 1
)

:: Create symbolic link to assets if it doesn't exist
if not exist  "%OUTPUT_PATH%\assets" (
mklink /D "%OUTPUT_PATH%\assets" "%ASSETS_PATH%"
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Failed to create symbolic link to assets. Please run as Administrator.' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'Symbolic link to assets created successfully.' -ForegroundColor Green"
)
)else (
    powershell -Command "Write-Host 'Symbolic link to assets already exists.' -ForegroundColor Cyan"
)


endlocal
