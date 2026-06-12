@echo off
rem library must be strict c89; test harness uses utest.h + windows.h (default std)
gcc -std=c89 -pedantic -Wall -Werror -fsyntax-only seni.c arena.c || exit /b 1
gcc test.c -o test.exe || exit /b 1
.\test.exe || exit /b 1
gcc test_e2e.c -o test_e2e.exe || exit /b 1
.\test_e2e.exe || exit /b 1
gcc test_fuzz.c -o test_fuzz.exe || exit /b 1
.\test_fuzz.exe || exit /b 1
gcc test_stress.c -o test_stress.exe || exit /b 1
.\test_stress.exe
