#!/bin/sh
set -e
# library must be strict c89; test harness uses utest.h (default std)
gcc -std=c89 -pedantic -Wall -Werror -fsyntax-only seni.c arena.c platform_linux.c
gcc test.c -o test.out
./test.out
gcc test_e2e.c -o test_e2e.out
./test_e2e.out
# fuzz under the sanitizers; no mallocs of our own, leak detection just
# flags dlopen internals
gcc -fsanitize=address,undefined -g test_fuzz.c -o test_fuzz.out
ASAN_OPTIONS=detect_leaks=0 ./test_fuzz.out
gcc -fsanitize=address,undefined -g test_stress.c -o test_stress.out
ASAN_OPTIONS=detect_leaks=0 ./test_stress.out
