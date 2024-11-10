@echo off
setlocal enabledelayedexpansion

:: Set up environment variables
call "%~dp0env_vars.bat" 

:: Compile Core DLL
call "%PROJECT_ROOT%\build_main.bat" --skip-vcvars
call "%PROJECT_ROOT%\build_core.bat" --skip-vcvars
call "%PROJECT_ROOT%\build_externals.bat" --skip-vcvars
call "%PROJECT_ROOT%\build_engine.bat" --skip-vcvars
endlocal