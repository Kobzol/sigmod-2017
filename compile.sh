#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$DIR"

mkdir -p build
cd build
if [ ! -f Makefile ]; then
   cmake -DCMAKE_BUILD_TYPE=Release -DREAL_RUN=ON ../
fi
make -j
