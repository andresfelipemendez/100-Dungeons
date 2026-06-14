#!/bin/sh
set -e
cd "$(dirname "$0")"
gcc test_tests_gen.c -o test_tests_gen.out \
    -I. -I.. -I../../lib/seni -lpthread
./test_tests_gen.out
