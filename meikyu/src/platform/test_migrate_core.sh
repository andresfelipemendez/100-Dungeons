#!/bin/sh
set -e
cd "$(dirname "$0")"
gcc test_migrate_core.c -o test_migrate_core.out \
    -I. -I.. -I../../lib/seni -lpthread
./test_migrate_core.out
