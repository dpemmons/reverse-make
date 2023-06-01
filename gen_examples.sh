#!/bin/bash

set -e

make clean
make

./build/debug/reverse-make examples/libuv.build.log > examples/libuv.summary.txt
./build/debug/reverse-make examples/zlib.build.log > examples/zlib.summary.txt