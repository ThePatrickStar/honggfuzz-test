#!/bin/bash

docker run \
    -d --name hfuzzdev \
    -v /sn640/fuzzerlog/honggfuzz:/src/honggfuzz \
	--shm-size=2g \
	--cap-add SYS_NICE \
	--cap-add SYS_PTRACE \
	-e FUZZ_OUTSIDE_EXPERIMENT=1 \
	-e FORCE_LOCAL=1 \
	-e TRIAL_ID=1 \
	-e FUZZER=honggfuzz_latest \
	-e BENCHMARK=sqlite3_ossfuzz \
	-e FUZZ_TARGET=ossfuzz \
	-e DEBUG_BUILDER=1 \
	--entrypoint "/bin/bash" \
	-it gcr.io/fuzzbench/builders/honggfuzz_latest/sqlite3_ossfuzz
