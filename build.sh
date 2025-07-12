#!/bin/sh
set -xe
clang -fsanitize=address -g src/*.c -o fishc
# clang -O3 src/*.c -o fishc
