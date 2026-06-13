#!/bin/sh
set -e
cd "$(dirname "$0")"
# library must be strict c89; test harness uses utest.h (default std)
gcc -std=c89 -pedantic -Wall -Werror -fsyntax-only seni.c arena.c
gcc test.c -o test.out
./test.out
# kaji.c (included by the migration-compiling tests) does a bare #include
# "dodai.h", so it needs -I../dodai; its other includes are relative.
gcc -I../dodai test_e2e.c -o test_e2e.out -lpthread
./test_e2e.out
# fuzz under the sanitizers; no mallocs of our own, leak detection just
# flags dlopen internals
gcc -fsanitize=address,undefined -g -I../dodai test_fuzz.c -o test_fuzz.out -lpthread
ASAN_OPTIONS=detect_leaks=0 ./test_fuzz.out
gcc -fsanitize=address,undefined -g -I../dodai test_stress.c -o test_stress.out -lpthread
ASAN_OPTIONS=detect_leaks=0 ./test_stress.out
