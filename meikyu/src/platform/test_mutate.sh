#!/bin/sh
set -e
cd "$(dirname "$0")"
gcc test_mutate.c -o test_mutate.out -I. -I.. -I../../lib/seni
./test_mutate.out
