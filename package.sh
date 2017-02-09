#!/bin/bash

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$DIR"

/bin/tar --exclude='bench.sh' -czf  submission.tar.gz src/*.* CMakeLists.txt *.sh