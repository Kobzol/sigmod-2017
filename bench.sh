#!/usr/bin/env bash

time -p ./cmake-build-release/sigmod_2017 > output.txt && diff output.txt test-harness/small.result
