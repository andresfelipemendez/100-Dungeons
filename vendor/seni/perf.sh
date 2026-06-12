#!/bin/sh
# cache-miss profiling under cachegrind (works in WSL2 where hardware PMU
# counters are unavailable). prints overall miss rates and the per-function
# breakdown for the seni hot paths.
set -e
gcc -O2 -g bench_cache.c -o bench_cache.out -lpthread
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=build/cachegrind.out \
    ./bench_cache.out 2> build/cachegrind.log
grep -E "refs:|misses:|miss rate" build/cachegrind.log || cat build/cachegrind.log
echo "---- per-function (top entries mentioning seni hot paths) ----"
cg_annotate build/cachegrind.out 2>/dev/null | head -60
