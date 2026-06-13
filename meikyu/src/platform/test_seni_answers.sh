#!/bin/sh
set -e
# Pure-core tests for seni_answers: no dodai, no SDL, no window.
# -I.  -> platform/ (seni_answers.h)
# -I.. -> src/ (base/, abi/ include roots, matching the engine build)
# -I../../lib/seni -> seni.h, arena.h, utest.h
cd "$(dirname "$0")"
gcc test_seni_answers.c -o test_seni_answers.out \
    -I. -I.. -I../../lib/seni -lpthread
./test_seni_answers.out
