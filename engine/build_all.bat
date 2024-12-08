@echo off
setlocal enabledelayedexpansion

:: Set up environment variables
call "%~dp0env_vars.bat" 

:: Compile Core DLL
call "%PROJECT_ROOT%\build_main.bat" 
call "%PROJECT_ROOT%\build_core.bat" 
call "%PROJECT_ROOT%\build_externals.bat" 
call "%PROJECT_ROOT%\build_engine.bat" 
call "%PROJECT_ROOT%\generate_compile_commands.bat" 
endlocal
