#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p build
gcc -g -Wall -Wextra -std=c99 test.c ../dodai/dodai_posix.c -I../dodai -lpthread -o build/test.out
./build/test.out
