@echo off

set C_COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe"
set COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang++.exe"

set FLAGS=-std=c++17 -fexceptions -Iinclude -Ilib/flecs -Ilib/glfw -Ilib/glfw/include -Ilib
set LINK_FLAGS=-lws2_32

set SOURCE_C=lib/flecs/flecs.c
set OBJECT_C=lib/flecs/flecs.o

set SOURCE=src\main.cpp
set OUTPUT=bin\game.exe

%C_COMPILER% -c %SOURCE_C% -o %OBJECT_C%

%COMPILER% %FLAGS% %SOURCE% %OBJECT_C% %LINK_FLAGS% -o %OUTPUT%

%OUTPUT%
