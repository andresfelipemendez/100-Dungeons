@echo off
REM Check for correct number of arguments
if "%~1"=="" (
    echo Usage: create_file_pair.bat [BaseFileName] [Path]
    exit /b 1
)

if "%~2"=="" (
    echo Usage: create_file_pair.bat [BaseFileName] [Path]
    exit /b 1
)

set BaseFileName=%~1
set TargetPath=%~2

REM Ensure the target directory exists
if not exist "%TargetPath%" (
    echo The specified path does not exist: "%TargetPath%"
    exit /b 1
)

REM Create the .h file
set HeaderFile=%TargetPath%\%BaseFileName%.h
if not exist "%HeaderFile%" (
    echo Creating %HeaderFile%
    echo #ifndef %BaseFileName:_H%_H >> "%HeaderFile%"
    echo #define %BaseFileName:_H%_H >> "%HeaderFile%"
    echo >> "%HeaderFile%"
    echo #endif >> "%HeaderFile%"
) else (
    echo %HeaderFile% already exists.
)

REM Create the .c file
set SourceFile=%TargetPath%\%BaseFileName%.c
if not exist "%SourceFile%" (
    echo Creating %SourceFile%
    echo #include "%BaseFileName%.h" >> "%SourceFile%"
) else (
    echo %SourceFile% already exists.
)

echo Files created or already exist: %HeaderFile%, %SourceFile%
exit /b 0
