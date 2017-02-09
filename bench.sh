#!/usr/bin/env bash

./compile.sh && time -p ./cmake-build-release/sigmod_2017 > output.txt && diff output.txt test-harness/small.result
