@echo off

:: Variables
set DOWNLOAD_URL=https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip
set DOWNLOAD_DIR=%TEMP%\ninja
set INSTALL_DIR="C:\Program Files\Ninja"

:: Create a temporary directory for the download
mkdir %DOWNLOAD_DIR%

:: Download ninja-win.zip to the temporary directory
echo Downloading Ninja from %DOWNLOAD_URL%
powershell -Command "Invoke-WebRequest -Uri %DOWNLOAD_URL% -OutFile %DOWNLOAD_DIR%\ninja-win.zip"

:: Unzip the downloaded file
echo Extracting Ninja...
powershell -Command "Expand-Archive -Path %DOWNLOAD_DIR%\ninja-win.zip -DestinationPath %DOWNLOAD_DIR% -Force"

:: Create the installation directory
if not exist %INSTALL_DIR% (
    mkdir %INSTALL_DIR%
)

:: Copy Ninja executable to Program Files
echo Installing Ninja to %INSTALL_DIR%
copy /Y %DOWNLOAD_DIR%\ninja.exe %INSTALL_DIR%

:: Clean up the temporary download directory
rmdir /S /Q %DOWNLOAD_DIR%

:: Add Ninja to the system PATH by directly modifying the registry
powershell -Command "Set-ItemProperty -Path 'HKLM:\System\CurrentControlSet\Control\Session Manager\Environment' -Name Path -Value ([Environment]::GetEnvironmentVariable('Path', 'Machine') + ';C:\Program Files\Ninja')"

echo Ninja has been installed and added to the PATH.
echo Please restart any open command prompts to use Ninja.
