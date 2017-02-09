#!/usr/bin/env bash

./compile.sh && time -p ./build/sigmod_2017 < small.load > output.txt && diff output.txt test-harness/small.result
