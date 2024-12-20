@echo off
setlocal enabledelayedexpansion


:: Set up environment variables
call "%~dp0env_vars.bat" 

:: Check if Ninja is installed
where ninja >nul 2>&1
if %errorlevel% neq 0 (
    echo Ninja not found. Running install_ninja.bat...
    call "%~dp0install_ninja.bat"
   
        echo close the terminal to see updated path and try again 
        exit /b.
    )
) else (
    echo Ninja is already installed.
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

:: Create symbolic link for build_engine.bat if it doesn't exist
if not exist "%OUTPUT_PATH%\build_engine.bat" (
    mklink "%OUTPUT_PATH%\build_engine.bat" "%BUILD_ENGINE_BAT%"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to build_engine.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to build_engine.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to build_engine.bat already exists.' -ForegroundColor Cyan"
)


if not exist "%OUTPUT_PATH%\build_editor.bat" (
    mklink "%OUTPUT_PATH%\build_editor.bat" "%PROJECT_ROOT%\build_editor.bat"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to build_editor.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to build_editor.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to build_editor.bat already exists.' -ForegroundColor Cyan"
)

:: Create symbolic link for build_engine.bat if it doesn't exist
if not exist "%OUTPUT_PATH%\build_externals.bat" (
    mklink "%OUTPUT_PATH%\build_externals.bat" "%BUILD_EXTERNALS_BAT%"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to build_externals.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to build_externals.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to build_externals.bat already exists.' -ForegroundColor Cyan"
)

:: Create symbolic link for build_engine.bat if it doesn't exist
if not exist "%OUTPUT_PATH%\build_core.bat" (
    mklink "%OUTPUT_PATH%\build_core.bat" "%PROJECT_ROOT%\build_core.bat"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to build_core.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to build_core.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to build_core.bat already exists.' -ForegroundColor Cyan"
)

:: Create symbolic link for build_engine.bat if it doesn't exist
if not exist "%OUTPUT_PATH%\build_component_serializer.bat" (
    mklink "%OUTPUT_PATH%\build_component_serializer.bat" "%PROJECT_ROOT%\build_component_serializer.bat"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to build_component_serializer.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to build_component_serializer.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to build_component_serializer.bat already exists.' -ForegroundColor Cyan"
)


echo %OUTPUT_PATH%
:: Create symbolic link for build_engine.bat if it doesn't exist
if not exist "%OUTPUT_PATH%\env_vars.bat" (
    mklink "%OUTPUT_PATH%\env_vars.bat" "%PROJECT_ROOT%\env_vars.bat"
    if %errorlevel% neq 0 (
        powershell -Command "Write-Host 'Failed to create symbolic link to env_vars.bat. Please run as Administrator.' -ForegroundColor Red"
        exit /b %errorlevel%
    ) else (
        powershell -Command "Write-Host 'Symbolic link to env_vars.bat created successfully.' -ForegroundColor Green"
    )
) else (
    powershell -Command "Write-Host 'Symbolic link to env_vars.bat already exists.' -ForegroundColor Cyan"
)

:: Compile Core DLL
call "%PROJECT_ROOT%\build_main.bat"
call "%PROJECT_ROOT%\build_core.bat"
call "%PROJECT_ROOT%\build_externals.bat"
call "%PROJECT_ROOT%\build_editor.bat"
call "%PROJECT_ROOT%\build_engine.bat"
call "%PROJECT_ROOT%\generate_compile_commands.bat"


:: Check if the compilation was successful
if %errorlevel% neq 0 (
    powershell -Command "Write-Host 'Compilation of AnitraEngine.exe failed' -ForegroundColor Red"
    exit /b %errorlevel%
) else (
    powershell -Command "Write-Host 'AnitraEngine.exe Compilation succeeded.' -ForegroundColor Green"
)
endlocal