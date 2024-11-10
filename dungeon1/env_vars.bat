@echo off

set "PROJECT_ROOT=C:\Users\andres\development\100-Dungeons\dungeon1"

set "OUTPUT_PATH=%PROJECT_ROOT%\build\Debug"
set "ASSETS_PATH=%PROJECT_ROOT%\assets"
set "BUILD_ENGINE_BAT=%PROJECT_ROOT%\build_engine.bat"
set "BUILD_EXTERNALS_BAT=%PROJECT_ROOT%\build_externals.bat"

set "INCLUDE_PATH=%PROJECT_ROOT%\include"
set "EXTERNALS_SRC_PATH=%PROJECT_ROOT%\src\externals"
set "EXTERNAL_INCLUDE_PATH=%PROJECT_ROOT%\src\externals"
set "CORE_INCLUDE_PATH=%PROJECT_ROOT%\src\core"
set "CORE_SRC_PATH=%PROJECT_ROOT%\src\core"
set "ENGINE_INCLUDE_PATH=%PROJECT_ROOT%\src\engine"
set "ENGINE_SRC_PATH=%PROJECT_ROOT%\src\engine"

:: Set paths for libraries and includes
set "FASTGLTF_INCLUDE_DIR=%PROJECT_ROOT%\lib\fastgltf\include"
set "FASTGLTF_LIB_PATH=%PROJECT_ROOT%\lib\fastgltf\lib\fastgltf.lib"
set "FASTGLTF_LIB_DIR=%PROJECT_ROOT%\lib\fastgltf\lib"

set "TOML_INCLUDE_DIR=%PROJECT_ROOT%\lib\toml"
set "TOML_LIB_PATH=%PROJECT_ROOT%\lib\toml\toml.lib"
set "TOML_LIB_DIR=%PROJECT_ROOT%\lib\toml"

set "GLAD_INCLUDE_DIR=%PROJECT_ROOT%\lib\glad"
set "GLAD_LIB_PATH=%PROJECT_ROOT%\lib\glad\glad.lib"
set "GLAD_LIB_DIR=%PROJECT_ROOT%\lib\glad"

set "GLFW_INCLUDE_DIR=%PROJECT_ROOT%\lib\glfw\include"
set "GLFW_LIB_PATH=%PROJECT_ROOT%\lib\glfw\lib\glfw3dll.lib"
set "GLFW_LIB_DIR=%PROJECT_ROOT%\lib\glfw\lib"

set "IMGUI_INCLUDE_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9"
set "IMGUI_INCLUDE_BACKENDS_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9\backends"
set "IMGUI_LIB_PATH=%PROJECT_ROOT%\lib\imgui-1.90.9\imgui.lib"
set "IMGUI_LIB_DIR=%PROJECT_ROOT%\lib\imgui-1.90.9"
