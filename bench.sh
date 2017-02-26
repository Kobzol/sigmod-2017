#!/usr/bin/env bash

time -p ./cmake-build-release/sigmod_2017 > output.txt && diff output.txt 50000_load/result #test-harness/large_small.result
