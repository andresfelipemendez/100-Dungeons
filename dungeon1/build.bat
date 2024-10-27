@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\cl.exe"

set FLAGS=/std:c++17 /EHsc /Iinclude /Ilib/flecs /Ilib/GLFW/include /Ilib

set LINK_FLAGS=ws2_32.lib kernel32.lib user32.lib gdi32.lib shell32.lib winmm.lib advapi32.lib lib\glfw\lib-vc2022\glfw3.lib ucrt.lib vcruntime.lib msvcrt.lib /NODEFAULTLIB:libcmt

set SOURCE_C=lib\flecs\flecs.c
set OBJECT_C=lib\flecs\flecs.obj

set SOURCE=src\main.cpp
set OUTPUT=bin\game.exe

%COMPILER% /c %SOURCE_C% /Fo%OBJECT_C%

%COMPILER% %FLAGS% %SOURCE% %OBJECT_C% /link %LINK_FLAGS% /OUT:%OUTPUT%
