@echo off

REM Check for debug flag
IF "%DEBUG%"=="1" (
    set CFLAGS=/Wall /EHsc /W3 /Od /Zi
) ELSE (
    set CFLAGS=/Wall /EHsc /W3 /O2 /DNDEBUG
)

REM Ensure Visual Studio environment variables are set
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Compile C files into object files
echo Compiling C files...
cl %CFLAGS% /c toml.c

REM Create static library
echo Creating static library...
lib /OUT:toml.lib toml.obj

REM Create dynamic library
echo Creating dynamic library...
cl toml.obj /LD /link /OUT:toml.dll /IMPLIB:toml.lib

REM Create executables
echo Creating executables...
cl /Fetoml_json.exe toml_json.c toml.lib
cl /Fetoml_cat.exe toml_cat.c toml.lib
cl /Fetoml_sample.exe toml_sample.c toml.lib

echo Build complete.
pause
