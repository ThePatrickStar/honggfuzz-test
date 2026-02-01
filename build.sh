#!/bin/bash

# RUN apt-get update -y && apt-get install -y libbfd-dev libunwind-dev

make clean
export DEBUG=1
export CC=gcc
export CXX=g++
export CCC=g++
CFLAGS="-funroll-loops" make
touch empty_lib.c
cc -c -o empty_lib.o empty_lib.c

# export CC=/src/honggfuzz/hfuzz_cc/hfuzz-clang
# export CXX=/src/honggfuzz/hfuzz_cc/hfuzz-clang++
# export FUZZER_LIB=/src/honggfuzz/empty_lib.o

# build target
# cd /src/sqlite3
# fuzzer_build

# cd /out
# ./honggfuzz --persistent --rlimit_rss 2048 --sanitizers_del_report=true --input /out/seeds --output /out/corpus/corpus --crashdir /out/corpus/crashes --dict /out/ossfuzz.dict -- /out/ossfuzz
