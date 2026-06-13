#!/bin/sh
set -e
cd "$(dirname "$0")"
gcc test_build_manifest.c -o test_build_manifest.out \
    -I. -I.. -I../../lib/seni -lpthread
./test_build_manifest.out
