@echo off
setlocal enabledelayedexpansion

:: Define paths
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"
set "BUILD_DEBUG=%PROJECT_ROOT%\build\Debug"
set "EXE_NAME=AnitraEngine.exe"

:: Change to the build\Debug directory
cd /d "%BUILD_DEBUG%"
call "%BUILD_DEBUG%\%EXE_NAME%" & set SAVEDRC=!ERRORLEVEL! & call;
exit /b %SAVEDRC%

endlocal
