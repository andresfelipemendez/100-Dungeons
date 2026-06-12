#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p build
gcc -g -Wall -Wextra -std=c99 test.c platform_linux.c -o build/test.out
./build/test.out
