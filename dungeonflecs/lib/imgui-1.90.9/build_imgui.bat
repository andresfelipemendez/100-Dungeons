@echo off
setlocal

:: Initialize environment for Visual Studio x64 release mode
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Paths
set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"

set "GLFW_INCLUDE_DIR=%PROJECT_ROOT%\lib\glfw\include"

set "IMGUI_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9"
set "OUTPUT_PATH=%PROJECT_ROOT%\lib\imgui-1.90.9"

:: Add include directories
set "INCLUDE_FLAGS=/I%IMGUI_DIR% /I%IMGUI_DIR%\backends  /I%GLFW_INCLUDE_DIR%""

:: ImGui source files
set IMGUI_SRC_FILES=%IMGUI_DIR%\imgui.cpp %IMGUI_DIR%\imgui_demo.cpp %IMGUI_DIR%\imgui_draw.cpp %IMGUI_DIR%\imgui_tables.cpp %IMGUI_DIR%\imgui_widgets.cpp %IMGUI_DIR%\backends\imgui_impl_glfw.cpp %IMGUI_DIR%\backends\imgui_impl_opengl3.cpp

:: Compile source files into object files
for %%f in (%IMGUI_SRC_FILES%) do (
    echo Compiling %%f...
    cl /c "%%f" /Fo"%OUTPUT_PATH%\%%~nf.obj" /MDd /Od /EHsc %INCLUDE_FLAGS%
    if %errorlevel% neq 0 (
        echo Compilation failed for %%f with error code %errorlevel%.
        exit /b %errorlevel%
    )
)

:: Create imgui.lib
echo Creating imgui.lib...
lib /OUT:"%OUTPUT_PATH%\imgui.lib" "%OUTPUT_PATH%\imgui.obj" "%OUTPUT_PATH%\imgui_demo.obj" "%OUTPUT_PATH%\imgui_draw.obj" "%OUTPUT_PATH%\imgui_tables.obj" "%OUTPUT_PATH%\imgui_widgets.obj" "%OUTPUT_PATH%\imgui_impl_glfw.obj" "%OUTPUT_PATH%\imgui_impl_opengl3.obj"

:: Check if library creation was successful
if %errorlevel% neq 0 (
    echo Failed to create imgui.lib with error code %errorlevel%.
    exit /b %errorlevel%
) else (
    echo imgui.lib created successfully.
)

endlocal
