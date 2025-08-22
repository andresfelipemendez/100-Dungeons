@echo off
setlocal

:: Call vcvars64.bat to set up environment variables for MSVC
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Paths
set "COMPILER=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\cl.exe"
set "LIBRARY_MANAGER=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\lib.exe"
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"
set "GLAD_PATH=%PROJECT_ROOT%\lib\glad"
set "OUTPUT_PATH=%PROJECT_ROOT%\lib\glad"

:: Compile glad.c to glad.obj
echo Compiling glad.c to glad.obj...
"%COMPILER%" /c "%GLAD_PATH%\glad.c" /I"%GLAD_PATH%" /Fo"%OUTPUT_PATH%\glad.obj"

:: Check if compilation was successful
if %errorlevel% neq 0 (
    echo Compilation of glad.c failed with error code %errorlevel%.
    exit /b %errorlevel%
)

:: Link glad.obj to glad.lib
echo Creating glad.lib from glad.obj...
"%LIBRARY_MANAGER%" /OUT:"%OUTPUT_PATH%\glad.lib" "%OUTPUT_PATH%\glad.obj"

:: Check if library creation was successful
if %errorlevel% neq 0 (
    echo Creation of glad.lib failed with error code %errorlevel%.
    exit /b %errorlevel%
) else (
    echo glad.lib created successfully in %OUTPUT_PATH%.
)

endlocal
