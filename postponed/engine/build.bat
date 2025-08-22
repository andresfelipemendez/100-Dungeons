@echo off
if not exist build.exe (
clang build.c -o build.exe
)
build.exe --engine
